#pragma once
#include "Timing/MusicalTimeline.h"
#include <optional>
#include <string>

namespace finger_drum::chart
{
    // Common editable score projection. A format adapter keeps its original
    // source data and owns validation/serialization; Save must not discard it.
    class IEditorDocument
    {
      public:
        virtual ~IEditorDocument() = default;
        virtual const PatternDocument &Pattern() const = 0;
        virtual const std::filesystem::path &SourcePath() const { return Pattern().sourcePath; }
        virtual const EffectDocument &Effects() const = 0;
        virtual const MusicalTimeline &Timeline() const noexcept = 0;
        virtual const std::vector<CompiledPatternNote> &Notes() const = 0;
        // Adapters may retain the full projection; indexed documents override these queries.
        virtual std::vector<CompiledPatternNote> NotesInMeasures(std::int64_t, std::int64_t) const { return Notes(); }
        virtual std::vector<CompiledPatternNote> NotesInTimeRange(rhythm::RhythmTime, rhythm::RhythmTime) const { return Notes(); }
        virtual const std::vector<TimingDirective> &Timing() const { return Pattern().timing; }
        virtual std::pair<rhythm::RhythmTime, rhythm::RhythmTime> NoteTimeRange() const
        {
            const auto &notes = Notes();
            return notes.empty() ? std::pair{rhythm::RhythmTime{}, rhythm::RhythmTime{}}
                                 : std::pair{notes.front().timing, notes.back().timing};
        }
        virtual std::uint64_t AudioSourceRevision() const noexcept { return Revision(); }
        virtual double EffectValueAt(EffectCommandType type, MusicalPosition position, double fallback = 1) const
        { return Timeline().EffectValueAt(Effects(), type, position, fallback); }
        virtual double MinimumScrollMultiplier() const { return 1; }
        virtual long double ScrollDistance(rhythm::RhythmTime target, rhythm::RhythmTime current) const
        { return static_cast<long double>(target.count()) - current.count(); }
        virtual bool Dirty() const noexcept = 0;
        virtual std::uint64_t Revision() const noexcept = 0;
        virtual void Replace(PatternDocument pattern, EffectDocument effects) = 0;
        virtual void AddNote(MusicalPosition position, int keyType, std::optional<MusicalPosition> end = {},
                             std::vector<std::string> extra = {}) = 0;
        virtual void DeleteNote(std::size_t sourceOrder) = 0;
        virtual void SetMeasureLength(std::int64_t measure, Rational length) = 0;
        virtual void Save() = 0;
    };
} // namespace finger_drum::chart
