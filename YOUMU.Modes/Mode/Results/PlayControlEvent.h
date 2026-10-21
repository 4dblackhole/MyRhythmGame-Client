#pragma once
#include "Common/RhythmTypes.h"
#include <cstddef>

enum class PlayControlType
{
    Pause,
    Resume,
    Rewind,
    Retire
};
// Ordered markers for a future replay view. Chart time can move backwards;
// inputIndex ties each marker to the append-only physical input history.
struct PlayControlEvent
{
    PlayControlType type{};
    RhythmTime from{}, to{};
    std::size_t inputIndex{};
    bool pauseMenuShown{};
};
