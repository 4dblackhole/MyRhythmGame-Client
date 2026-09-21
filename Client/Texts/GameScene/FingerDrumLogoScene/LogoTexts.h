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

    [[nodiscard]] const LogoTextSet &Logo(Language language) noexcept;
} // namespace finger_drum::texts
