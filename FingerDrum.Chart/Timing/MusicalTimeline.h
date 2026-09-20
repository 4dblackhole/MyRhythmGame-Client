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
        [[nodiscard]] std::size_t CountSubdivisions(
            MusicalPosition begin,
            MusicalPosition end,
            std::size_t divisionsPerWholeNote) const;
        [[nodiscard]] std::vector<rhythm::RhythmTime>
            CompileMeasureStarts() const;
        [[nodiscard]] Rational MeasureLength(std::int64_t measure) const;
        [[nodiscard]] Rational PositionToWholeNotes(MusicalPosition position) const;
        [[nodiscard]] MusicalPosition PositionAtWholeNotes(const Rational& beat) const;
        [[nodiscard]] double EffectValueAt(const EffectDocument& effects,
            EffectCommandType type, MusicalPosition position, double defaultValue = 1) const;

    private:
        struct TempoPoint
        {
            Rational position{};
            long double seconds{};
            double bpm{120.0};
        };

        void BuildMeasurePrefixSums(const PatternDocument& pattern);
        void BuildTempoPoints();
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
