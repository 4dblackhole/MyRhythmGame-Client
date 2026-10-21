#pragma once

#include "Editing/IEditorDocument.h"
#include "Automation/ScrollAutomation.h"
#include "Utility/IntervalIndex.h"

#include <algorithm>
#include <list>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>

// Editable source beats are exact rationals. Recompile the prefix sums and
// note timestamps only after an edit, never during drawing or hit testing.
class ChartEditor final : public IEditorDocument
{
  public:
    ChartEditor(PatternDocument pattern, EffectDocument effects = {}, RhythmDuration runtimeOffset = {});
    ChartEditor(const ChartEditor &);
    ChartEditor &operator=(const ChartEditor &);
    ChartEditor(ChartEditor &&) noexcept = default;
    ChartEditor &operator=(ChartEditor &&) noexcept = default;
    [[nodiscard]] const PatternDocument &Pattern() const override;
    const std::filesystem::path &SourcePath() const override { return pattern_.sourcePath; }
    [[nodiscard]] const EffectDocument &Effects() const override;
    [[nodiscard]] const MusicalTimeline &Timeline() const noexcept override
    {
        return timeline_;
    }
    [[nodiscard]] const std::vector<CompiledPatternNote> &Notes() const override;
    const std::vector<TimingDirective> &Timing() const override;
    std::vector<CompiledPatternNote> NotesInMeasures(std::int64_t begin, std::int64_t end) const override;
    std::vector<CompiledPatternNote> NotesInTimeRange(RhythmTime begin, RhythmTime end) const override;
    std::pair<RhythmTime, RhythmTime> NoteTimeRange() const override;
    std::uint64_t AudioSourceRevision() const noexcept override { return audioSourceRevision_; }
    double EffectValueAt(EffectCommandType, MusicalPosition, double fallback = 1) const override;
    double MinimumScrollMultiplier() const override
    {
        const auto minimum = noteSpeedIndex_.empty()
                                 ? minimumScrollMultiplier_
                                 : std::min(minimumScrollMultiplier_, noteSpeedIndex_.begin()->first);
        return minimum * scroll_.MinimumMultiplier();
    }
    long double ScrollDistance(RhythmTime target, RhythmTime current) const override
    { return scroll_.Distance(target, current); }
    [[nodiscard]] bool Dirty() const noexcept override
    {
        return dirty_;
    }
    [[nodiscard]] std::uint64_t Revision() const noexcept override
    {
        return revision_;
    }
    void Replace(PatternDocument pattern, EffectDocument effects) override;
    void ReplaceTiming(std::vector<TimingDirective> timing) override;
    void ReplaceEffects(EffectDocument effects) override;
    void AddNote(MusicalPosition position, int keyType, std::optional<MusicalPosition> end = {},
                 std::vector<std::string> extra = {}) override;
    std::optional<EditorNoteSelection> FindNote(std::size_t sourceOrder) const override;
    void UpdateNote(std::size_t sourceOrder, PatternNote head, std::optional<MusicalPosition> end) override;
    void DeleteNote(std::size_t sourceOrder) override;
    void SetMeasureLength(std::int64_t measure, Rational length) override;
    void Save() override;
    bool CanUndo() const noexcept override
    {
        return !undo_.empty();
    }
    bool CanRedo() const noexcept override
    {
        return !redo_.empty();
    }
    void Undo() override;
    void Redo() override;
    [[nodiscard]] static std::string WritePattern(const PatternDocument &pattern);
    [[nodiscard]] static std::string WriteEffects(const EffectDocument &effects);

  private:
    struct HistoryState
    {
        std::vector<PatternNote> notes;          // Only changed source events, never a full note snapshot.
        std::optional<PatternDocument> metadata; // Contains no notes/timing.
        std::optional<std::vector<TimingDirective>> timing;
        std::optional<EffectDocument> effects;
    };
    struct HistoryEdit
    {
        HistoryState before, after;
        std::uint64_t beforeId{}, afterId{};
    };
    using HistoryList = std::list<HistoryEdit>;
    using SavedHistoryPaths = std::vector<std::pair<std::filesystem::path *, std::filesystem::path>>;
    HistoryList PrepareHistory(HistoryState before, HistoryState after) const;
    HistoryList ReplacementHistory(const ChartEditor &) const;
    SavedHistoryPaths PrepareSavedHistoryPaths(const std::filesystem::path &);
    void CommitHistory(HistoryList &) noexcept;
    void TransferHistory(ChartEditor &) noexcept;
    void RestoreHistory(const HistoryState &, const HistoryState &, std::uint64_t);
    void RestoreNotes(const HistoryState &, const HistoryState &);
    void ApplyNotePatch(std::span<const std::size_t>, std::span<const CompiledPatternNote>);
    void ReplaceData(PatternDocument, EffectDocument);
    void PublishReplacement(ChartEditor &&);
    HistoryList undo_, redo_;
    std::uint64_t historyState_{}, savedHistoryState_{}, nextHistoryState_{1};
    void InitializeEventTrees();
    CompiledPatternNote CompileNote(PatternNote) const;
    void InvalidateNoteSnapshots() noexcept;
    void EraseNote(std::size_t) noexcept;
    std::map<std::size_t, std::size_t> BuildLongPartners(
        std::span<const std::size_t> excluded = {}, std::span<const CompiledPatternNote> replacements = {}) const;
    std::vector<CompiledPatternNote> IncludeLongNotePartners(std::map<std::size_t, const CompiledPatternNote *>) const;
    void EnsureLongIntervalIndices() const;
    using NoteKey = std::pair<MusicalPosition, std::size_t>;
    using TimeKey = std::pair<RhythmTime, std::size_t>;
    // Trees own all editable events. DTO vectors below are lazy, read-only projections.
    std::map<NoteKey, CompiledPatternNote> noteTree_;
    std::map<std::size_t, NoteKey> noteKeys_;
    std::map<TimeKey, NoteKey> timeIndex_;
    // Actual compiled speeds, keyed by stable ID for transactional insertion/removal.
    std::set<std::pair<double, std::size_t>> noteSpeedIndex_;
    std::map<std::size_t, std::size_t> longPartners_;
    std::map<NoteKey, std::size_t> longEndpoints_;
    mutable IntervalIndex<RhythmTime, std::size_t> longTimeIntervals_;
    mutable IntervalIndex<std::int64_t, std::size_t> longMeasureIntervals_;
    mutable std::uint64_t longIntervalRevision_{};
    std::multimap<MusicalPosition, TimingDirective> timingTree_;
    std::multimap<MusicalPosition, EffectCommand> effectTree_;
    std::multimap<MusicalPosition, HitSoundChange> soundChangeTree_;
    // Compiled-time lookup is independent of the owning musical-position tree.
    std::map<std::pair<EffectCommandType, RhythmTime>, const EffectCommand *> effectIndex_;
    mutable PatternDocument pattern_;
    mutable EffectDocument effects_;
    RhythmDuration runtimeOffset_{}; // Session option; never serialized as chart data.
    MusicalTimeline timeline_;
    ScrollAutomation scroll_;
    mutable std::vector<CompiledPatternNote> notes_;
    mutable bool patternNotesDirty_{true}, timingDirty_{true}, effectsDirty_{true}, notesDirty_{true};
    std::size_t nextNoteId_{};
    std::uint64_t audioSourceRevision_{1};
    double minimumScrollMultiplier_{1};
    bool dirty_{};
    std::uint64_t revision_{1};
};
