#pragma once

#include "Editing/IEditorDocument.h"
#include "Automation/ScrollAutomation.h"

#include <optional>
#include <map>
#include <string>

namespace finger_drum::chart
{
    // Editable source beats are exact rationals. Recompile the prefix sums and
    // note timestamps only after an edit, never during drawing or hit testing.
    class ChartEditor final : public IEditorDocument
    {
      public:
        ChartEditor(PatternDocument pattern, EffectDocument effects = {});
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
        std::vector<CompiledPatternNote> NotesInTimeRange(rhythm::RhythmTime begin, rhythm::RhythmTime end) const override;
        std::pair<rhythm::RhythmTime, rhythm::RhythmTime> NoteTimeRange() const override;
        std::uint64_t AudioSourceRevision() const noexcept override { return audioSourceRevision_; }
        double EffectValueAt(EffectCommandType, MusicalPosition, double fallback = 1) const override;
        double MinimumScrollMultiplier() const override { return minimumScrollMultiplier_ * scroll_.MinimumMultiplier(); }
        long double ScrollDistance(rhythm::RhythmTime target, rhythm::RhythmTime current) const override
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
        void AddNote(MusicalPosition position, int keyType, std::optional<MusicalPosition> end = {},
                     std::vector<std::string> extra = {}) override;
        void DeleteNote(std::size_t sourceOrder) override;
        void SetMeasureLength(std::int64_t measure, Rational length) override;
        void Save() override;
        [[nodiscard]] static std::string WritePattern(const PatternDocument &pattern);
        [[nodiscard]] static std::string WriteEffects(const EffectDocument &effects);

      private:
        void InitializeEventTrees();
        CompiledPatternNote CompileNote(PatternNote) const;
        void InvalidateNoteSnapshots() noexcept;
        void EraseNote(std::size_t) noexcept;
        std::map<std::size_t, std::size_t> BuildLongPartners(
            std::optional<std::size_t> excluded = {}, std::optional<std::size_t> excludedPartner = {}) const;
        std::vector<CompiledPatternNote> IncludeLongNotePartners(std::map<std::size_t, const CompiledPatternNote *>) const;
        using NoteKey = std::pair<MusicalPosition, std::size_t>;
        using TimeKey = std::pair<rhythm::RhythmTime, std::size_t>;
        // Trees own all editable events. DTO vectors below are lazy, read-only projections.
        std::map<NoteKey, CompiledPatternNote> noteTree_;
        std::map<std::size_t, NoteKey> noteKeys_;
        std::map<TimeKey, NoteKey> timeIndex_;
        std::map<std::size_t, std::size_t> longPartners_;
        std::map<NoteKey, std::size_t> longEndpoints_;
        std::multimap<MusicalPosition, TimingDirective> timingTree_;
        std::multimap<MusicalPosition, EffectCommand> effectTree_;
        std::multimap<MusicalPosition, HitSoundChange> soundChangeTree_;
        std::map<std::pair<EffectCommandType, MusicalPosition>, const EffectCommand *> effectIndex_;
        mutable PatternDocument pattern_;
        mutable EffectDocument effects_;
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
} // namespace finger_drum::chart
