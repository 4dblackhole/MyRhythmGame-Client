#pragma once

#include "Texts/TextCatalog.h"

#include <array>
#include <string_view>

namespace finger_drum::texts
{
    struct LogoTextSet
    {
        std::array<std::wstring_view, 4> menu;
    };

    [[nodiscard]] LogoTextSet Logo(const TextCatalog &texts);
} // namespace finger_drum::texts
