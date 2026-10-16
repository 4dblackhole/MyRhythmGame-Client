#pragma once
#include "Texts/TextCatalog.h"
#include <array>
namespace finger_drum::texts
{
    struct PauseTextSet
    {
        std::array<std::wstring_view, 4> actions;
        std::wstring_view resumeValueFormat, rewindValueFormat, preparing;
    };
    PauseTextSet Pause(const TextCatalog &texts);
} // namespace finger_drum::texts
