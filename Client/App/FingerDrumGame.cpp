#include "FingerDrumGame.h"

#include "GameFlow/FingerDrumSceneIds.h"
#include "GameScene/FingerDrumLogoScene.h"
#include "GameScene/LobbyScene.h"
#include "GameScene/RhythmTestScene.h"

#include <stdexcept>
#include <string>

FingerDrumGame::FingerDrumGame(const bool smokeTest) noexcept
    : smokeTest_(smokeTest)
{
}

mrg::EngineConfig FingerDrumGame::GetEngineConfig() const
{
    mrg::EngineConfig config;
    config.windowTitle = L"FingerDrum";
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
    config.showWindow = !smokeTest_;
    config.autoExitAfterRenderedFrames = smokeTest_ ? 3 : 0;
    return config;
}

void FingerDrumGame::RegisterScenes(mrg::scene::SceneManager& scenes)
{
    // FingerDrum's title Scene is intentionally registered separately from
    // the archived examples. Future title/menu routes stay in this Client
    // catalog instead of reviving the ColoredCube sample as a dependency.
    if (!scenes.RegisterScene<FingerDrumLogoScene>(
            std::string(finger_drum::scene_ids::Logo),
            mrg::scene::SceneRetention::KeepAlive) ||
        !scenes.RegisterScene<LobbyScene>(
            std::string(finger_drum::scene_ids::Lobby),
            mrg::scene::SceneRetention::KeepAlive) ||
        !scenes.RegisterScene<RhythmTestScene>(
            std::string(finger_drum::scene_ids::RhythmTest),
            // Gameplay routes keep only their factory while inactive. The
            // concrete mode Scene is constructed on entry and destroyed as
            // soon as it returns to Lobby.
            mrg::scene::SceneRetention::DestroyOnExit))
    {
        throw std::runtime_error("Failed to register the FingerDrum Scenes.");
    }
}

std::string_view FingerDrumGame::InitialSceneId() const noexcept
{
    return finger_drum::scene_ids::Logo;
}
