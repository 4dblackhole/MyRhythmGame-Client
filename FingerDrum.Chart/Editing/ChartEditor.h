#pragma once

#include "Timing/MusicalTimeline.h"

#include <optional>
#include <string>

namespace finger_drum::chart
{
// Editable source beats are exact rationals. Recompile the prefix sums and
// note timestamps only after an edit, never during drawing or hit testing.
class ChartEditor final
{
  public:
    ChartEditor(PatternDocument pattern, EffectDocument effects = {});
    [[nodiscard]] const PatternDocument &Pattern() const noexcept
    {
        return pattern_;
    }
    [[nodiscard]] const EffectDocument &Effects() const noexcept
    {
        return effects_;
    }
    [[nodiscard]] const MusicalTimeline &Timeline() const noexcept
    {
        return timeline_;
    }
    [[nodiscard]] const std::vector<CompiledPatternNote> &Notes() const noexcept
    {
        return notes_;
    }
    [[nodiscard]] bool Dirty() const noexcept
    {
        return dirty_;
    }
    [[nodiscard]] std::uint64_t Revision() const noexcept
    {
        return revision_;
    }
    void Replace(PatternDocument pattern, EffectDocument effects);
    void AddNote(MusicalPosition position, int keyType, std::optional<MusicalPosition> end = {},
                 std::vector<std::string> extra = {});
    void DeleteNote(std::size_t sourceOrder);
    void SetMeasureLength(std::int64_t measure, Rational length);
    void Save();
    [[nodiscard]] static std::string WritePattern(const PatternDocument &pattern);
    [[nodiscard]] static std::string WriteEffects(const EffectDocument &effects);

  private:
    void Rebuild();
    PatternDocument pattern_;
    EffectDocument effects_;
    MusicalTimeline timeline_;
    std::vector<CompiledPatternNote> notes_;
    bool dirty_{};
    std::uint64_t revision_{1};
};
} // namespace finger_drum::chart
