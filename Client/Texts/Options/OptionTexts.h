#pragma once

#include "Texts/TextCatalog.h"

#include <string_view>

namespace finger_drum::texts
{
    struct OptionTextSet
    {
        std::wstring_view title;
        std::wstring_view language;
        std::wstring_view skin;
        std::wstring_view middleware;
        std::wstring_view output;
        std::wstring_view driver;
        std::wstring_view automatic;
        std::wstring_view noDrivers;
        std::wstring_view noSound;
        std::wstring_view saveError;
        std::wstring_view audioError;
        std::wstring_view outputFallback;
        std::wstring_view driverUnavailable;
    };

    [[nodiscard]] const OptionTextSet &Options(Language language) noexcept;
} // namespace finger_drum::texts
