#include "ColoredCubeGame.h"

#include "App/AssetPaths.h"
#include "Examples/ColoredCube/GameFlow/SceneIds.h"
#include "Examples/ColoredCube/GameScene/BlankScene.h"
#include "Examples/ColoredCube/GameScene/ColoredCubeScene.h"
#include "Examples/ColoredCube/GameScene/Examples/CollisionExampleScene.h"
#include "Examples/ColoredCube/GameScene/Examples/MeshExampleScene.h"
#include "Examples/ColoredCube/GameScene/Examples/WidgetExampleScene.h"

#include <algorithm>
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>

ColoredCubeGame::ColoredCubeGame(
    const bool smokeTest,
    const bool showPerformanceOverlay,
    std::string initialSceneId) noexcept
    : smokeTest_(smokeTest),
      showPerformanceOverlay_(smokeTest || showPerformanceOverlay),
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
            SceneRetention::DestroyOnExit,
            std::ref(ScreenVisuals())))
    {
        throw std::runtime_error("Failed to register the Client Scene routes.");
    }
}

std::string_view ColoredCubeGame::InitialSceneId() const noexcept
{
    return initialSceneId_;
}

void ColoredCubeGame::OnClientInitialized(
    const mrg::EngineServices& services)
{
    // The Client deliberately owns its presentation resources. The Engine
    // supplies TextRenderSystem as a service, but does not choose fonts or
    // draw a game-specific performance overlay.
    framesPerSecondFont_ = services.textRendering.LoadFontFile(
        mrg::platform::ResolveExecutableRelativePath(
            mrg_client::asset_paths::fonts::ExamplePixel));
    updatesPerSecondFont_ = services.textRendering.LoadFontFile(
        mrg::platform::ResolveExecutableRelativePath(
            mrg_client::asset_paths::fonts::FingerDrum));
    if (smokeTest_)
    {
        BeginAudioPlaybackSmokeCheck(services);
    }
}

void ColoredCubeGame::BeginAudioPlaybackSmokeCheck(
    const mrg::EngineServices& services)
{
    std::string error;
    std::shared_ptr<mrg::audio::AudioClip> clip = services.audio.LoadSound(
        mrg::platform::ResolveExecutableRelativePath(
            mrg_client::asset_paths::unused_examples::PopSound),
        mrg::audio::AudioLoadMode::Sample,
        error);
    if (clip == nullptr)
    {
        throw std::runtime_error("Audio smoke load failed: " + error);
    }
    mrg::audio::AudioPlaybackSettings settings;
    settings.startPaused = true;
    audioSmokePlayback_ = AudioPlayback().Play(clip, settings, nullptr, error);
    if (audioSmokePlayback_ == mrg::audio::InvalidAudioPlaybackId)
    {
        throw std::runtime_error("Audio smoke setup failed: " + error);
    }
    // Only the game-wide playback manager retains the clip after this returns.
}

void ColoredCubeGame::CompleteAudioPlaybackSmokeCheck()
{
    if (audioSmokePlayback_ == mrg::audio::InvalidAudioPlaybackId)
    {
        return;
    }
    if (!audioSmokeSourceRendered_)
    {
        return;
    }
    if (!audioSmokeTransitionRequested_)
    {
        if (!Scenes().ChangeScene(game::scene_ids::Blank))
        {
            throw std::runtime_error("Audio smoke Scene transition failed.");
        }
        audioSmokeTransitionRequested_ = true;
        return;
    }
    auto* voice = AudioPlayback().FindVoice(audioSmokePlayback_);
    if (Scenes().CurrentSceneId() != game::scene_ids::Blank ||
        voice == nullptr || !voice->IsPlaying())
    {
        throw std::runtime_error("Managed audio did not survive the Scene transition.");
    }
    std::string error;
    const std::size_t playbackCount = AudioPlayback().PlaybackCount();
    mrg::audio::AudioPlaybackSettings restartSettings;
    if (!AudioPlayback().Restart(
            audioSmokePlayback_, restartSettings, nullptr, error) ||
        AudioPlayback().PlaybackCount() != playbackCount ||
        !AudioPlayback().Stop(audioSmokePlayback_, error) ||
        AudioPlayback().FindVoice(audioSmokePlayback_) != nullptr)
    {
        throw std::runtime_error("Audio smoke playback control failed: " + error);
    }
    audioSmokePlayback_ = mrg::audio::InvalidAudioPlaybackId;
}

