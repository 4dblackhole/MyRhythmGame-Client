#include "FingerDrumGame.h"

#include "GameFlow/FingerDrumSceneIds.h"
#include "GameScene/EditorScene.h"
#include "GameScene/FingerDrumLogoScene.h"
#include "GameScene/MusicSelectScene.h"
#include "GameScene/RhythmTestScene.h"

#include <Windows.h>

#include <algorithm>
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>

FingerDrumGame::FingerDrumGame(
    const bool smokeTest,
    std::string initialSceneId,
    const bool rhythmDebugMode) noexcept
    : smokeTest_(smokeTest),
      showPerformanceOverlay_(smokeTest),
      rhythmDebugMode_(rhythmDebugMode),
      initialSceneId_(std::move(initialSceneId)),
      launchRequest_(
          std::make_shared<finger_drum::GameplayLaunchRequest>())
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
            mrg::scene::SceneRetention::KeepAlive,
            std::ref(ScreenVisuals())) ||
        !scenes.RegisterScene<MusicSelectScene>(
            std::string(finger_drum::scene_ids::Lobby),
            mrg::scene::SceneRetention::KeepAlive,
            std::ref(ScreenVisuals()),
            std::ref(AudioPlayback()),
            launchRequest_) ||
        !scenes.RegisterScene<MusicSelectScene>(
            std::string(finger_drum::scene_ids::EditorSongSelect),
            mrg::scene::SceneRetention::KeepAlive,
            std::ref(ScreenVisuals()),
            std::ref(AudioPlayback()),
            launchRequest_,
            SongSelectPurpose::Editor) ||
        !scenes.RegisterScene<EditorScene>(
            std::string(finger_drum::scene_ids::Editor),
            mrg::scene::SceneRetention::DestroyOnExit) ||
        !scenes.RegisterScene<RhythmTestScene>(
            std::string(finger_drum::scene_ids::RhythmTest),
            // Gameplay routes keep only their factory while inactive. The
            // concrete mode Scene is constructed on entry and destroyed as
            // soon as it returns to Lobby.
            mrg::scene::SceneRetention::DestroyOnExit,
            launchRequest_,
            std::ref(AudioPlayback()),
            std::ref(ScreenVisuals()),
            rhythmDebugMode_))
    {
        throw std::runtime_error("Failed to register the FingerDrum Scenes.");
    }
}

std::string_view FingerDrumGame::InitialSceneId() const noexcept
{
    return initialSceneId_;
}

void FingerDrumGame::OnClientInitialized(
    const mrg::EngineServices& services)
{
    // Performance text is Client presentation, so the game owns the font and
    // chooses F7 independently from Engine scheduling.
    performanceFont_ = services.textRendering.LoadFontFile(
        mrg::platform::ResolveExecutableRelativePath(
            L"assets\\fonts\\Rajdhani-SemiBold.ttf"));
}

void FingerDrumGame::OnClientUpdated(const mrg::UpdateContext& context)
{
    if (context.input.WasKeyPressed(VK_F7))
    {
        showPerformanceOverlay_ = !showPerformanceOverlay_;
    }
    if (!context.performance.hasMeasurement ||
        context.performance.measurementIndex ==
            lastPerformanceMeasurementIndex_)
    {
        return;
    }
    RefreshPerformanceText(context.performance);
    lastPerformanceMeasurementIndex_ = context.performance.measurementIndex;
}

void FingerDrumGame::OnClientRendered(
    const mrg::graphics::RenderContext& context)
{
    if (!showPerformanceOverlay_ || performanceFont_ == nullptr ||
        context.textRendering == nullptr)
    {
        return;
    }

    constexpr float bottomMargin = 18.0F;
    constexpr float lineHeight = 26.0F;
    const float viewportHeight = static_cast<float>(context.height);
    const float upsY = std::max(
        viewportHeight - bottomMargin - lineHeight,
        0.0F);
    SubmitPerformanceLine(
        *context.textRendering,
        framesPerSecondText_,
        performanceFont_,
        std::max(upsY - lineHeight, 0.0F),
        static_cast<float>(context.width));
    SubmitPerformanceLine(
        *context.textRendering,
        updatesPerSecondText_,
        performanceFont_,
        upsY,
        static_cast<float>(context.width));
}

void FingerDrumGame::OnClientShuttingDown() noexcept
{
    performanceFont_.reset();
}

void FingerDrumGame::RefreshPerformanceText(
    const mrg::PerformanceStatistics& performance)
{
    framesPerSecondText_ =
        L"FPS  " + std::to_wstring(performance.framesPerSecond);
    updatesPerSecondText_ =
        L"UPS  " + std::to_wstring(performance.updatesPerSecond);
}

void FingerDrumGame::SubmitPerformanceLine(
    mrg::graphics::TextRenderSystem& textRendering,
    const std::wstring_view text,
    const mrg::graphics::FontHandle& font,
    const float layoutY,
    const float viewportWidth)
{
    constexpr float rightMargin = 18.0F;
    constexpr float layoutWidth = 240.0F;
    constexpr float lineHeight = 26.0F;
    mrg::graphics::TextDrawCommand command;
    command.positionPixels = {
        std::max(viewportWidth - rightMargin - layoutWidth, 0.0F),
        layoutY};
    command.layoutSizePixels = {
        std::min(layoutWidth, std::max(viewportWidth - rightMargin, 1.0F)),
        lineHeight};
    command.horizontalAlignment =
        mrg::graphics::TextHorizontalAlignment::Trailing;
    command.verticalAlignment =
        mrg::graphics::TextVerticalAlignment::Center;
    command.style.font = font;
    command.style.fontSizePixels = 19.0F;

    command.positionPixels.x += 1.5F;
    command.positionPixels.y += 1.5F;
    command.style.color = {0.0F, 0.0F, 0.0F, 0.70F};
    textRendering.Submit(text, command);
    command.positionPixels.x -= 1.5F;
    command.positionPixels.y -= 1.5F;
    command.style.color = {0.15F, 0.48F, 0.82F, 1.0F};
    textRendering.Submit(text, command);
}
