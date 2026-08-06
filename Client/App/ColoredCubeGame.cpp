#include "ColoredCubeGame.h"

#include "GameFlow/SceneIds.h"
#include "GameScene/BlankScene.h"
#include "GameScene/ColoredCubeScene.h"
#include "GameScene/Examples/CollisionExampleScene.h"
#include "GameScene/Examples/MeshExampleScene.h"
#include "GameScene/Examples/WidgetExampleScene.h"

#include <stdexcept>
#include <string>
#include <utility>

ColoredCubeGame::ColoredCubeGame(
    const bool smokeTest,
    const bool showPerformanceOverlay,
    std::string initialSceneId) noexcept
    : smokeTest_(smokeTest),
      showPerformanceOverlay_(showPerformanceOverlay),
      initialSceneId_(std::move(initialSceneId))
{
}

mrg::EngineConfig ColoredCubeGame::GetEngineConfig() const
{
    mrg::EngineConfig config;
    config.windowTitle = L"MyRhythmGame - DX12 scene framework";
    config.windowWidth = 1280;
    config.windowHeight = 720;
    config.clearColor = {
        240.0F / 255.0F,
        248.0F / 255.0F,
        1.0F,
        1.0F};
    config.audio.preferredBackend =
        mrg::audio::AudioOutputBackend::Automatic;
    config.audio.fallBackToWasapi = true;
    config.audio.allowNoSoundFallback = true;
    config.performanceOverlay.framesPerSecondFontFile =
        L"assets\\fonts\\PressStart2P-Regular.ttf";
    config.performanceOverlay.updatesPerSecondFontFile =
        L"assets\\fonts\\Rajdhani-SemiBold.ttf";
    config.performanceOverlay.framesPerSecondFontSizePixels = 15.0F;
    config.performanceOverlay.updatesPerSecondFontSizePixels = 23.0F;
    config.performanceOverlay.framesPerSecondColor =
        {0.10F, 0.36F, 0.92F, 1.0F};
    config.performanceOverlay.updatesPerSecondColor =
        {0.90F, 0.18F, 0.38F, 1.0F};
    config.performanceOverlay.initiallyVisible =
        smokeTest_ || showPerformanceOverlay_;
    config.showWindow = !smokeTest_;
    config.autoExitAfterRenderedFrames = smokeTest_ ? 3 : 0;
    return config;
}

void ColoredCubeGame::RegisterScenes(mrg::scene::SceneManager& scenes)
{
    using mrg::scene::SceneRetention;

    // SceneManager centrally owns every route and its lifetime policy. The
    // factories create objects lazily when ChangeScene enters their route.
    if (!scenes.RegisterScene<ColoredCubeScene>(
            std::string(game::scene_ids::ColoredCube),
            SceneRetention::KeepAlive,
            smokeTest_) ||
        !scenes.RegisterScene<BlankScene>(
            std::string(game::scene_ids::Blank),
            SceneRetention::KeepAlive) ||
        !scenes.RegisterScene<MeshExampleScene>(
            std::string(game::scene_ids::MeshExample),
            SceneRetention::DestroyOnExit) ||
        !scenes.RegisterScene<CollisionExampleScene>(
            std::string(game::scene_ids::CollisionExample),
            SceneRetention::DestroyOnExit) ||
        !scenes.RegisterScene<WidgetExampleScene>(
            std::string(game::scene_ids::WidgetExample),
            SceneRetention::DestroyOnExit))
    {
        throw std::runtime_error("Failed to register the Client Scene routes.");
    }
}

std::string_view ColoredCubeGame::InitialSceneId() const noexcept
{
    return initialSceneId_;
}
