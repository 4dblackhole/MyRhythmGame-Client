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
        virtual const PatternDocument &Pattern() const noexcept = 0;
        virtual const EffectDocument &Effects() const noexcept = 0;
        virtual const MusicalTimeline &Timeline() const noexcept = 0;
        virtual const std::vector<CompiledPatternNote> &Notes() const noexcept = 0;
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
