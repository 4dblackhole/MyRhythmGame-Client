#pragma once

#include "MRG_Core.h"

#include <string_view>

// The Client root for FingerDrum.  The Engine owns the platform loop while
// this class selects the game window policy and registers game-owned Scenes.
class FingerDrumGame final : public mrg::scene::SceneGameClient
{
public:
    explicit FingerDrumGame(bool smokeTest) noexcept;

    [[nodiscard]] mrg::EngineConfig GetEngineConfig() const override;

protected:
    void RegisterScenes(mrg::scene::SceneManager& scenes) override;
    [[nodiscard]] std::string_view InitialSceneId() const noexcept override;

private:
    bool smokeTest_{};
};
