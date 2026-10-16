#pragma once
#include "Texts/TextCatalog.h"
#include <array>
namespace finger_drum::texts
{
    struct PauseTextSet
    {
        std::wstring_view title, practice, controls;
        std::array<std::wstring_view, 4> actions;
        std::wstring_view resumeFormat, rewindFormat, retry, retire, preparing;
    };
    PauseTextSet Pause(const TextCatalog &texts);
} // namespace finger_drum::texts
