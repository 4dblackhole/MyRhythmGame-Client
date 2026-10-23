#include "YOUMUGame.h"

#include "App/AssetPaths.h"
#include "Catalog/SongCatalog.h"
#include "EditorScene/EditorScene.h"
#include "GameFlow/YOUMUSceneIds.h"
#include "GameScene/YOUMULogoScene/YOUMULogoScene.h"
#include "GameScene/MusicSelectScene/MusicSelectScene.h"
#include "GameScene/ResultScene/ResultScene.h"
#include "GameScene/RhythmTestScene/RhythmTestScene.h"
#include "Presentation/UiColors.h"
#include "Texts/GameIdentity.h"
#include "Records/PlayRecordStore.h"

#include <Windows.h>

#include <algorithm>
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>

YOUMUGame::YOUMUGame(const bool smokeTest, std::string initialSceneId,
                               const bool rhythmDebugMode)
    : smokeTest_(smokeTest), showPerformanceOverlay_(smokeTest), rhythmDebugMode_(rhythmDebugMode),
      initialSceneId_(std::move(initialSceneId)),
      launchRequest_(std::make_shared<GameplayLaunchStore>()),
      optionSettings_(OptionSettings::NextToExecutable())
{
    std::string error;
    // Invalid/unwritable settings must not prevent startup. The options panel
    // retains the load warning until the user successfully saves new settings.
    if (!optionSettings_.Load(error))
        OutputDebugStringA(error.c_str());
    GameplayLaunchRequest initialRequest;
    initialRequest.songsRoot = optionSettings_.SongsRoot();
    launchRequest_->Set(std::move(initialRequest));
    texts_.SetLanguage(optionSettings_.Values().language);
    framesPerSecondText_ = texts_.Get("Performance.measuringFps");
    updatesPerSecondText_ = texts_.Get("Performance.measuringUps");
    for (const auto &diagnostic : texts_.Diagnostics())
        OutputDebugStringA((diagnostic + "\n").c_str());
    SkinSetSelection::Instance().Initialize(optionSettings_.Values().skinSet);
}

EngineConfig YOUMUGame::GetEngineConfig() const
{
    EngineConfig config;
    config.windowTitle = GameName;
    config.windowWidth = optionSettings_.Values().windowWidth;
    config.windowHeight = optionSettings_.Values().windowHeight;
    const auto color = ui_colors::ClearBackground;
    config.clearColor = {color.red, color.green, color.blue, color.alpha};
    config.audio.preferredBackend = optionSettings_.Values().audioOutput;
    // Restore the named device after enumeration instead of trusting a stale index.
    config.audio.driverIndex = -1;
    config.audio.fallBackToWasapi = true;
    config.audio.allowNoSoundFallback = true;
    config.showWindow = !smokeTest_;
    config.autoExitAfterRenderedFrames = smokeTest_ ? 3 : 0;
    return config;
}

void YOUMUGame::RegisterScenes(SceneManager &scenes)
{
    if (smokeTest_ && initialSceneId_ == EditorSceneId)
    {
        const auto catalog = SongCatalog{}.Load(optionSettings_.SongsRoot());
        const auto song = std::ranges::find_if(
            catalog.songs, [](const auto &entry) { return !entry.patterns.empty(); });
        if (song == catalog.songs.end())
            throw std::runtime_error("Editor smoke requires an external Songs chart.");
        GameplayLaunchRequest smokeRequest{song->patterns.front().patternPath,
                                                        song->patterns.front().effectPath, song->audioPath,
                                                        song->patterns.front().pattern.mode};
        smokeRequest.songsRoot = optionSettings_.SongsRoot();
        launchRequest_->Set(std::move(smokeRequest));
    }
    if (!scenes.RegisterScene<YOUMULogoScene>(std::string(LogoSceneId),
                                                   SceneRetention::KeepAlive, std::ref(ScreenVisuals()),
                                                   std::ref(texts_), std::ref(optionSettings_)) ||
        !scenes.RegisterScene<MusicSelectScene>(std::string(LobbySceneId),
                                                SceneRetention::KeepAlive, std::ref(ScreenVisuals()),
                                                std::ref(AudioPlayback()), launchRequest_, SongSelectPurpose::Gameplay,
                                                std::ref(texts_), std::ref(optionSettings_)) ||
        !scenes.RegisterScene<MusicSelectScene>(std::string(EditorSongSelectSceneId),
                                                SceneRetention::KeepAlive, std::ref(ScreenVisuals()),
                                                std::ref(AudioPlayback()), launchRequest_, SongSelectPurpose::Editor,
                                                std::ref(texts_), std::ref(optionSettings_)) ||
        !scenes.RegisterScene<EditorScene>(std::string(EditorSceneId),
                                           SceneRetention::DestroyOnExit, std::ref(ScreenVisuals()),
                                           launchRequest_, std::ref(texts_)) ||
        !scenes.RegisterScene<RhythmTestScene>(std::string(RhythmTestSceneId),
                                               // Gameplay routes keep only their factory while inactive. The
                                               // concrete mode Scene is constructed on entry and destroyed as
                                               // soon as it transfers its result.
                                               SceneRetention::DestroyOnExit, launchRequest_,
                                               std::ref(AudioPlayback()), std::ref(ScreenVisuals()), rhythmDebugMode_,
                                               std::ref(texts_), std::ref(pendingResult_)) ||
        !scenes.RegisterScene(std::string(ResultSceneId), SceneRetention::DestroyOnExit,
                              [this] { return CreateResultScene(); }))
    {
        throw std::runtime_error("Failed to register the game Scenes.");
    }
}

