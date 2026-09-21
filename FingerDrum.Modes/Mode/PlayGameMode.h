#pragma once
#include "Mode/Submodules/PlaySession.h"

namespace finger_drum::mode
{
    struct ModeLoadResult
    {
        std::unique_ptr<PlaySession> session;
        std::vector<chart::Diagnostic> diagnostics;

        [[nodiscard]] bool Succeeded() const noexcept;
    };

    class IPlayGameMode
    {
      public:
        virtual ~IPlayGameMode() = default;
        [[nodiscard]] virtual std::string_view Id() const noexcept = 0;
        [[nodiscard]] virtual ModeLoadResult LoadSession(
            const std::filesystem::path &patternPath,
            const std::optional<std::filesystem::path> &effectPath = std::nullopt) const = 0;
        [[nodiscard]] virtual ModeLoadResult CreateSession(
            const chart::PatternDocument &pattern,
            const chart::EffectDocument &effects = {}) const = 0;
    };
} // namespace finger_drum::mode
