#include "Timing/MusicalTimeline.h"
#include "Automation/InterpolationExpression.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <stdexcept>
#include <tuple>

namespace finger_drum::chart
{
    namespace
    {
        constexpr long double MicrosecondsPerWholeNoteAtBpmOne = 240'000'000.0L;

        rhythm::RhythmTime CheckedTime(long double microseconds)
        {
            using Time = rhythm::RhythmTime;
            if (!std::isfinite(microseconds) ||
                microseconds <= static_cast<long double>(std::numeric_limits<Time::rep>::lowest()) ||
                microseconds >= static_cast<long double>(std::numeric_limits<Time::rep>::max()))
                throw std::overflow_error("Compiled chart time exceeds the microsecond clock range.");
            return Time{static_cast<Time::rep>(std::llround(microseconds))};
        }
    } // namespace

    MusicalTimeline::MusicalTimeline(const PatternDocument &pattern)
        : baseBpm_(pattern.baseBpm), offsetMilliseconds_(pattern.patternOffsetMilliseconds), directives_(pattern.timing)
    {
        if (!std::isfinite(baseBpm_) || baseBpm_ <= 0.0)
        {
            throw std::invalid_argument("Base BPM must be finite and greater than zero.");
        }
        if (!std::isfinite(offsetMilliseconds_))
        {
            throw std::invalid_argument("Pattern offset must be finite.");
        }

        BuildMeasurePrefixSums(pattern);
        std::ranges::stable_sort(directives_, [](const TimingDirective &left, const TimingDirective &right)
                                 { return left.position < right.position; });
        BuildTimePoints();
        measureSpans_.reserve(measureLengths_.size());
        for (std::size_t i = 0; i < measureLengths_.size(); ++i)
            measureSpans_.push_back(BuildMeasureSpan(static_cast<std::int64_t>(i)));
    }

    rhythm::RhythmTime MusicalTimeline::Compile(const MusicalPosition position) const
    {
        return CompileAbsolute(PositionToWholeNotes(position));
    }

    std::vector<CompiledPatternNote> MusicalTimeline::CompileNotes(const PatternDocument &pattern) const
    {
        std::vector<CompiledPatternNote> result;
        result.reserve(pattern.notes.size());
        for (const PatternNote &note : pattern.notes)
        {
            result.push_back({note, Compile(note.position), 1, IsPositionActive(note.position)});
        }
        std::ranges::stable_sort(result,
                                 [](const CompiledPatternNote &left, const CompiledPatternNote &right)
                                 {
                                     if (left.timing != right.timing)
                                     {
                                         return left.timing < right.timing;
                                     }
                                     return left.note.sourceOrder < right.note.sourceOrder;
                                 });
        return result;
    }

    std::vector<CompiledEffectCommand> MusicalTimeline::CompileEffects(const EffectDocument &effects) const
    {
        std::vector<CompiledEffectCommand> result;
        result.reserve(effects.commands.size());
        for (const EffectCommand &command : effects.commands)
        {
            if (command.position.measure < 0 || command.position.measure >= MaximumMeasureCount ||
                (command.endPosition &&
                 (command.endPosition->measure < 0 || command.endPosition->measure >= MaximumMeasureCount)))
                throw std::invalid_argument("An effect must use a measure in 0..99,999.");
            if (!IsEffectActive(command))
                continue;
            result.push_back(CompiledEffectCommand{
                command, Compile(command.position),
                command.endPosition ? CheckedTime(static_cast<long double>(Compile(*command.endPosition).count()) -
                                                  Compile(command.position).count())
                                    : rhythm::RhythmDuration{static_cast<rhythm::RhythmDuration::rep>(
                                          std::llround(command.durationMilliseconds * 1000.0))}});
        }
        std::ranges::stable_sort(result,
                                 [](const CompiledEffectCommand &left, const CompiledEffectCommand &right)
                                 {
                                     return std::pair{left.timing, left.command.position} <
                                            std::pair{right.timing, right.command.position};
                                 });
        return result;
    }

