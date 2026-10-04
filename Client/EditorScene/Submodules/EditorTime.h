#pragma once
#include "Timing/MusicalTimeline.h"
#include <cmath>
#include <limits>
#include <stdexcept>

namespace editor_time
{
    inline finger_drum::rhythm::RhythmTime Microseconds(double milliseconds)
    {
        using Time = finger_drum::rhythm::RhythmTime;
        const long double value = static_cast<long double>(milliseconds) * 1000;
        if (!std::isfinite(value) || value <= std::numeric_limits<Time::rep>::lowest() + 1.0L ||
            value >= std::numeric_limits<Time::rep>::max() - 1.0L)
            throw std::invalid_argument("Editor time must fit the rhythm clock's microsecond range.");
        return Time{static_cast<Time::rep>(std::llround(value))};
    }

    inline std::int64_t MeasureNearTime(const finger_drum::chart::MusicalTimeline &timeline, double milliseconds)
    {
        static_cast<void>(Microseconds(milliseconds));
        // Preserve the existing seek range and prefix-sum search, including pre-roll.
        std::int64_t low = 0, high = 1;
        const auto at = [&](std::int64_t measure) { return timeline.Compile({measure, {}}).count() / 1000.0; };
        while (at(high) <= milliseconds)
        {
            if (high > 1'000'000)
                throw std::invalid_argument("Editor time is beyond the supported measure range.");
            high *= 2;
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
} // namespace editor_time
