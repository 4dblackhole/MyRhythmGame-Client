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
        std::wstring_view skinSaveError;
    };

    [[nodiscard]] const OptionTextSet &Options(Language language) noexcept;
} // namespace finger_drum::texts
