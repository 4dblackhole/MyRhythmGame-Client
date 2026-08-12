#include "Timing/MusicalTimeline.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace finger_drum::chart
{
    MusicalTimeline::MusicalTimeline(const PatternDocument& pattern)
        : baseBpm_(pattern.baseBpm),
          offsetMilliseconds_(pattern.patternOffsetMilliseconds),
          directives_(pattern.timing)
    {
        if (baseBpm_ <= 0.0)
        {
            throw std::invalid_argument("Base BPM must be greater than zero.");
        }
        std::ranges::stable_sort(
            directives_,
            [](const TimingDirective& left, const TimingDirective& right)
            {
                return left.position < right.position;
            });

        tempoPoints_.push_back({0.0L, baseBpm_});
        for (const TimingDirective& directive : directives_)
        {
            if (directive.type != TimingDirectiveType::Bpm ||
                directive.value <= 0.0)
            {
                continue;
            }
            tempoPoints_.push_back({
                PositionToBeat(directive.position),
                directive.value});
        }
        std::ranges::stable_sort(
            tempoPoints_,
            [](const TempoPoint& left, const TempoPoint& right)
            {
                return left.beat < right.beat;
            });

        // The final declaration at an identical beat wins without creating a
        // zero-length tempo segment.
        std::vector<TempoPoint> deduplicated;
        for (const TempoPoint point : tempoPoints_)
        {
            if (!deduplicated.empty() &&
                std::abs(deduplicated.back().beat - point.beat) < 1e-12L)
            {
                deduplicated.back().bpm = point.bpm;
            }
            else
            {
                deduplicated.push_back(point);
            }
        }
        tempoPoints_ = std::move(deduplicated);
    }

    rhythm::RhythmTime MusicalTimeline::Compile(
        const MusicalPosition position) const noexcept
    {
        const long double milliseconds =
            SecondsAtBeat(PositionToBeat(position)) * 1000.0L +
            DelayMillisecondsAt(position) +
            static_cast<long double>(offsetMilliseconds_);
        return rhythm::RhythmTime{static_cast<rhythm::RhythmTime::rep>(
            std::llround(milliseconds * 1000.0L))};
    }

    std::vector<CompiledPatternNote> MusicalTimeline::CompileNotes(
        const PatternDocument& pattern) const
    {
        std::vector<CompiledPatternNote> result;
        result.reserve(pattern.notes.size());
        for (const PatternNote& note : pattern.notes)
        {
            result.push_back({note, Compile(note.position)});
        }
        std::ranges::stable_sort(
            result,
            [](const CompiledPatternNote& left,
               const CompiledPatternNote& right)
            {
                if (left.timing != right.timing)
                {
                    return left.timing < right.timing;
                }
                return left.note.sourceOrder < right.note.sourceOrder;
            });
        return result;
    }

    std::vector<CompiledEffectCommand> MusicalTimeline::CompileEffects(
        const EffectDocument& effects) const
    {
        std::vector<CompiledEffectCommand> result;
        result.reserve(effects.commands.size());
        for (const EffectCommand& command : effects.commands)
        {
            result.push_back(CompiledEffectCommand{
                command,
                Compile(command.position),
                rhythm::RhythmDuration{
                    static_cast<rhythm::RhythmDuration::rep>(std::llround(
                        command.durationMilliseconds * 1000.0))}});
        }
        std::ranges::stable_sort(
            result,
            [](const CompiledEffectCommand& left,
               const CompiledEffectCommand& right)
            {
                return left.timing < right.timing;
            });
        return result;
    }

    std::vector<rhythm::RhythmTime> MusicalTimeline::CompileSubdivisions(
        const MusicalPosition begin,
        const MusicalPosition end,
        const std::size_t divisionsPerWholeNote) const
    {
        std::vector<rhythm::RhythmTime> result;
        if (divisionsPerWholeNote == 0)
        {
            return result;
        }

        const long double beginBeat = PositionToBeat(begin);
        const long double endBeat = PositionToBeat(end);
        const long double beatStep = 4.0L /
            static_cast<long double>(divisionsPerWholeNote);
        for (long double beat = beginBeat;
             beat < endBeat - 1e-12L;
             beat += beatStep)
        {
            result.push_back(CompileBeat(beat));
        }
        return result;
    }

    long double MusicalTimeline::PositionToBeat(
        const MusicalPosition position) const noexcept
    {
        long double beat = 0.0L;
        for (std::int64_t measure = 0; measure < position.measure; ++measure)
        {
            beat += 4.0L * MeasureRatioAt(measure);
        }
        beat += 4.0L * MeasureRatioAt(position.measure) *
            position.fraction.Value();
        return beat;
    }

    long double MusicalTimeline::MeasureRatioAt(
        const std::int64_t measure) const noexcept
    {
        long double ratio = 1.0L;
        for (const TimingDirective& directive : directives_)
        {
            if (directive.type != TimingDirectiveType::MeasureLength ||
                directive.position.measure > measure)
            {
                continue;
            }
            ratio = directive.ratio.Value();
        }
        return ratio;
    }

    long double MusicalTimeline::SecondsAtBeat(
        const long double beat) const noexcept
    {
        long double seconds = 0.0L;
        long double segmentBegin = 0.0L;
        double bpm = baseBpm_;
        for (const TempoPoint& point : tempoPoints_)
        {
            if (point.beat <= segmentBegin)
            {
                bpm = point.bpm;
                continue;
            }
            if (point.beat >= beat)
            {
                break;
            }
            seconds += (point.beat - segmentBegin) * 60.0L /
                static_cast<long double>(bpm);
            segmentBegin = point.beat;
            bpm = point.bpm;
        }
        seconds += (beat - segmentBegin) * 60.0L /
            static_cast<long double>(bpm);
        return seconds;
    }

    rhythm::RhythmTime MusicalTimeline::CompileBeat(
        const long double beat) const noexcept
    {
        const long double milliseconds = SecondsAtBeat(beat) * 1000.0L +
            DelayMillisecondsAtBeat(beat) +
            static_cast<long double>(offsetMilliseconds_);
        return rhythm::RhythmTime{static_cast<rhythm::RhythmTime::rep>(
            std::llround(milliseconds * 1000.0L))};
    }

    long double MusicalTimeline::DelayMillisecondsAt(
        const MusicalPosition position) const noexcept
    {
        long double result = 0.0L;
        for (const TimingDirective& directive : directives_)
        {
            if (directive.type == TimingDirectiveType::DelayMilliseconds &&
                directive.position <= position)
            {
                result += static_cast<long double>(directive.value);
            }
        }
        return result;
    }

    long double MusicalTimeline::DelayMillisecondsAtBeat(
        const long double beat) const noexcept
    {
        long double result = 0.0L;
        for (const TimingDirective& directive : directives_)
        {
            if (directive.type == TimingDirectiveType::DelayMilliseconds &&
                PositionToBeat(directive.position) <= beat)
            {
                result += static_cast<long double>(directive.value);
            }
        }
        return result;
    }
}
