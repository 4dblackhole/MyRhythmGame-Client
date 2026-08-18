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
            MusicalPosition position) const;
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
            Rational position{};
            long double seconds{};
            double bpm{120.0};
        };

        void BuildMeasurePrefixSums(const PatternDocument& pattern);
        void BuildTempoPoints();
        [[nodiscard]] Rational PositionToWholeNotes(
            MusicalPosition position) const;
        [[nodiscard]] long double SecondsAt(
            const Rational& position) const;
        [[nodiscard]] rhythm::RhythmTime CompileAbsolute(
            const Rational& position) const;
        [[nodiscard]] long double DelayMillisecondsAt(
            const Rational& position) const;

        double baseBpm_{120.0};
        double offsetMilliseconds_{};
        std::vector<TimingDirective> directives_;
        std::vector<Rational> measureLengths_;
        std::vector<Rational> measurePrefixSums_;
        std::vector<TempoPoint> tempoPoints_;
    };
}
