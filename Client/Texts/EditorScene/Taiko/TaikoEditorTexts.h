#pragma once
#include "Texts/TextCatalog.h"
#include <array>
#include <string_view>
namespace finger_drum::texts
{
    struct TaikoEditorTextSet
    {
        std::array<std::wstring_view, 7> toolGroups;
        std::array<std::wstring_view, 13> toolVariants;
        std::wstring_view effectsHelp;
        std::wstring_view don;
        std::wstring_view kat;
        std::wstring_view donSound;
        std::wstring_view katSound;
    };
    const TaikoEditorTextSet &TaikoEditor(Language language) noexcept;
} // namespace finger_drum::texts
