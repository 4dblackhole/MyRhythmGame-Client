#include "Timing/MusicalTimeline.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <stdexcept>

namespace finger_drum::chart
{
    namespace
    {
        constexpr long double SecondsPerWholeNoteAtBpmOne = 240.0L;
    }

    MusicalTimeline::MusicalTimeline(const PatternDocument& pattern)
        : baseBpm_(pattern.baseBpm),
          offsetMilliseconds_(pattern.patternOffsetMilliseconds),
          directives_(pattern.timing)
    {
        if (!std::isfinite(baseBpm_) || baseBpm_ <= 0.0)
        {
            throw std::invalid_argument(
                "Base BPM must be finite and greater than zero.");
        }
        if (!std::isfinite(offsetMilliseconds_))
        {
            throw std::invalid_argument("Pattern offset must be finite.");
        }

        BuildMeasurePrefixSums(pattern);
        std::ranges::stable_sort(
            directives_,
            [](const TimingDirective& left, const TimingDirective& right)
            {
                return left.position < right.position;
            });
        BuildTempoPoints();
        BuildScrollPoints();
    }

    rhythm::RhythmTime MusicalTimeline::Compile(
        const MusicalPosition position) const
    {
        return CompileAbsolute(PositionToWholeNotes(position));
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
                command.endPosition ? Compile(*command.endPosition) - Compile(command.position) : rhythm::RhythmDuration{
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
        result.reserve(CountSubdivisions(
            begin,
            end,
            divisionsPerWholeNote));
        if (divisionsPerWholeNote == 0)
        {
            return result;
        }

        Rational position = PositionToWholeNotes(begin);
        const Rational endPosition = PositionToWholeNotes(end);
        const Rational step{
            1,
            static_cast<Rational::Representation>(divisionsPerWholeNote)};
        while (position < endPosition)
        {
            result.push_back(CompileAbsolute(position));
            position += step;
        }
        return result;
    }

    std::size_t MusicalTimeline::CountSubdivisions(
        const MusicalPosition begin,
        const MusicalPosition end,
        const std::size_t divisionsPerWholeNote) const
    {
        if (divisionsPerWholeNote == 0)
        {
            return 0;
        }
        if (divisionsPerWholeNote > static_cast<std::size_t>(
                std::numeric_limits<Rational::Representation>::max()))
        {
            throw std::overflow_error(
                "The musical subdivision denominator is too large.");
        }

        const Rational beginPosition = PositionToWholeNotes(begin);
        const Rational endPosition = PositionToWholeNotes(end);
        if (endPosition <= beginPosition)
        {
            return 0;
        }

        const Rational scaledLength =
            (endPosition - beginPosition) *
            static_cast<Rational::Representation>(divisionsPerWholeNote);
        const Rational::Representation quotient =
            scaledLength.Numerator() / scaledLength.Denominator();
        const Rational::Representation remainder =
            scaledLength.Numerator() % scaledLength.Denominator();
        const auto count = quotient + (remainder == 0 ? 0 : 1);
        if (static_cast<std::uint64_t>(count) >
            std::numeric_limits<std::size_t>::max())
        {
            throw std::overflow_error(
                "The musical subdivision count is too large.");
        }
        return static_cast<std::size_t>(count);
    }

    std::vector<rhythm::RhythmTime>
    MusicalTimeline::CompileMeasureStarts() const
    {
        std::vector<rhythm::RhythmTime> result;
        result.reserve(measureLengths_.size());
        for (std::size_t index = 0; index < measureLengths_.size(); ++index)
        {
            result.push_back(CompileAbsolute(measurePrefixSums_[index]));
        }
        return result;
    }

    Rational MusicalTimeline::MeasureLength(const std::int64_t measure) const
    {
        if (measure < 0)
        {
            throw std::invalid_argument("A measure cannot be negative.");
        }
        return static_cast<std::uint64_t>(measure) < measureLengths_.size()
            ? measureLengths_[static_cast<std::size_t>(measure)]
            : measureLengths_.back();
    }

    void MusicalTimeline::BuildMeasurePrefixSums(
        const PatternDocument& pattern)
    {
        std::int64_t maximumMeasure = 0;
        std::map<std::int64_t, Rational> measureChanges;
        for (const TimingDirective& directive : directives_)
        {
            if (directive.position.measure < 0)
            {
                throw std::invalid_argument(
                    "A timing directive cannot use a negative measure.");
            }
            maximumMeasure = std::max(
                maximumMeasure,
                directive.position.measure);
            if (directive.type == TimingDirectiveType::MeasureLength)
            {
                if (directive.ratio <= Rational{0, 1})
                {
                    throw std::invalid_argument(
                        "A measure length must be greater than zero.");
                }
                measureChanges[directive.position.measure] = directive.ratio;
            }
        }
        for (const PatternNote& note : pattern.notes)
        {
            if (note.position.measure < 0)
            {
                throw std::invalid_argument(
                    "A note cannot use a negative measure.");
            }
            maximumMeasure = std::max(maximumMeasure, note.position.measure);
        }

        measureLengths_.reserve(
            static_cast<std::size_t>(maximumMeasure + 1));
        measurePrefixSums_.reserve(
            static_cast<std::size_t>(maximumMeasure + 2));
        measurePrefixSums_.push_back(Rational{0, 1});

        Rational currentLength{1, 1};
        for (std::int64_t measure = 0;
             measure <= maximumMeasure;
             ++measure)
        {
            if (const auto change = measureChanges.find(measure);
                change != measureChanges.end())
            {
                currentLength = change->second;
            }
            measureLengths_.push_back(currentLength);
            measurePrefixSums_.push_back(
                measurePrefixSums_.back() + currentLength);
        }
    }

    void MusicalTimeline::BuildTempoPoints()
    {
        struct TempoDefinition
        {
            Rational position;
            double bpm{};
        };

        std::vector<TempoDefinition> definitions;
        definitions.push_back({Rational{0, 1}, baseBpm_});
        for (const TimingDirective& directive : directives_)
        {
            if (directive.type != TimingDirectiveType::Bpm)
            {
                continue;
            }
            if (!std::isfinite(directive.value) || directive.value <= 0.0)
            {
                throw std::invalid_argument(
                    "A BPM directive must be finite and greater than zero.");
            }
            definitions.push_back({
                PositionToWholeNotes(directive.position),
                directive.value});
        }
        std::ranges::stable_sort(
            definitions,
            [](const TempoDefinition& left, const TempoDefinition& right)
            {
                return left.position < right.position;
            });

        std::vector<TempoDefinition> deduplicated;
        for (const TempoDefinition& definition : definitions)
        {
            if (!deduplicated.empty() &&
                deduplicated.back().position == definition.position)
            {
                deduplicated.back().bpm = definition.bpm;
            }
            else
            {
                deduplicated.push_back(definition);
            }
        }

        Rational previousPosition{0, 1};
        long double elapsedSeconds = 0.0L;
        double previousBpm = baseBpm_;
        for (const TempoDefinition& definition : deduplicated)
        {
            elapsedSeconds += (definition.position - previousPosition).Value() *
                SecondsPerWholeNoteAtBpmOne /
                static_cast<long double>(previousBpm);
            tempoPoints_.push_back({
                definition.position,
                elapsedSeconds,
                definition.bpm});
            previousPosition = definition.position;
            previousBpm = definition.bpm;
        }
    }

    Rational MusicalTimeline::PositionToWholeNotes(
        const MusicalPosition position) const
    {
        if (position.measure < 0)
        {
            throw std::invalid_argument(
                "A musical position cannot use a negative measure.");
        }

        const std::size_t measure = static_cast<std::size_t>(position.measure);
        if (measure < measureLengths_.size())
        {
            return measurePrefixSums_[measure] + position.fraction;
        }

        const auto additionalMeasures = static_cast<Rational::Representation>(
            measure - measureLengths_.size());
        return measurePrefixSums_.back() +
            measureLengths_.back() * additionalMeasures +
            position.fraction;
    }

    MusicalPosition MusicalTimeline::PositionAtWholeNotes(const Rational& beat) const
    {
        if (beat < Rational{}) throw std::invalid_argument("Beat cannot be negative.");
        const auto next = std::ranges::upper_bound(measurePrefixSums_, beat);
        if (next != measurePrefixSums_.end())
        {
            const auto measure = std::distance(measurePrefixSums_.begin(), next) - 1;
            return {measure, beat - measurePrefixSums_[static_cast<std::size_t>(measure)]};
        }
        // Extrapolated measures use the last known signature. Binary search
        // keeps seeking sparse/long charts exact without a per-measure loop.
        std::int64_t low = static_cast<std::int64_t>(measureLengths_.size());
        std::int64_t high = low + 1;
        while (PositionToWholeNotes({high, {}}) <= beat)
        {
            if (high > std::numeric_limits<std::int64_t>::max() / 2)
                throw std::overflow_error("Beat is beyond the supported measure range.");
            high *= 2;
        }
        while (low + 1 < high)
        {
            const auto middle = low + (high - low) / 2;
            if (PositionToWholeNotes({middle, {}}) <= beat) low = middle;
            else high = middle;
        }
        return {low, beat - PositionToWholeNotes({low, {}})};
    }

    long double MusicalTimeline::WholeNotesAtTime(const rhythm::RhythmTime time) const noexcept
    {
        const long double microseconds = static_cast<long double>(time.count());
        const auto secondsToBeats = [](const long double delta, const double bpm)
        {
            return delta * static_cast<long double>(bpm) / 240'000'000.0L;
        };
        const ScrollPoint& first = scrollPoints_.front();
        if (microseconds < first.timeAfterMicroseconds)
        {
            return microseconds < first.timeBeforeMicroseconds
                ? first.beat + secondsToBeats(microseconds - first.timeBeforeMicroseconds, first.bpm)
                : first.beat;
        }

        auto next = scrollTimesMonotonic_
            ? std::ranges::upper_bound(scrollPoints_, microseconds, {},
                                       &ScrollPoint::timeAfterMicroseconds)
            : scrollPoints_.begin();
        if (!scrollTimesMonotonic_)
        {
            // A negative delay can overlap earlier chart time. Choose the
            // latest reached chart anchor without assuming sorted timestamps.
            for (auto point = scrollPoints_.begin(); point != scrollPoints_.end(); ++point)
                if (point->timeAfterMicroseconds <= microseconds) next = std::next(point);
        }
        const ScrollPoint& current = *std::prev(next);
        if (next != scrollPoints_.end() && microseconds >= next->timeBeforeMicroseconds)
            return next->beat;
        return current.beat + secondsToBeats(
            microseconds - current.timeAfterMicroseconds, current.bpm);
    }

    rhythm::RhythmTime MusicalTimeline::TimeAtWholeNotes(const long double beat) const
    {
        if (!std::isfinite(beat))
            throw std::invalid_argument("Scroll beat must be finite.");
        const ScrollPoint& first = scrollPoints_.front();
        const ScrollPoint* point = &first;
        if (beat > first.beat)
        {
            const auto next = std::ranges::upper_bound(scrollPoints_, beat, {}, &ScrollPoint::beat);
            point = &*std::prev(next);
        }
        const long double origin = beat < first.beat
            ? first.timeBeforeMicroseconds : point->timeAfterMicroseconds;
        const long double value = origin + (beat - point->beat) *
            240'000'000.0L / static_cast<long double>(point->bpm);
        if (value < static_cast<long double>(std::numeric_limits<rhythm::RhythmTime::rep>::min()) ||
            value > static_cast<long double>(std::numeric_limits<rhythm::RhythmTime::rep>::max()))
            throw std::overflow_error("Scroll time exceeds the supported range.");
        return rhythm::RhythmTime{static_cast<rhythm::RhythmTime::rep>(std::llround(value))};
    }

    double MusicalTimeline::EffectValueAt(const EffectDocument& effects,
        EffectCommandType type, MusicalPosition position, double defaultValue) const
    {
        const EffectCommand* selected = nullptr;
        for (const auto& c : effects.commands)
            if (c.type == type && c.position <= position &&
                (!selected || selected->position <= c.position)) selected = &c;
        if (!selected) return defaultValue;
        const auto& c = *selected;
        double amount = 1;
        if (c.endPosition)
        {
            const auto begin = PositionToWholeNotes(c.position);
            const auto length = PositionToWholeNotes(*c.endPosition) - begin;
            if (length > Rational{}) amount = static_cast<double>(
                (PositionToWholeNotes(position) - begin).Value() / length.Value());
        }
        else if (c.durationMilliseconds > 0)
            amount = (Compile(position) - Compile(c.position)).count() / (c.durationMilliseconds * 1000);
        amount = std::clamp(amount, 0.0, 1.0);
        if (c.curve == AutomationCurve::Step) amount = amount >= 1 ? 1 : 0;
        else if (c.curve == AutomationCurve::Smoothstep) amount = amount * amount * (3 - 2 * amount);
        else if (c.curve == AutomationCurve::Exponential) amount *= amount;
        return std::lerp(c.beginValue, c.endValue, amount);
    }

    long double MusicalTimeline::SecondsAt(
        const Rational& position) const
    {
        const auto next = std::ranges::upper_bound(
            tempoPoints_,
            position,
            {},
            &TempoPoint::position);
        const TempoPoint& point = next == tempoPoints_.begin() ?
            tempoPoints_.front() : *std::prev(next);
        return point.seconds + (position - point.position).Value() *
            SecondsPerWholeNoteAtBpmOne /
            static_cast<long double>(point.bpm);
    }

    void MusicalTimeline::BuildScrollPoints()
    {
        std::map<Rational, long double> delays;
        delays[Rational{0, 1}] = 0.0L;
        for (const TempoPoint& point : tempoPoints_)
            delays[point.position] += 0.0L;
        for (const TimingDirective& directive : directives_)
            if (directive.type == TimingDirectiveType::DelayMilliseconds)
            {
                if (!std::isfinite(directive.value))
                    throw std::invalid_argument("A delay must be finite.");
                delays[PositionToWholeNotes(directive.position)] +=
                    static_cast<long double>(directive.value) * 1'000.0L;
            }

        long double accumulatedDelay = 0.0L;
        for (const auto& [position, delay] : delays)
        {
            const long double before = SecondsAt(position) * 1'000'000.0L +
                accumulatedDelay + static_cast<long double>(offsetMilliseconds_) * 1'000.0L;
            accumulatedDelay += delay;
            const auto nextTempo = std::ranges::upper_bound(
                tempoPoints_, position, {}, &TempoPoint::position);
            const TempoPoint& tempo = *std::prev(nextTempo);
            scrollPoints_.push_back({position.Value(), before, before + delay, tempo.bpm});
            if (scrollPoints_.size() > 1 &&
                scrollPoints_.back().timeAfterMicroseconds <
                    scrollPoints_[scrollPoints_.size() - 2].timeAfterMicroseconds)
                scrollTimesMonotonic_ = false;
        }
    }

    rhythm::RhythmTime MusicalTimeline::CompileAbsolute(
        const Rational& position) const
    {
        const long double microseconds =
            SecondsAt(position) * 1'000'000.0L +
            DelayMillisecondsAt(position) * 1'000.0L +
            static_cast<long double>(offsetMilliseconds_) * 1'000.0L;
        return rhythm::RhythmTime{static_cast<rhythm::RhythmTime::rep>(
            std::llround(microseconds))};
    }

    long double MusicalTimeline::DelayMillisecondsAt(
        const Rational& position) const
    {
        long double result = 0.0L;
        for (const TimingDirective& directive : directives_)
        {
            if (directive.type == TimingDirectiveType::DelayMilliseconds &&
                PositionToWholeNotes(directive.position) <= position)
            {
                result += static_cast<long double>(directive.value);
            }
        }
        return result;
    }
}