    std::vector<std::pair<std::size_t, std::size_t>> MusicalTimeline::SubdivisionRanges(MusicalPosition begin,
                                                                                        MusicalPosition end,
                                                                                        std::size_t divisions) const
    {
        if (divisions == 0)
            return {};
        if (divisions > static_cast<std::size_t>(std::numeric_limits<Rational::Representation>::max()))
            throw std::overflow_error("The musical subdivision denominator is too large.");
        const auto first = PositionToWholeNotes(begin), last = PositionToWholeNotes(end);
        if (last <= first)
            return {};
        const auto denominator = static_cast<Rational::Representation>(divisions);
        const auto ceilCount = [&](const Rational &position)
        {
            if (position <= first)
                return std::size_t{0};
            const auto scaled = (position - first) * denominator;
            const auto count =
                scaled.Numerator() / scaled.Denominator() + (scaled.Numerator() % scaled.Denominator() != 0 ? 1 : 0);
            if (static_cast<std::uint64_t>(count) > std::numeric_limits<std::size_t>::max())
                throw std::overflow_error("The musical subdivision count is too large.");
            return static_cast<std::size_t>(count);
        };
        const auto count = ceilCount(last);
        std::vector<std::pair<std::size_t, std::size_t>> result;
        std::size_t cursor = 0;
        // Find removed grid-index intervals without allocating or iterating their ticks.
        auto span = std::ranges::upper_bound(removedSpans_, first, {}, &RemovedSpan::end);
        for (; span != removedSpans_.end() && span->anchor < last && cursor < count; ++span)
        {
            const auto removedEnd = std::min(count, ceilCount(span->end));
            auto low = cursor, high = removedEnd;
            while (low < high)
            {
                const auto middle = low + (high - low) / 2;
                if (Contains(*span, first + Rational{static_cast<Rational::Representation>(middle), denominator}))
                    high = middle;
                else
                    low = middle + 1;
            }
            if (cursor < low)
                result.emplace_back(cursor, low);
            cursor = removedEnd;
        }
        if (cursor < count)
            result.emplace_back(cursor, count);
        return result;
    }

    std::vector<rhythm::RhythmTime> MusicalTimeline::CompileSubdivisions(MusicalPosition begin, MusicalPosition end,
                                                                         std::size_t divisions) const
    {
        const auto ranges = SubdivisionRanges(begin, end, divisions);
        std::size_t count = 0;
        for (const auto &[first, last] : ranges)
            count += last - first;
        const auto origin = PositionToWholeNotes(begin);
        const auto length = PositionToWholeNotes(end) - origin;
        // Decide alignment before conversion to microseconds: rounding must not create a tail tick.
        const bool includeTail = divisions != 0 && length >= Rational{} &&
                                 (length * static_cast<Rational::Representation>(divisions)).Denominator() == 1 &&
                                 IsPositionActive(end);
        std::vector<rhythm::RhythmTime> result;
        result.reserve(count + (includeTail ? 1 : 0));
        for (const auto &[first, last] : ranges)
            for (std::size_t i = first; i < last; ++i)
                result.push_back(CompileAbsolute(origin + Rational{static_cast<Rational::Representation>(i),
                                                                   static_cast<Rational::Representation>(divisions)}));
        if (includeTail)
            result.push_back(Compile(end));
        return result;
    }

    std::size_t MusicalTimeline::CountSubdivisions(MusicalPosition begin, MusicalPosition end,
                                                   std::size_t divisions) const
    {
        std::size_t count = 0;
        for (const auto &[first, last] : SubdivisionRanges(begin, end, divisions))
            count += last - first;
        return count;
    }

