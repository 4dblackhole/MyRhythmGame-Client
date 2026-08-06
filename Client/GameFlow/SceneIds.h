#pragma once

#include <string_view>

// Game-specific route IDs belong to the Client. SceneManager stores and
// resolves them, while individual Scenes only select a route by this catalog.
namespace game::scene_ids
{
    inline constexpr std::string_view ColoredCube = "Cube";
    inline constexpr std::string_view Blank = "Blank";
    inline constexpr std::string_view MeshExample = "Example.Mesh";
    inline constexpr std::string_view CollisionExample = "Example.Collision";
    inline constexpr std::string_view WidgetExample = "Example.Widgets";
}