std::unique_ptr<GameScene> YOUMUGame::CreateResultScene()
{
    auto launch = launchRequest_->Snapshot();
    ResultViewContext display{launch.songTitle, launch.patternCreator, launch.IsValid(), launch.songArtist};
    if (!smokeTest_ && !rhythmDebugMode_ && launch.IsValid()) display.recordDatabase = PlayRecordStore::DefaultPath();
    ResultScene::RetryHandler retry;
    if (display.canRetry)
        retry = [store = launchRequest_, snapshot = std::move(launch)](SceneManager &scenes)
        {
            // A retry owns a fresh session; restore the played selection without
            // retaining any note, timer or voice from the completed session.
            store->Set(snapshot);
            return scenes.ChangeScene(RhythmTestSceneId);
        };
    return std::make_unique<ResultScene>(std::move(pendingResult_), ScreenVisuals(), texts_, std::move(display),
                                         std::move(retry));
}

std::string_view YOUMUGame::InitialSceneId() const noexcept
{
    return initialSceneId_;
}

void YOUMUGame::OnClientInitialized(const EngineServices &services)
{
    // This hook runs before the first update/render; output switching retains clips.
    RestoreSavedAudioDriver(services.audio);
    // Performance text is Client presentation, so the game owns the font and
    // chooses F7 independently from Engine scheduling.
    performanceFont_ =
        services.textRendering.LoadFontFile(YOUMU());
}

void YOUMUGame::OnClientUpdated(const UpdateContext &context)
{
    if (context.input.WasKeyPressed(VK_F7))
    {
        showPerformanceOverlay_ = !showPerformanceOverlay_;
    }
    if (!context.performance.hasMeasurement ||
        context.performance.measurementIndex == lastPerformanceMeasurementIndex_)
    {
        return;
    }
    RefreshPerformanceText(context.performance);
    lastPerformanceMeasurementIndex_ = context.performance.measurementIndex;
}

void YOUMUGame::OnClientRendered(const RenderContext &context)
{
    if (!showPerformanceOverlay_ || performanceFont_ == nullptr || context.textRendering == nullptr)
    {
        return;
    }

    constexpr float bottomMargin = 18.0F;
    constexpr float lineHeight = 26.0F;
    const float viewportHeight = static_cast<float>(context.height);
    const float upsY = std::max(viewportHeight - bottomMargin - lineHeight, 0.0F);
    SubmitPerformanceLine(*context.textRendering, framesPerSecondText_, performanceFont_,
                          std::max(upsY - lineHeight, 0.0F), static_cast<float>(context.width));
    SubmitPerformanceLine(*context.textRendering, updatesPerSecondText_, performanceFont_, upsY,
                          static_cast<float>(context.width));
}

void YOUMUGame::OnClientShuttingDown() noexcept
{
    performanceFont_.reset();
}

void YOUMUGame::RestoreSavedAudioDriver(AudioSystem &audio)
{
    const auto &saved = optionSettings_.Values();
    if (audio.RequestedOutput() != saved.audioOutput ||
        (saved.driverIndex < 0 && saved.driverName.empty())) return;
    const auto &drivers = audio.OutputDrivers();
    auto selected = std::ranges::find_if(drivers, [&](const auto &driver) {
        return driver.driverIndex == saved.driverIndex &&
            (saved.driverName.empty() || driver.name == saved.driverName);
    });
    if (selected == drivers.end() && !saved.driverName.empty())
        selected = std::ranges::find(drivers, saved.driverName, &AudioDeviceInfo::name);
    if (selected == drivers.end()) return; // Keep the default device when hardware is unavailable.
    std::string error;
    if (!audio.SetOutputDriver(selected->driverIndex, error)) OutputDebugStringA(error.c_str());
}

void YOUMUGame::RefreshPerformanceText(const PerformanceStatistics &performance)
{
    framesPerSecondText_ =
        std::wstring(texts_.Get("Performance.fps")) + L"  " + std::to_wstring(performance.framesPerSecond);
    updatesPerSecondText_ =
        std::wstring(texts_.Get("Performance.ups")) + L"  " + std::to_wstring(performance.updatesPerSecond);
}

void YOUMUGame::SubmitPerformanceLine(TextRenderSystem &textRendering,
                                           const std::wstring_view text,
                                           const FontHandle &font,
                                           const float layoutY, const float viewportWidth)
{
    constexpr float rightMargin = 18.0F;
    constexpr float layoutWidth = 240.0F;
    constexpr float lineHeight = 26.0F;
    TextDrawCommand command;
    command.positionPixels = {std::max(viewportWidth - rightMargin - layoutWidth, 0.0F), layoutY};
    command.layoutSizePixels = {std::min(layoutWidth, std::max(viewportWidth - rightMargin, 1.0F)),
                                lineHeight};
    command.horizontalAlignment = TextHorizontalAlignment::Trailing;
    command.verticalAlignment = TextVerticalAlignment::Center;
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