    std::vector<rhythm::RhythmTime> MusicalTimeline::CompileMeasureStarts() const
    {
        std::vector<rhythm::RhythmTime> result;
        result.reserve(measureSpans_.size());
        for (const auto &span : measureSpans_)
            if (span.end > span.begin)
                result.push_back(span.begin);
        return result;
    }

    bool MusicalTimeline::Contains(const RemovedSpan &span, const Rational &position) const
    {
        if (position < span.anchor || position >= span.end)
            return false;
        return position >= span.firstRemovedAnchor || CompileAbsolute(position) >= span.target;
    }

    const MusicalTimeline::RemovedSpan *MusicalTimeline::RemovedAt(const Rational &position) const
    {
        const auto next = std::ranges::upper_bound(removedSpans_, position, {}, &RemovedSpan::end);
        return next != removedSpans_.end() && Contains(*next, position) ? &*next : nullptr;
    }

    bool MusicalTimeline::IsPositionActive(MusicalPosition position) const
    {
        return !RemovedAt(PositionToWholeNotes(position));
    }

    bool MusicalTimeline::IsEffectActive(const EffectCommand &command) const
    {
        // Areas keep historical endpoint times exactly like long notes. Only
        // point commands disappear when their authored position is overwritten.
        return command.endPosition ? Compile(*command.endPosition) >= Compile(command.position)
                                   : IsPositionActive(command.position);
    }

    MusicalTimeline::MeasureTimeSpan MusicalTimeline::BuildMeasureSpan(std::int64_t measure) const
    {
        const auto first = PositionToWholeNotes({measure, {}});
        const auto last = first + MeasureLength(measure);
        const auto *cutBegin = RemovedAt(first), *cutEnd = RemovedAt(last);
        const auto removedLengthBefore = [&](const Rational &p)
        {
            const auto next = std::ranges::upper_bound(removedSpans_, p, {}, &RemovedSpan::end);
            long double length = next == removedSpans_.begin() ? 0 : std::prev(next)->removedWholePrefix;
            if (next != removedSpans_.end())
                length += std::max(0.0L, p.Value() - next->sourceBegin);
            return length;
        };
        const auto gapLengthBefore = [&](const Rational &p)
        {
            // The closed measure end owns a gap exactly on its boundary.
            const auto next = std::ranges::upper_bound(activeTimePoints_, p, {}, &TimePoint::gapPosition);
            return next == activeTimePoints_.begin() ? 0.0L : std::prev(next)->gapWholePrefix;
        };
        return {cutBegin ? cutBegin->target : CompileAbsolute(first), cutEnd ? cutEnd->target : CompileAbsolute(last),
                std::max(0.0L, MeasureLength(measure).Value() - removedLengthBefore(last) + removedLengthBefore(first) +
                                   gapLengthBefore(last) - gapLengthBefore(first)),
                cutBegin ? PositionAtWholeNotes(cutBegin->end) : MusicalPosition{measure, {}}};
    }

