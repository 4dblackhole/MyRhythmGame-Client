#pragma once

#include "Mode/PlayGameMode.h"
#include "Taiko/Submodules/TaikoNoteDefinition.h"
#include "Taiko/Submodules/TaikoSoundIds.h"

#include <array>
#include <cstddef>

namespace finger_drum::mode
{
    class TaikoMode final : public IPlayGameMode
    {
      public:
        [[nodiscard]] std::string_view Id() const noexcept override;
        [[nodiscard]] ModeLoadResult LoadSession(
            const std::filesystem::path &patternPath,
            const std::optional<std::filesystem::path> &effectPath = std::nullopt) const override;
        [[nodiscard]] ModeLoadResult CreateSession(
            const chart::PatternDocument &pattern,
            const chart::EffectDocument &effects = {}) const override;
    };
} // namespace finger_drum::mode
