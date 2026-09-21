#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace finger_drum
{
    // Value snapshot for one scene entry. Shared transfer is encapsulated by
    // GameplayLaunchStore; an active session owns its copy.
    struct GameplayLaunchRequest
    {
        std::filesystem::path patternPath;
        std::optional<std::filesystem::path> effectPath;
        std::filesystem::path musicPath;
        std::string mode;

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
        }
    };
} // namespace finger_drum
