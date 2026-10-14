#pragma once

#include "Model/Submodules/CompiledChart.h"

#include <cstddef>
#include <utility>
#include <vector>

namespace finger_drum::chart
{
    class MusicalTimeline final
    {
      public:
        struct MeasureTimeSpan
        {
            rhythm::RhythmTime begin{}, end{};
            long double wholeNotes{};
            MusicalPosition sourceBegin{};
        };
        explicit MusicalTimeline(const PatternDocument &pattern, rhythm::RhythmDuration runtimeOffset = {});

        [[nodiscard]] rhythm::RhythmTime Compile(MusicalPosition position) const;
        [[nodiscard]] std::vector<CompiledPatternNote> CompileNotes(const PatternDocument &pattern) const;
        [[nodiscard]] std::vector<CompiledEffectCommand> CompileEffects(const EffectDocument &effects) const;
        // Scheduled ticks include an active end position only when it lies on the begin-anchored grid.
        [[nodiscard]] std::vector<rhythm::RhythmTime> CompileSubdivisions(MusicalPosition begin, MusicalPosition end,
                                                                          std::size_t divisionsPerWholeNote) const;
        // Automatic hit-count targets use [begin, end), independently of scheduled ticks.
        [[nodiscard]] std::size_t CountSubdivisions(MusicalPosition begin, MusicalPosition end,
                                                    std::size_t divisionsPerWholeNote) const;
        [[nodiscard]] std::vector<rhythm::RhythmTime> CompileMeasureStarts() const;
        [[nodiscard]] const std::vector<MeasureTimeSpan> &MeasureSpans() const noexcept
        {
            return measureSpans_;
        }
        [[nodiscard]] Rational MeasureLength(std::int64_t measure) const;
        [[nodiscard]] Rational PositionToWholeNotes(MusicalPosition position) const;
        [[nodiscard]] MusicalPosition PositionAtWholeNotes(const Rational &beat) const;
        [[nodiscard]] long double WholeNotesAtTime(rhythm::RhythmTime time) const noexcept;
        [[nodiscard]] rhythm::RhythmTime TimeAtWholeNotes(long double beat) const;
        [[nodiscard]] double BaseBpm() const noexcept
        {
            return baseBpm_;
        }
        [[nodiscard]] bool IsPositionActive(MusicalPosition position) const;
        [[nodiscard]] bool IsEffectActive(const EffectCommand &command) const;
        [[nodiscard]] MeasureTimeSpan MeasureSpan(std::int64_t measure) const;
        [[nodiscard]] const EffectCommand *FindEffectAt(const EffectDocument &, EffectCommandType,
                                                        MusicalPosition) const;
        [[nodiscard]] double EffectValueAt(const EffectDocument &effects, EffectCommandType type,
                                           MusicalPosition position, double defaultValue = 1) const;
        [[nodiscard]] double EffectValueAt(const EffectCommand &, MusicalPosition) const;

      private:
        struct TimePoint
        {
            Rational position{};
            long double timeBeforeMicroseconds{};
            long double timeAfterMicroseconds{};
            double bpm{120.0};
            long double gapWholePrefix{};
            Rational gapPosition{};
        };

        void BuildMeasurePrefixSums(const PatternDocument &pattern);
        void BuildTimePoints();
        std::pair<Rational, long double> RemoveElapsedInterval(const Rational &, rhythm::RhythmTime target,
                                                               long double after);
        struct RemovedSpan
        {
            Rational anchor{}, firstRemovedAnchor{}, end{};
            rhythm::RhythmTime target{};
            long double sourceBegin{}, removedWholePrefix{};
        };
        [[nodiscard]] const RemovedSpan *RemovedAt(const Rational &position) const;
        [[nodiscard]] bool Contains(const RemovedSpan &, const Rational &position) const;
        [[nodiscard]] std::vector<std::pair<std::size_t, std::size_t>> SubdivisionRanges(MusicalPosition begin,
                                                                                         MusicalPosition end,
                                                                                         std::size_t divisions) const;
        [[nodiscard]] MeasureTimeSpan BuildMeasureSpan(std::int64_t measure) const;
        [[nodiscard]] long double MicrosecondsAt(const Rational &position) const;
        [[nodiscard]] rhythm::RhythmTime CompileAbsolute(const Rational &position) const;

        rhythm::RhythmDuration runtimeOffset_{};
        double baseBpm_{120.0};
        double offsetMilliseconds_{};
        std::vector<TimingDirective> directives_;
        std::vector<Rational> measureLengths_;
        std::vector<Rational> measurePrefixSums_;
        std::vector<TimePoint> timePoints_;
        // Historical anchors preserve old endpoint times. Surviving anchors own
        // forward tempo state and inverse seeking after a negative time jump.
        std::vector<TimePoint> activeTimePoints_;
        std::vector<RemovedSpan> removedSpans_;
        std::vector<MeasureTimeSpan> measureSpans_;
    };
} // namespace finger_drum::chart
