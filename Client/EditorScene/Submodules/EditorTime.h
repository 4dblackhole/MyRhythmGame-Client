#pragma once
#include "Timing/MusicalTimeline.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

inline RhythmTime Microseconds(double milliseconds)
{
    using Time = RhythmTime;
    const long double value = static_cast<long double>(milliseconds) * 1000;
    if (!std::isfinite(value) || value <= std::numeric_limits<Time::rep>::lowest() + 1.0L ||
        value >= std::numeric_limits<Time::rep>::max() - 1.0L)
        throw std::invalid_argument("Editor time must fit the rhythm clock's microsecond range.");
    return Time{static_cast<Time::rep>(std::llround(value))};
}

inline std::int64_t MeasureNearTime(const MusicalTimeline &timeline, double milliseconds)
{
    static_cast<void>(Microseconds(milliseconds));
    // Bound the exponential search without evaluating an unnecessarily distant
    // timestamp (very low BPM can overflow the clock at the final measure).
    std::int64_t low = 0, high = 1;
    const auto at = [&](std::int64_t measure) { return timeline.MeasureSpan(measure).begin.count() / 1000.0; };
    while (at(high) <= milliseconds)
    {
        if (high == MaximumMeasureCount)
            throw std::invalid_argument("Editor time is beyond 100,000 measures.");
        low = high;
        high = std::min(high * 2, MaximumMeasureCount);
    }
    while (low + 1 < high)
    {
        const auto middle = low + (high - low) / 2;
        if (at(middle) <= milliseconds)
            low = middle;
        else
            high = middle;
    }
    return low;
}
