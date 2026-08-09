#pragma once

#include <string_view>

// Scene route IDs are Client-owned game data. The initial FingerDrum route
// deliberately does not share the archived ColoredCube example identifiers.
namespace finger_drum::scene_ids
{
    inline constexpr std::string_view Logo = "FingerDrum.Logo";
    inline constexpr std::string_view Lobby = "FingerDrum.Lobby";
}
