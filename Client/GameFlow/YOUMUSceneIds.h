#pragma once

#include <string_view>

// Scene route IDs are Client-owned game data.

inline constexpr std::string_view LogoSceneId = "YOUMU.Logo";
inline constexpr std::string_view LobbySceneId = "YOUMU.Lobby";
inline constexpr std::string_view EditorSongSelectSceneId =
    "YOUMU.EditorSongSelect";
inline constexpr std::string_view EditorSceneId = "YOUMU.Editor";
inline constexpr std::string_view RhythmTestSceneId = "YOUMU.RhythmTest";
inline constexpr std::string_view ResultSceneId = "YOUMU.Result";