void ColoredCubeGame::OnClientUpdated(const mrg::UpdateContext& context)
{
    CompleteAudioPlaybackSmokeCheck();
    // Visibility is a game policy: F1 affects only this Client's overlay,
    // not Engine scheduling or the performance measurement itself.
    if (context.input.WasKeyPressed(VK_F1))
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
    lastPerformanceMeasurementIndex_ =
        context.performance.measurementIndex;
}

void ColoredCubeGame::OnClientRendered(
    const mrg::graphics::RenderContext& context)
{
    audioSmokeSourceRendered_ = true;
    if (!showPerformanceOverlay_ || context.textRendering == nullptr ||
        framesPerSecondFont_ == nullptr || updatesPerSecondFont_ == nullptr)
    {
        return;
    }

    // Keep the layout independent from a Scene's camera and leave room for
    // the window edge, regardless of the current render-target size.
    constexpr float rightMarginPixels = 20.0F;
    constexpr float bottomMarginPixels = 20.0F;
    constexpr float lineGapPixels = 4.0F;
    constexpr float layoutWidthPixels = 300.0F;
    constexpr float framesPerSecondFontSizePixels = 15.0F;
    constexpr float updatesPerSecondFontSizePixels = 23.0F;
    constexpr DirectX::XMFLOAT4 framesPerSecondColor{
        0.10F, 0.36F, 0.92F, 1.0F};
    constexpr DirectX::XMFLOAT4 updatesPerSecondColor{
        0.90F, 0.18F, 0.38F, 1.0F};

    const float viewportWidth = static_cast<float>(context.width);
    const float viewportHeight = static_cast<float>(context.height);
    const float availableWidth = std::max(
        viewportWidth - rightMarginPixels,
        1.0F);
    const float layoutWidth = std::min(layoutWidthPixels, availableWidth);
    const float layoutX = std::max(availableWidth - layoutWidth, 0.0F);
    const float framesLineHeight = framesPerSecondFontSizePixels * 1.5F;
    const float updatesLineHeight = updatesPerSecondFontSizePixels * 1.5F;
    const float updatesY = std::max(
        viewportHeight - bottomMarginPixels - updatesLineHeight,
        0.0F);
    const float framesY = std::max(
        updatesY - lineGapPixels - framesLineHeight,
        0.0F);

    SubmitPerformanceLine(
        *context.textRendering,
        framesPerSecondText_,
        framesPerSecondFont_,
        framesPerSecondFontSizePixels,
        framesPerSecondColor,
        layoutX,
        framesY,
        layoutWidth,
        framesLineHeight);
    SubmitPerformanceLine(
        *context.textRendering,
        updatesPerSecondText_,
        updatesPerSecondFont_,
        updatesPerSecondFontSizePixels,
        updatesPerSecondColor,
        layoutX,
        updatesY,
        layoutWidth,
        updatesLineHeight);
}

void ColoredCubeGame::OnClientShuttingDown() noexcept
{
    // Font data belongs to the TextRenderSystem, so release Client handles
    // while EngineServices are still valid.
    updatesPerSecondFont_.reset();
    framesPerSecondFont_.reset();
}

void ColoredCubeGame::RefreshPerformanceText(
    const mrg::PerformanceStatistics& performance)
{
    framesPerSecondText_ =
        L"FPS: " + std::to_wstring(performance.framesPerSecond);
    updatesPerSecondText_ =
        L"UPS: " + std::to_wstring(performance.updatesPerSecond);
}

void ColoredCubeGame::SubmitPerformanceLine(
    mrg::graphics::TextRenderSystem& textRendering,
    const std::wstring_view text,
    const mrg::graphics::FontHandle& font,
    const float fontSizePixels,
    const DirectX::XMFLOAT4& color,
    const float layoutX,
    const float layoutY,
    const float layoutWidth,
    const float lineHeight)
{
    mrg::graphics::TextDrawCommand command;
    command.positionPixels = {layoutX, layoutY};
    command.layoutSizePixels = {layoutWidth, lineHeight};
    command.horizontalAlignment =
        mrg::graphics::TextHorizontalAlignment::Trailing;
    command.verticalAlignment =
        mrg::graphics::TextVerticalAlignment::Center;
    command.style.font = font;
    command.style.fontSizePixels = fontSizePixels;

    // A small shadow keeps the Client-selected colors legible over bright
    // and textured Scene content.
    command.positionPixels.x += 1.5F;
    command.positionPixels.y += 1.5F;
    command.style.color = {0.0F, 0.0F, 0.0F, 0.65F};
    textRendering.Submit(text, command);

    command.positionPixels.x -= 1.5F;
    command.positionPixels.y -= 1.5F;
    command.style.color = color;
    textRendering.Submit(text, command);
}
