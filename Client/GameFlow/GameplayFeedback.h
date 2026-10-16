#pragma once
#include "Note/Submodules/NoteTypes.h"

struct GameplayFeedback
{
    bool reset{};
    bool keyPressed{};
    finger_drum::rhythm::NoteProcessResult result;
};
