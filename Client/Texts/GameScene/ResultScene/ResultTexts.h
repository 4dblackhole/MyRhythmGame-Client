#pragma once
#include "Texts/TextCatalog.h"
#include <array>
namespace finger_drum::texts
{
    struct ResultTextSet
    {
        std::wstring_view accuracy, percent, noteCounts;
        std::array<std::wstring_view, 3> groups;
        std::wstring_view hpGraph, hpFuture, noHpData, timeAxis, start, end;
        std::wstring_view judgements, ticks, tickFormat, groupTitle;
        std::wstring_view rank, score, song, pattern, creator;
        std::wstring_view histogram, peakFormat, frequency, errorAxis, early, late;
        std::wstring_view scatter, mean, standardDeviation, variance, noTimingData;
        std::wstring_view replay, retry, back, backShortcut;
        std::wstring_view artist;
    };
    ResultTextSet Result(const TextCatalog &texts);
} // namespace finger_drum::texts
