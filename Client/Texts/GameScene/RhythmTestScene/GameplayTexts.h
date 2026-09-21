#pragma once

#include "Texts/TextCatalog.h"

#include <string_view>

namespace finger_drum::texts
{
    struct GameplayTextSet
    {
        std::wstring_view audioErrorPrefix;
        std::wstring_view audioInitializationFailed;
        std::wstring_view focusNone;
        std::wstring_view focusPrefix;
        std::wstring_view lastFormat;
        std::wstring_view lastNone;
    };

    [[nodiscard]] const GameplayTextSet &Gameplay(Language language) noexcept;
} // namespace finger_drum::texts
