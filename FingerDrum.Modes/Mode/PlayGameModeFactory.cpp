#include "Mode/PlayGameModeFactory.h"
#include "Taiko/TaikoMode.h"
#include <stdexcept>

namespace finger_drum::mode
{
    std::vector<ModeInputLayout> AvailableInputLayouts()
    {
        return {TaikoMode{}.InputLayout()};
    }
    std::unique_ptr<IPlayGameMode> CreatePlayGameMode(std::string_view id, PlaySessionOptions options)
    {
        if (id == "Taiko")
            return std::make_unique<TaikoMode>(std::move(options));
        throw std::invalid_argument("Unsupported game mode: " + std::string(id));
    }
} // namespace finger_drum::mode
