#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace finger_drum
{
    // Lobby writes the selected chart into this small game-flow object before
    // requesting the gameplay route. SceneManager's transient factory captures
    // the shared object, then constructs a fresh gameplay Scene on each entry.
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
}
