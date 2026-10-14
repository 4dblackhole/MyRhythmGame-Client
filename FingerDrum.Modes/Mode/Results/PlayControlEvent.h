#pragma once
#include "Common/RhythmTypes.h"
#include <cstddef>

namespace finger_drum::mode
{
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
        rhythm::RhythmTime from{}, to{};
        std::size_t inputIndex{};
        bool pauseMenuShown{};
    };
} // namespace finger_drum::mode
