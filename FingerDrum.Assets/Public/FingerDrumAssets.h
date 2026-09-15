#pragma once

#include <filesystem>
#include <string>

namespace finger_drum::assets
{
    inline constexpr int BuiltInAssetResourceId = 101;

    // Installs the executable's RCDATA pack into a versioned per-user cache.
    // Calling this more than once is safe. It must succeed before resolving
    // any built-in asset paths.
    [[nodiscard]] bool InitializeBuiltInAssets(
        std::string& errorMessage) noexcept;

    [[nodiscard]] bool BuiltInAssetsInitialized() noexcept;
    [[nodiscard]] std::filesystem::path BuiltInAssetRoot();
    [[nodiscard]] std::filesystem::path BuiltInSongsRoot();
    [[nodiscard]] std::filesystem::path UserSongsRoot();

    [[nodiscard]] std::filesystem::path ResolveBuiltInAsset(
        const std::filesystem::path& relativePath);

    // Returns a user-provided file when it exists, otherwise the matching
    // file from the embedded Default Skin. The fallback is selected per file
    // so partial user skins remain usable.
    [[nodiscard]] std::filesystem::path ResolveSkinAsset(
        const std::filesystem::path& preferredSkinRoot,
        const std::filesystem::path& relativePath);
    [[nodiscard]] std::filesystem::path ResolveDefaultSkinAsset(
        const std::filesystem::path& relativePath);
}
