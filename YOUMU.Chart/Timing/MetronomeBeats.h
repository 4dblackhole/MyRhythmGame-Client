#pragma once
#include "Timing/MusicalTimeline.h"
#include <algorithm>
#include <stdexcept>
struct MetronomeBeat
{
    RhythmTime time;
    bool accent{};
};
inline std::vector<MetronomeBeat> CompileQuarterBeats(const MusicalTimeline &timeline, std::int64_t lastMeasure,
                                                      RhythmTime end)
{
    if (lastMeasure < 0 || lastMeasure > 100'000)
        throw std::invalid_argument("Metronome measure range is invalid.");
    std::vector<MetronomeBeat> beats;
    for (std::int64_t measure = 0; measure <= lastMeasure; ++measure)
    {
        const auto length = timeline.MeasureLength(measure);
        const auto origin = timeline.PositionToWholeNotes({measure, {}});
        for (Rational offset{}; offset < length; offset = offset + Rational{1, 4})
        {
            const auto position = timeline.PositionAtWholeNotes(origin + offset);
            if (!timeline.IsPositionActive(position))
                continue;
            const auto time = timeline.Compile(position);
            if (time <= end)
                beats.push_back({time, offset == Rational{}});
        }
    }
    std::ranges::stable_sort(beats, {}, &MetronomeBeat::time);
    return beats;
}
