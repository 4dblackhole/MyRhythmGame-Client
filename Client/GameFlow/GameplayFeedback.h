#pragma once
#include "Note/Submodules/NoteTypes.h"

struct GameplayFeedback
{
    bool reset{};
    bool keyPressed{};
    NoteProcessResult result;
};
