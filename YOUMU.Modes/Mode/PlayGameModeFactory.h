#pragma once
#include "Mode/PlayGameMode.h"

[[nodiscard]] std::vector<ModeInputLayout> AvailableInputLayouts();
[[nodiscard]] std::unique_ptr<IPlayGameMode> CreatePlayGameMode(std::string_view id,
                                                                PlaySessionOptions options = {});
