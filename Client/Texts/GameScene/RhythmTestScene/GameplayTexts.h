#pragma once

#include "Texts/TextCatalog.h"

#include <string_view>

struct GameplayTextSet
{
    std::wstring_view errorPrefix;
    std::wstring_view audioInitializationFailed;
    std::wstring_view focusNone;
    std::wstring_view focusPrefix;
    std::wstring_view lastFormat;
    std::wstring_view lastNone;
    std::wstring_view early;
    std::wstring_view late;
};

[[nodiscard]] GameplayTextSet Gameplay(const TextCatalog &texts);
