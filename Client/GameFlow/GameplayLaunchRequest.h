#pragma once

#include "Mode/PlayOptions.h"
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

// Value snapshot for one scene entry. Shared transfer is encapsulated by
// GameplayLaunchStore; an active session owns its copy.
struct GameplayLaunchRequest
{
    std::filesystem::path patternPath;
    std::optional<std::filesystem::path> effectPath;
    std::filesystem::path musicPath;
    std::string mode;
    // Display values decoded from the same catalog selection as the paths.
    // Result/retry uses this snapshot rather than reopening metadata files.
    std::wstring songTitle;
    std::wstring patternCreator;
    // Freeze settings for this attempt, including a Result retry.
    std::int32_t globalOffsetMs{};
    std::map<std::string, std::vector<std::uint16_t>> inputKeys;
    std::wstring songArtist;
    std::filesystem::path songsRoot;
    std::filesystem::path backgroundPath;
    double backgroundOffsetX{}, backgroundOffsetY{};

    PlayOptions playOptions;

    [[nodiscard]] bool IsValid() const noexcept
    {
        return !patternPath.empty() && !musicPath.empty();
    }

    void Clear() noexcept
    {
        patternPath.clear();
        effectPath.reset();
        musicPath.clear();
        mode.clear();
        songTitle.clear();
        patternCreator.clear();
        globalOffsetMs = 0;
        playOptions = {};
        inputKeys.clear();
        songArtist.clear();
        songsRoot.clear();
        backgroundPath.clear();
        backgroundOffsetX = backgroundOffsetY = 0;
    }
};
