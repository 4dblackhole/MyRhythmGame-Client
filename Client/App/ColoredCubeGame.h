#pragma once


#include "MRG_Core.h"

#include <string_view>

class ColoredCubeGame final : public mrg::scene::SceneGameClient
{
public:
    explicit ColoredCubeGame(
        bool smokeTest,
        bool showPerformanceOverlay = false) noexcept;

    [[nodiscard]] mrg::EngineConfig GetEngineConfig() const override;

protected:
    void RegisterScenes(mrg::scene::SceneManager& scenes) override;
    [[nodiscard]] std::string_view InitialSceneId() const noexcept override;

private:
    bool smokeTest_{};
    bool showPerformanceOverlay_{};
};
