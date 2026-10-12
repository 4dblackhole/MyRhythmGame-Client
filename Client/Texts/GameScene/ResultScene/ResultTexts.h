#pragma once
#include "Texts/TextCatalog.h"
#include <array>

namespace finger_drum::texts
{
    struct ResultTextSet
    {
        std::wstring_view title, completed, interrupted, totalAccuracy, noteCounts;
        std::array<std::wstring_view, 3> groups;
        std::wstring_view groupFormat, timingStatistics, noTimingData, timeGraph, histogram;
        std::wstring_view timeAxis, errorAxis, back, inputCount, judgementHelp, tickCounts;
    };
    const ResultTextSet &Result(Language language) noexcept;
} // namespace finger_drum::texts
