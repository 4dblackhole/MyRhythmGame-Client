#pragma once

#include "Model/ChartDocument.h"

#include <cstddef>
#include <vector>

namespace finger_drum::chart
{
    class MusicalTimeline final
    {
    public:
        explicit MusicalTimeline(const PatternDocument& pattern);

        [[nodiscard]] rhythm::RhythmTime Compile(
            MusicalPosition position) const noexcept;
        [[nodiscard]] std::vector<CompiledPatternNote> CompileNotes(
            const PatternDocument& pattern) const;
        [[nodiscard]] std::vector<CompiledEffectCommand> CompileEffects(
            const EffectDocument& effects) const;
        [[nodiscard]] std::vector<rhythm::RhythmTime> CompileSubdivisions(
            MusicalPosition begin,
            MusicalPosition end,
            std::size_t divisionsPerWholeNote) const;

    private:
        struct TempoPoint
        {
            long double beat{};
            double bpm{120.0};
        };

        [[nodiscard]] long double PositionToBeat(
            MusicalPosition position) const noexcept;
        [[nodiscard]] long double MeasureRatioAt(
            std::int64_t measure) const noexcept;
        [[nodiscard]] long double SecondsAtBeat(long double beat) const noexcept;
        [[nodiscard]] rhythm::RhythmTime CompileBeat(
            long double beat) const noexcept;
        [[nodiscard]] long double DelayMillisecondsAt(
            MusicalPosition position) const noexcept;
        [[nodiscard]] long double DelayMillisecondsAtBeat(
            long double beat) const noexcept;

        double baseBpm_{120.0};
        double offsetMilliseconds_{};
        std::vector<TimingDirective> directives_;
        std::vector<TempoPoint> tempoPoints_;
    };
}
