#pragma once

#include "Editing/IEditorDocument.h"

#include <optional>
#include <string>

namespace finger_drum::chart
{
    // Editable source beats are exact rationals. Recompile the prefix sums and
    // note timestamps only after an edit, never during drawing or hit testing.
    class ChartEditor final : public IEditorDocument
    {
      public:
        ChartEditor(PatternDocument pattern, EffectDocument effects = {});
        [[nodiscard]] const PatternDocument &Pattern() const noexcept override
        {
            return pattern_;
        }
        [[nodiscard]] const EffectDocument &Effects() const noexcept override
        {
            return effects_;
        }
        [[nodiscard]] const MusicalTimeline &Timeline() const noexcept override
        {
            return timeline_;
        }
        [[nodiscard]] const std::vector<CompiledPatternNote> &Notes() const noexcept override
        {
            return notes_;
        }
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
        void CompileNoteCache();
        PatternDocument pattern_;
        EffectDocument effects_;
        MusicalTimeline timeline_;
        std::vector<CompiledPatternNote> notes_;
        bool dirty_{};
        std::uint64_t revision_{1};
    };
} // namespace finger_drum::chart