    MusicalTimeline::MeasureTimeSpan MusicalTimeline::MeasureSpan(std::int64_t measure) const
    {
        if (measure < 0 || measure > MaximumMeasureCount)
            throw std::invalid_argument("A measure exceeds the supported range.");
        return static_cast<std::uint64_t>(measure) < measureSpans_.size()
                   ? measureSpans_[static_cast<std::size_t>(measure)]
                   : BuildMeasureSpan(measure);
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

    void MusicalTimeline::BuildMeasurePrefixSums(const PatternDocument &pattern)
    {
        std::int64_t maximumMeasure = 0;
        std::map<std::int64_t, Rational> measureChanges;
        for (const TimingDirective &directive : directives_)
        {
            if (directive.position.measure < 0 || directive.position.measure >= MaximumMeasureCount)
            {
                throw std::invalid_argument("A timing directive must use a measure in 0..99,999.");
            }
            maximumMeasure = std::max(maximumMeasure, directive.position.measure);
            if (directive.type == TimingDirectiveType::MeasureLength)
            {
                if (directive.ratio <= Rational{0, 1})
                {
                    throw std::invalid_argument("A measure length must be greater than zero.");
                }
                measureChanges[directive.position.measure] = directive.ratio;
            }
        }
        for (const PatternNote &note : pattern.notes)
        {
            if (note.position.measure < 0 || note.position.measure >= MaximumMeasureCount)
            {
                throw std::invalid_argument("A note must use a measure in 0..99,999.");
            }
            maximumMeasure = std::max(maximumMeasure, note.position.measure);
        }

        measureLengths_.reserve(static_cast<std::size_t>(maximumMeasure + 1));
        measurePrefixSums_.reserve(static_cast<std::size_t>(maximumMeasure + 2));
        measurePrefixSums_.push_back(Rational{0, 1});

        Rational currentLength{1, 1};
        for (std::int64_t measure = 0; measure <= maximumMeasure; ++measure)
        {
            if (const auto change = measureChanges.find(measure); change != measureChanges.end())
            {
                currentLength = change->second;
            }
            measureLengths_.push_back(currentLength);
            measurePrefixSums_.push_back(measurePrefixSums_.back() + currentLength);
        }
    }

    void MusicalTimeline::BuildTimePoints()
    {
        struct Change
        {
            std::optional<double> bpm;
            long double delayMicroseconds{};
        };
        std::map<Rational, Change> changes;
        for (const auto &directive : directives_)
        {
            if (directive.type == TimingDirectiveType::MeasureLength)
                continue;
            auto &change = changes[PositionToWholeNotes(directive.position)];
            if (directive.type == TimingDirectiveType::Bpm)
            {
                if (!std::isfinite(directive.value) || directive.value <= 0)
                    throw std::invalid_argument("A BPM directive must be finite and greater than zero.");
                change.bpm = directive.value;
            }
            else
            {
                if (!std::isfinite(directive.value) || directive.position.fraction < Rational{} ||
                    directive.position.fraction > MeasureLength(directive.position.measure))
                    throw std::invalid_argument("A delay must be finite and inside its measure's closed end.");
                change.delayMicroseconds += static_cast<long double>(directive.value) * 1'000;
            }
        }
        const long double offset = static_cast<long double>(offsetMilliseconds_) * 1'000;
        static_cast<void>(CheckedTime(offset));
        timePoints_.push_back({{}, offset, offset, baseBpm_});
        activeTimePoints_ = timePoints_;
        timePoints_.reserve(changes.size() + 1);
        activeTimePoints_.reserve(changes.size() + 1);
        for (const auto &[position, change] : changes)
        {
            const auto &previous = activeTimePoints_.back();
            const long double before = previous.timeAfterMicroseconds + (position - previous.position).Value() *
                                                                            MicrosecondsPerWholeNoteAtBpmOne /
                                                                            previous.bpm;
            const long double after = before + change.delayMicroseconds;
            const auto target = CheckedTime(after);
            long double retainedGap =
                std::max(0.0L, change.delayMicroseconds) * previous.bpm / MicrosecondsPerWholeNoteAtBpmOne;
            auto gapPosition = position;
            if (change.delayMicroseconds < 0)
                std::tie(gapPosition, retainedGap) = RemoveElapsedInterval(position, target, after);
            const auto &anchor = activeTimePoints_.back();
            const TimePoint point{
                position,   before, after, change.bpm.value_or(anchor.bpm), anchor.gapWholePrefix + retainedGap,
                gapPosition};
            timePoints_.push_back(point);
            activeTimePoints_.push_back(point);
        }
    }

    std::pair<Rational, long double> MusicalTimeline::RemoveElapsedInterval(const Rational &position,
                                                                            rhythm::RhythmTime target,
                                                                            long double after)
    {
        Rational firstRemoved = position;
        // Restore the tempo before all overwritten BPM/delay anchors. The
        // synthetic Base BPM origin also supports a jump before chart zero.
        while (activeTimePoints_.size() > 1 && CheckedTime(activeTimePoints_.back().timeAfterMicroseconds) >= target)
        {
            firstRemoved = activeTimePoints_.back().position;
            activeTimePoints_.pop_back();
        }
        const auto &anchor = activeTimePoints_.back();
        while (!removedSpans_.empty() && removedSpans_.back().end > anchor.position)
            removedSpans_.pop_back();
        const long double elapsedBeats =
            (after - anchor.timeAfterMicroseconds) * anchor.bpm / MicrosecondsPerWholeNoteAtBpmOne;
        const long double sourceBegin = std::clamp(anchor.position.Value() + elapsedBeats, 0.0L, firstRemoved.Value());
        const long double prefix = removedSpans_.empty() ? 0 : removedSpans_.back().removedWholePrefix;
        removedSpans_.push_back(
            {anchor.position, firstRemoved, position, target, sourceBegin, prefix + position.Value() - sourceBegin});
        // A cut inside a former positive gap retains its elapsed remainder
        // without retaining the overwritten delay/BPM command itself.
        return {firstRemoved, std::max(0.0L, elapsedBeats - (sourceBegin - anchor.position.Value()))};
    }

    Rational MusicalTimeline::PositionToWholeNotes(const MusicalPosition position) const
    {
        // The end sentinel is needed for measure-boundary queries, but not file entries.
        if (position.measure < 0 || position.measure > MaximumMeasureCount)
        {
            throw std::invalid_argument("A musical position exceeds the supported measure range.");
        }

        const std::size_t measure = static_cast<std::size_t>(position.measure);
        if (measure < measureLengths_.size())
        {
            return measurePrefixSums_[measure] + position.fraction;
        }

        const auto additionalMeasures = static_cast<Rational::Representation>(measure - measureLengths_.size());
        return measurePrefixSums_.back() + measureLengths_.back() * additionalMeasures + position.fraction;
    }

    MusicalPosition MusicalTimeline::PositionAtWholeNotes(const Rational &beat) const
    {
        if (beat < Rational{})
            throw std::invalid_argument("Beat cannot be negative.");
        const auto next = std::ranges::upper_bound(measurePrefixSums_, beat);
        if (next != measurePrefixSums_.end())
        {
            const auto measure = std::distance(measurePrefixSums_.begin(), next) - 1;
            return {measure, beat - measurePrefixSums_[static_cast<std::size_t>(measure)]};
        }
        // Extrapolated measures use the last known signature. Binary search
        // keeps seeking sparse/long charts exact without a per-measure loop.
        std::int64_t low = static_cast<std::int64_t>(measureLengths_.size());
        if (low >= MaximumMeasureCount)
            throw std::out_of_range("Beat is beyond 100,000 measures.");
        std::int64_t high = low + 1;
        while (PositionToWholeNotes({high, {}}) <= beat)
        {
            if (high == MaximumMeasureCount)
                throw std::out_of_range("Beat is beyond 100,000 measures.");
            low = high;
            high = std::min(high * 2, MaximumMeasureCount);
        }
        while (low + 1 < high)
        {
            const auto middle = low + (high - low) / 2;
            if (PositionToWholeNotes({middle, {}}) <= beat)
                low = middle;
            else
                high = middle;
        }
        return {low, beat - PositionToWholeNotes({low, {}})};
    }

    long double MusicalTimeline::WholeNotesAtTime(const rhythm::RhythmTime time) const noexcept
    {
        const long double microseconds = static_cast<long double>(time.count());
        const auto secondsToBeats = [](const long double delta, const double bpm)
        { return delta * static_cast<long double>(bpm) / 240'000'000.0L; };
        auto firstPoint = activeTimePoints_.begin();
        if (activeTimePoints_.size() > 1 &&
            activeTimePoints_[1].timeAfterMicroseconds < firstPoint->timeAfterMicroseconds)
            ++firstPoint;
        const auto next = std::upper_bound(firstPoint, activeTimePoints_.end(), microseconds,
                                           [](long double value, const TimePoint &point)
                                           { return value < point.timeAfterMicroseconds; });
        if (next == firstPoint)
            return secondsToBeats(microseconds - timePoints_.front().timeBeforeMicroseconds, baseBpm_);
        const TimePoint &current = *std::prev(next);
        if (next != activeTimePoints_.end() && microseconds >= next->timeBeforeMicroseconds)
            return next->position.Value();
        return current.position.Value() + secondsToBeats(microseconds - current.timeAfterMicroseconds, current.bpm);
    }

    rhythm::RhythmTime MusicalTimeline::TimeAtWholeNotes(const long double beat) const
    {
        if (!std::isfinite(beat))
            throw std::invalid_argument("Scroll beat must be finite.");
        const TimePoint &first = timePoints_.front();
        const TimePoint *point = &first;
        if (beat > first.position.Value())
        {
            const auto next = std::ranges::upper_bound(timePoints_, beat, {},
                                                       [](const TimePoint &value) { return value.position.Value(); });
            point = &*std::prev(next);
        }
        const long double origin =
            beat < first.position.Value() ? first.timeBeforeMicroseconds : point->timeAfterMicroseconds;
        return CheckedTime(origin + (beat - point->position.Value()) * MicrosecondsPerWholeNoteAtBpmOne /
                                        static_cast<long double>(point->bpm));
    }

    const EffectCommand *MusicalTimeline::FindEffectAt(const EffectDocument &effects, EffectCommandType type,
                                                       MusicalPosition position) const
    {
        const auto time = Compile(position);
        const EffectCommand *selected = nullptr;
        rhythm::RhythmTime selectedTime{};
        for (const auto &c : effects.commands)
            if (c.type == type && IsEffectActive(c))
            {
                const auto begin = Compile(c.position);
                if (begin <= time &&
                    (!selected || begin > selectedTime || (begin == selectedTime && selected->position <= c.position)))
                {
                    selected = &c;
                    selectedTime = begin;
                }
            }
        return selected;
    }

    double MusicalTimeline::EffectValueAt(const EffectDocument &effects, EffectCommandType type,
                                          MusicalPosition position, double defaultValue) const
    {
        const auto *selected = FindEffectAt(effects, type, position);
        return selected ? EffectValueAt(*selected, position) : defaultValue;
    }

    double MusicalTimeline::EffectValueAt(const EffectCommand &c, MusicalPosition position) const
    {
        const auto duration =
            c.endPosition
                ? CheckedTime(static_cast<long double>(Compile(*c.endPosition).count()) - Compile(c.position).count())
                : rhythm::RhythmDuration{
                      static_cast<rhythm::RhythmDuration::rep>(std::llround(c.durationMilliseconds * 1000))};
        if (duration <= rhythm::RhythmDuration::zero())
            return c.endValue;
        return EvaluateInterpolation(
            c, static_cast<double>(static_cast<long double>(Compile(position).count()) - Compile(c.position).count()) /
                   static_cast<double>(duration.count()));
    }

    long double MusicalTimeline::MicrosecondsAt(const Rational &position) const
    {
        const auto next = std::ranges::upper_bound(timePoints_, position, {}, &TimePoint::position);
        const TimePoint &point = next == timePoints_.begin() ? timePoints_.front() : *std::prev(next);
        return point.timeAfterMicroseconds + (position - point.position).Value() * MicrosecondsPerWholeNoteAtBpmOne /
                                                 static_cast<long double>(point.bpm);
    }

    rhythm::RhythmTime MusicalTimeline::CompileAbsolute(const Rational &position) const
    {
        return CheckedTime(MicrosecondsAt(position));
    }
} // namespace finger_drum::chart
