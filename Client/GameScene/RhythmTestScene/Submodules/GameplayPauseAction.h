#pragma once
#include "Common/RhythmTypes.h"
#include <cstddef>

enum class GameplayPauseAction
{
    None,
    Resume,
    Rewind,
    Retry,
    Retire
};
struct GameplayPauseChoice
{
    GameplayPauseAction action{GameplayPauseAction::None};
    RhythmDuration leadIn{500'000};
    std::size_t measures{4};
};
