#pragma once

#include <string_view>

// Scene route IDs are Client-owned game data.
namespace finger_drum::scene_ids
{
    inline constexpr std::string_view Logo = "FingerDrum.Logo";
    inline constexpr std::string_view Lobby = "FingerDrum.Lobby";
    inline constexpr std::string_view EditorSongSelect =
        "FingerDrum.EditorSongSelect";
    inline constexpr std::string_view Editor = "FingerDrum.Editor";
    inline constexpr std::string_view RhythmTest = "FingerDrum.RhythmTest";
}
