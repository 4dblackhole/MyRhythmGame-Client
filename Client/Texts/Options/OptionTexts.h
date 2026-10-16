#pragma once

#include "Texts/TextCatalog.h"

#include <string_view>

namespace finger_drum::texts
{
    struct OptionTextSet
    {
        std::wstring_view resolution, globalOffset, offsetHint, keys, primary, auxiliary, pressKey;
        std::wstring_view windowError, inputError;
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
        std::wstring_view loadWarning;
        std::wstring_view middlewareFmod, outputWasapi, outputAsio;
    };

    [[nodiscard]] std::wstring_view InputBindingName(const TextCatalog &texts, std::string_view mode,
                                                     std::string_view id) noexcept;
    [[nodiscard]] OptionTextSet Options(const TextCatalog &texts);
} // namespace finger_drum::texts
