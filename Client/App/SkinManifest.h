#pragma once
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

// Resolved once per skin selection, never from the rendering loop. Empty
// paths represent UI elements whose default is drawn procedurally.
struct SkinImages
{
    std::map<std::string, std::filesystem::path, std::less<>> paths;
    std::vector<std::string> diagnostics;
};
[[nodiscard]] SkinImages LoadSkinImages(const std::filesystem::path &skinRoot,
                                        const std::filesystem::path &defaultSkinRoot);
[[nodiscard]] std::string SkinImageKey(std::string_view scene, std::string_view key);
