#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>

namespace finger_drum::rhythm
{
    using RhythmTime = std::chrono::microseconds;
    using RhythmDuration = std::chrono::microseconds;
    using NoteId = std::uint64_t;
    using NoteAction = std::uint32_t;
    using PhysicalKey = std::uint16_t;
    using SoundId = std::string;
    using AudioBusId = std::string;
}
