#include "RhythmTestScene.h"

#include "GameFlow/FingerDrumSceneIds.h"
#include "Model/ChartDocument.h"
#include "Taiko/TaikoMode.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <format>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
    using finger_drum::chart::MusicalPosition;
    using finger_drum::chart::PatternNote;
    using finger_drum::chart::Rational;
    using finger_drum::rhythm::NoteState;

    constexpr mrg::visual2d::Color DonRed{0.95F, 0.25F, 0.29F, 1.0F};
    constexpr mrg::visual2d::Color KatBlue{0.18F, 0.58F, 0.95F, 1.0F};
    constexpr mrg::visual2d::Color RollGold{1.0F, 0.64F, 0.12F, 1.0F};
    constexpr std::array<mrg::visual2d::Color, 4> InputKeyBaseColors{{
        {0.04F, 0.57F, 0.81F, 1.0F},
        {0.92F, 0.07F, 0.11F, 1.0F},
        {0.92F, 0.07F, 0.11F, 1.0F},
        {0.04F, 0.57F, 0.81F, 1.0F},
    }};
    constexpr std::array<mrg::visual2d::Color, 4> InputKeyWeakColors{{
        {0.17F, 0.72F, 0.96F, 1.0F},
        {1.0F, 0.18F, 0.25F, 1.0F},
        {1.0F, 0.18F, 0.25F, 1.0F},
        {0.17F, 0.72F, 0.96F, 1.0F},
    }};
    constexpr std::array<mrg::visual2d::Color, 4> InputKeyStrongColors{{
        {0.56F, 0.90F, 1.0F, 1.0F},
        {1.0F, 0.45F, 0.49F, 1.0F},
        {1.0F, 0.45F, 0.49F, 1.0F},
        {0.56F, 0.90F, 1.0F, 1.0F},
    }};
#if defined(_DEBUG)
    // Match the former RPG project's single compile-time switch. Set this to
    // false when a Debug build should use only the real QPC/DSP timeline.
    constexpr bool ReferenceTimeDebug = true;
#else
    constexpr bool ReferenceTimeDebug = false;
#endif
    constexpr float CanvasReferenceWidth = 1280.0F;
    constexpr float CanvasReferenceHeight = 720.0F;
    constexpr float InGameAssetScale = 2.0F / 3.0F;
    constexpr float GearMargin = 20.0F;
    constexpr float GearPadding = 4.0F;
    constexpr float GearRightMargin = 0.0F;
    constexpr float LaneCenterY = 0.0F;
    constexpr float TravelDistance = 920.0F;
    constexpr finger_drum::rhythm::RhythmDuration ApproachDuration{900'000};
    constexpr finger_drum::rhythm::RhythmDuration MissedTravelDuration{
        220'000};
    constexpr finger_drum::rhythm::RhythmDuration FocusSuccessDuration{
        200'000};
    constexpr float TwoPi = 6.28318530717958647692F;

    [[nodiscard]] mrg::visual2d::Size ScaledImageSize(
        const mrg::visual2d::ScreenVisual2DManager& rendering,
        const mrg::visual2d::ImageHandle image)
    {
        const mrg::visual2d::Size imageSize = rendering.GetImageSize(image);
        if (imageSize.width <= 0.0F || imageSize.height <= 0.0F)
        {
            throw std::logic_error(
                "An in-game skin image must have a non-zero native size.");
        }
        return {
            imageSize.width * InGameAssetScale,
            imageSize.height * InGameAssetScale};
    }

    template <typename ComponentType>
    [[nodiscard]] ComponentType& RequireComponent(
        mrg::visual2d::Visual2DNode& node)
    {
        ComponentType* const component = node.GetComponent<ComponentType>();
        if (component == nullptr)
        {
            throw std::logic_error("A gameplay node is missing a component.");
        }
        return *component;
    }

    void SetColor(
        mrg::visual2d::Visual2DNode& node,
        const mrg::visual2d::Color color)
    {
        mrg::visual2d::VisualStyle style{};
        style.normal = color;
        style.hovered = color;
        style.pressed = color;
        style.disabled = {color.red, color.green, color.blue, 0.25F};
        RequireComponent<mrg::visual2d::SpriteVisualComponent>(node).
            SetStyle(style);
    }

    [[nodiscard]] std::filesystem::path SkinAssetPath(
        const std::filesystem::path& file)
    {
        return mrg::platform::ResolveExecutableRelativePath(
            std::filesystem::path(L"assets\\skins\\test Skin") / file);
    }

    [[nodiscard]] std::filesystem::path InGameSkinAssetPath(
        const std::filesystem::path& file)
    {
        return SkinAssetPath(std::filesystem::path(L"InGame") / file);
    }

    [[nodiscard]] std::filesystem::path SoundAssetPath(
        const std::filesystem::path& file)
    {
        return mrg::platform::ResolveExecutableRelativePath(
            std::filesystem::path(L"assets\\sounds") / file);
    }

    [[nodiscard]] std::wstring Utf8ToWide(const std::string_view value)
    {
        if (value.empty())
        {
            return {};
        }
        const int length = MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            value.data(),
            static_cast<int>(value.size()),
            nullptr,
            0);
        if (length <= 0)
        {
            return L"Audio initialization failed.";
        }
        std::wstring result(static_cast<std::size_t>(length), L'\0');
        MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            value.data(),
            static_cast<int>(value.size()),
            result.data(),
            length);
        return result;
    }

    void AddPatternNote(
        finger_drum::chart::PatternDocument& pattern,
        const std::int64_t measure,
        const std::int64_t numerator,
        const std::int64_t denominator,
        const finger_drum::mode::TaikoNoteType type,
        const finger_drum::mode::TaikoPatternAction action,
        std::vector<std::string> extraData = {})
    {
        PatternNote note;
        note.position = MusicalPosition{measure, Rational{numerator, denominator}};
        note.keyType = static_cast<int>(type);
        note.actionType = static_cast<int>(action);
        note.extraData = std::move(extraData);
        note.sourceOrder = pattern.notes.size();
        pattern.notes.push_back(std::move(note));
    }

    [[nodiscard]] bool IsBigVisual(const std::string_view visualId) noexcept
    {
        return visualId == "Taiko.BigDon" || visualId == "Taiko.BigKat" ||
            visualId == "Taiko.BigRoll" || visualId == "Taiko.Balloon" ||
            visualId == "Taiko.DengDeng";
    }

    [[nodiscard]] bool IsLongVisual(const std::string_view visualId) noexcept
    {
        return visualId == "Taiko.Roll" || visualId == "Taiko.BigRoll" ||
            visualId == "Taiko.Balloon" ||
            visualId == "Taiko.DengDeng" ||
            visualId == "Taiko.Buzz.Don" ||
            visualId == "Taiko.Buzz.Kat";
    }

    [[nodiscard]] mrg::visual2d::Color AmbientColor(
        const std::string_view visualId) noexcept
    {
        if (visualId == "Taiko.Kat" || visualId == "Taiko.BigKat" ||
            visualId == "Taiko.Buzz.Kat")
        {
            return KatBlue;
        }
        if (visualId == "Taiko.Roll" || visualId == "Taiko.BigRoll" ||
            visualId == "Taiko.Balloon" || visualId == "Taiko.DengDeng")
        {
            return RollGold;
        }
        return DonRed;
    }
}

RhythmTestScene::RhythmTestScene(
    std::shared_ptr<finger_drum::GameplayLaunchRequest> launchRequest,
    mrg::audio::AudioPlaybackManager& playback,
    mrg::visual2d::ScreenVisual2DManager& screenVisuals,
    const bool debugMode)
    : launchRequest_(std::move(launchRequest)),
      screenVisuals_(screenVisuals),
      audioRouter_(playback),
      debugMode_(debugMode)
{
}

void RhythmTestScene::Initialize(const mrg::EngineServices& services)
{
    if (launchRequest_ == nullptr)
    {
        throw std::invalid_argument("Gameplay requires a launch request.");
    }

    width_ = services.windowWidth;
    height_ = services.windowHeight;
    PrepareDebugLaunchRequest();
    session_ = CreateSession();
    canvasHandle_ = screenVisuals_.CreateOwnedCanvas({
        {1280.0F, 720.0F},
        mrg::visual2d::CanvasScaleMode::FixedHeight});
    canvas_ = canvasHandle_.Get();
    if (canvas_ == nullptr)
    {
        throw std::runtime_error("Failed to create the gameplay screen Canvas.");
    }
    CreatePresentation();
    CreateNoteVisuals();
    InitializeAudio(services);
    StartTimeline(services.audio.CaptureClockSnapshot());
    if (!debugMode_)
    {
        ScheduleMusic();
    }
}

void RhythmTestScene::BeginScene()
{
    static_cast<void>(canvasHandle_.SetVisible(true));
}

void RhythmTestScene::EndScene() noexcept
{
    static_cast<void>(canvasHandle_.SetVisible(false));
}

void RhythmTestScene::Update(
    const mrg::UpdateContext& context,
    mrg::scene::SceneManager& scenes)
{
    if (canvas_ == nullptr || session_ == nullptr)
    {
        return;
    }

    const mrg::audio::AudioClockSnapshot clock =
        context.audio.CaptureClockSnapshot();
    if (context.input.WasKeyPressed(VK_ESCAPE))
    {
        if (!ReturnToLobby(scenes))
        {
            throw std::runtime_error("Failed to return to song select.");
        }
        return;
    }

    keyBeam_.Update(context.deltaSeconds);
    ProcessControlKeys(context.input, clock);
    if (debugMode_)
    {
        ProcessDebugTimeline(context.input, clock, context.deltaSeconds);
    }
    if (debugMode_ ||
        timer_.CurrentState() ==
            finger_drum::rhythm::RhythmTimer::State::Running)
    {
        ProcessRhythmInput(context.input);
        const auto time = timer_.Now(clock.performanceCounterTicks);
        UpdateSession(context.input, time);
        audioRouter_.ApplyAutomation(session_->EvaluateAutomation(time));
        UpdatePresentation(time);
    }
    else
    {
        UpdatePresentation(timer_.Now(clock.performanceCounterTicks));
    }
    UpdateInputPresentation(context.input);
    audioRouter_.Update();
    if (!audioRouter_.LastError().empty())
    {
        PresentAudioError(audioRouter_.LastError());
    }

    // DestroyOnExit removes this complete mode instance after the deferred
    // transition. Only Lobby and its selected catalog data remain alive.
    if (!debugMode_ && IsPatternComplete())
    {
        completedElapsedSeconds_ += context.deltaSeconds;
        if (completedElapsedSeconds_ >= 3.0)
        {
            if (!ReturnToLobby(scenes))
            {
                throw std::runtime_error("Failed to return to song select.");
            }
            return;
        }
    }
}

void RhythmTestScene::Render(const mrg::graphics::RenderContext&)
{
}

void RhythmTestScene::OnResize(
    const std::uint32_t width,
    const std::uint32_t height)
{
    width_ = width;
    height_ = height;
    if (canvas_ != nullptr)
    {
        UpdatePresentationLayout();
    }
}

void RhythmTestScene::Shutdown() noexcept
{
    keyBeam_.Shutdown();
    timer_.Stop();
    audioRouter_.Shutdown();
    noteVisuals_.clear();
    presentedNoteIds_.clear();
    completionEffects_.clear();
    measureLineVisuals_.clear();
    keyIndicators_.fill(nullptr);
    keyGlows_.fill(nullptr);
    keyPressFlashes_.fill(nullptr);
    background_ = nullptr;
    scrollGearBorder_ = nullptr;
    scrollGearSurface_ = nullptr;
    inputPresentationRoot_ = nullptr;
    inputPanel_ = nullptr;
    laneRoot_ = nullptr;
    laneTiles_.clear();
    laneImage_ = {};
    strongKeyLightImage_ = {};
    weakKeyLightImage_ = {};
    keyPressFlashImage_ = {};
    gameProgressBar_ = nullptr;
    accuracyIndicator_ = nullptr;
    judgementIndicator_ = nullptr;
    audioErrorLabel_ = nullptr;
    canvas_ = nullptr;
    canvasHandle_.Reset();
    session_.reset();
    musicRegistered_ = false;
    if (launchRequest_ != nullptr)
    {
        launchRequest_->Clear();
    }
}

std::unique_ptr<finger_drum::mode::PlaySession>
RhythmTestScene::CreateSession()
{
    if (!launchRequest_->IsValid())
    {
        return debugMode_
            ? CreateLongNoteDebugSession()
            : CreateDemoSession();
    }
    if (!launchRequest_->mode.empty() && launchRequest_->mode != "Taiko")
    {
        throw std::runtime_error(
            "The selected pattern requests an unsupported game mode: " +
            launchRequest_->mode);
    }

    finger_drum::mode::TaikoMode mode;
    finger_drum::mode::ModeLoadResult loaded = mode.LoadSession(
        launchRequest_->patternPath,
        launchRequest_->effectPath);
    if (!loaded.Succeeded())
    {
        const std::string detail = loaded.diagnostics.empty()
            ? "Unknown chart error."
            : loaded.diagnostics.front().message;
        throw std::runtime_error("Failed to load the selected pattern: " + detail);
    }
    return std::move(loaded.session);
}

void RhythmTestScene::PrepareDebugLaunchRequest()
{
    if (!debugMode_ || launchRequest_->IsValid())
    {
        return;
    }
    const std::filesystem::path songs =
        mrg::platform::ResolveExecutableRelativePath(L"assets\\songs");
    const std::filesystem::path patternPath = songs /
        L"Pattern\\angeldream\\angeldream [long notes test].ymp";
    const std::filesystem::path musicPath = songs /
        L"Music\\Angeldream\\angel dream hand shaking.mp3";
    if (!std::filesystem::is_regular_file(patternPath) ||
        !std::filesystem::is_regular_file(musicPath))
    {
        return;
    }
    launchRequest_->patternPath = patternPath;
    launchRequest_->musicPath = musicPath;
    launchRequest_->mode = "Taiko";
}

std::unique_ptr<finger_drum::mode::PlaySession>
RhythmTestScene::CreateDemoSession()
{
    finger_drum::chart::PatternDocument pattern;
    pattern.name = "FingerDrum architecture test";
    pattern.mode = "Taiko";
    pattern.baseBpm = 120.0;
    pattern.judgementLevel = 50;

    using NoteType = finger_drum::mode::TaikoNoteType;
    using Action = finger_drum::mode::TaikoPatternAction;
    AddPatternNote(pattern, 0, 0, 1, NoteType::Don, Action::Down);
    AddPatternNote(pattern, 0, 1, 4, NoteType::Kat, Action::Down);
    AddPatternNote(pattern, 0, 2, 4, NoteType::BigDon, Action::Down);
    AddPatternNote(pattern, 0, 3, 4, NoteType::BigKat, Action::Down);
    AddPatternNote(pattern, 1, 0, 1, NoteType::Roll, Action::LongNoteStart);
    AddPatternNote(pattern, 2, 0, 1, NoteType::Roll, Action::LongNoteEnd);
    AddPatternNote(pattern, 2, 1, 4, NoteType::TickRoll, Action::LongNoteStart);
    AddPatternNote(pattern, 3, 0, 1, NoteType::TickRoll, Action::LongNoteEnd);

    finger_drum::mode::TaikoMode mode;
    finger_drum::mode::ModeLoadResult result = mode.CreateSession(pattern);
    if (!result.Succeeded())
    {
        throw std::runtime_error("Failed to create the Taiko demo session.");
    }
    return std::move(result.session);
}

std::unique_ptr<finger_drum::mode::PlaySession>
RhythmTestScene::CreateLongNoteDebugSession()
{
    finger_drum::chart::PatternDocument pattern;
    pattern.name = "Long Notes Debug";
    pattern.mode = "Taiko";
    pattern.baseBpm = 180.0;
    pattern.patternOffsetMilliseconds = 104.0;
    pattern.judgementLevel = 50;

    using NoteType = finger_drum::mode::TaikoNoteType;
    using Action = finger_drum::mode::TaikoPatternAction;
    const auto addLong = [&pattern](
        const std::int64_t measure,
        const std::int64_t startNumerator,
        const NoteType type,
        std::vector<std::string> extraData = {})
    {
        AddPatternNote(
            pattern,
            measure,
            startNumerator,
            4,
            type,
            Action::LongNoteStart,
            std::move(extraData));
        AddPatternNote(
            pattern,
            measure,
            startNumerator + 1,
            4,
            type,
            Action::LongNoteEnd);
    };

    addLong(0, 0, NoteType::Roll);
    addLong(0, 2, NoteType::BigRoll);
    addLong(1, 0, NoteType::TickRoll, {"TickDivision=16"});
    addLong(1, 2, NoteType::BigTickRoll, {"TickDivision=16"});
    addLong(2, 0, NoteType::Balloon, {"8"});
    addLong(2, 2, NoteType::DengDeng, {"HitCount=8"});
    addLong(
        3,
        0,
        NoteType::Buzz,
        {"Action=Don", "TickDivision=16"});
    addLong(
        3,
        2,
        NoteType::Buzz,
        {"Action=Kat", "TickDivision=16"});

    finger_drum::mode::TaikoMode mode;
    finger_drum::mode::ModeLoadResult result = mode.CreateSession(pattern);
    if (!result.Succeeded())
    {
        throw std::runtime_error(
            "Failed to create the built-in long-note debug session.");
    }
    return std::move(result.session);
}

void RhythmTestScene::CreatePresentation()
{
    auto& root = canvas_->Root();

    background_ = &mrg::visual2d::CreatePanel(
        root,
        {-CanvasReferenceWidth * 0.5F,
         -CanvasReferenceHeight * 0.5F,
         CanvasReferenceWidth,
         CanvasReferenceHeight},
        "Background");
    SetColor(*background_, {0.941F, 0.973F, 1.0F, 1.0F});

    audioErrorLabel_ = &mrg::visual2d::CreateLabel(
        root,
        {-500.0F, 320.0F, 1000.0F, 28.0F},
        L"",
        "Hud.AudioError");
    audioErrorLabel_->SetZIndex(20);
    audioErrorLabel_->SetVisible(false);
    auto& audioErrorText = RequireComponent<
        mrg::visual2d::TextVisualComponent>(*audioErrorLabel_);
    audioErrorText.SetFontSize(16.0F);
    audioErrorText.SetTextColor({0.78F, 0.06F, 0.08F, 1.0F});
    audioErrorText.SetHorizontalAlignment(
        mrg::visual2d::TextAlignment::Center);

    const auto progressImage = screenVisuals_.RegisterImage(
        InGameSkinAssetPath(L"GameProgressBar.png"));
    const auto progressSize = ScaledImageSize(
        screenVisuals_, progressImage);
    gameProgressBar_ = &mrg::visual2d::CreateSprite(
        root,
        {-CanvasReferenceWidth * 0.5F + GearMargin,
         CanvasReferenceHeight * 0.5F - 16.0F - progressSize.height,
         progressSize.width,
         progressSize.height},
        progressImage,
        "Hud.GameProgress");
    gameProgressBar_->SetZIndex(5);

    const auto accuracyImage = screenVisuals_.RegisterImage(
        InGameSkinAssetPath(L"AccuracyIndicator.png"));
    const auto accuracySize = ScaledImageSize(
        screenVisuals_, accuracyImage);
    accuracyIndicator_ = &mrg::visual2d::CreateSprite(
        root,
        {CanvasReferenceWidth * 0.5F - GearMargin - accuracySize.width,
         CanvasReferenceHeight * 0.5F - 50.0F - accuracySize.height,
         accuracySize.width,
         accuracySize.height},
        accuracyImage,
        "Hud.Accuracy.Unavailable");
    accuracyIndicator_->SetZIndex(5);

    const auto judgementImage = screenVisuals_.RegisterImage(
        InGameSkinAssetPath(L"JudgementIndicator.png"));
    const auto judgementSize = ScaledImageSize(
        screenVisuals_, judgementImage);
    judgementIndicator_ = &mrg::visual2d::CreateSprite(
        root,
        {-judgementSize.width * 0.5F,
         -CanvasReferenceHeight * 0.5F + 18.0F,
         judgementSize.width,
         judgementSize.height},
        judgementImage,
        "Hud.JudgementGuide");
    judgementIndicator_->SetZIndex(5);

    scrollGearBorder_ = &mrg::visual2d::CreatePanel(
        root,
        {-CanvasReferenceWidth * 0.5F + GearMargin - GearPadding,
         LaneCenterY - laneWidth_ * 0.5F - GearPadding,
         CanvasReferenceWidth - GearMargin - GearRightMargin +
             GearPadding * 2.0F,
         laneWidth_ + GearPadding * 2.0F},
        "ScrollGear.Border");
    SetColor(*scrollGearBorder_, {0.16F, 0.31F, 0.58F, 1.0F});
    scrollGearSurface_ = &mrg::visual2d::CreatePanel(
        root,
        {-CanvasReferenceWidth * 0.5F + GearMargin,
         LaneCenterY - laneWidth_ * 0.5F,
         CanvasReferenceWidth - GearMargin - GearRightMargin,
         laneWidth_},
        "ScrollGear.Surface");
    SetColor(*scrollGearSurface_, {0.73F, 0.82F, 0.93F, 1.0F});

    CreateLaneVisuals(root);
    inputPresentationRoot_ = &root.CreateChild("InputPresentation");
    inputPresentationRoot_->SetZIndex(2);
    const auto inputPanelImage = screenVisuals_.RegisterImage(
        InGameSkinAssetPath(L"InputPanel.png"));
    inputPanelSize_ = ScaledImageSize(
        screenVisuals_, inputPanelImage);
    inputPanel_ = &mrg::visual2d::CreateSprite(
        *inputPresentationRoot_,
        {0.0F, 0.0F, inputPanelSize_.width, inputPanelSize_.height},
        inputPanelImage,
        "Input.Panel");
    inputPanel_->SetZIndex(0);
    CreateKeyIndicators(*inputPanel_);

    UpdatePresentationLayout();
}

void RhythmTestScene::CreateLaneVisuals(
    mrg::visual2d::Visual2DNode& sceneRoot)
{
    // Author the lane in its reusable default orientation: local +Y is the
    // future-note direction and every lane-owned visual is a descendant.
    const float laneScreenLeft = GearMargin + inputPanelSize_.width;
    const float laneLength = std::max(
        canvas_->LogicalSize().width - laneScreenLeft - GearRightMargin,
        1.0F);
    laneRoot_ = &sceneRoot.CreateChild("ScrollGear.Lane");
    laneRoot_->SetPivot({0.5F, 0.5F});
    laneRoot_->SetSize({laneWidth_, laneLength});
    laneRoot_->SetPosition({
        -canvas_->LogicalSize().width * 0.5F +
            laneScreenLeft + laneLength * 0.5F,
        LaneCenterY});
    laneRoot_->SetZIndex(1);

    // Taiko is a presentation of the same vertical lane rotated clockwise.
    // Notes continue to update only local Y, so changing this one transform
    // redirects the background, effects, judgement line and notes together.
    laneRoot_->Transform().SetRotationRollPitchYaw(
        0.0F,
        0.0F,
        -DirectX::XM_PIDIV2);

    CreateLaneSurface();
    CreateMeasureLineVisuals();

    const auto beamImage = screenVisuals_.RegisterImage(
        InGameSkinAssetPath(L"LaneLight.png"));
    keyBeam_.Initialize(*laneRoot_, beamImage,
        ScaledImageSize(screenVisuals_, beamImage));

    const auto judgementImage = screenVisuals_.RegisterImage(
        InGameSkinAssetPath(L"JudgementCircle.png"));
    const auto judgementSize = ScaledImageSize(
        screenVisuals_, judgementImage);
    auto& judgementLine = mrg::visual2d::CreateSprite(
        *laneRoot_,
        {(laneWidth_ - judgementSize.width) * 0.5F,
         judgementLocalY_ - judgementSize.height * 0.5F,
         judgementSize.width,
         judgementSize.height},
        judgementImage,
        "Lane.JudgementCircle");
    judgementLine.SetZIndex(4);
}

void RhythmTestScene::CreateLaneSurface()
{
    if (laneRoot_ == nullptr)
    {
        throw std::logic_error("Lane background requires a Lane root.");
    }

    laneImage_ = screenVisuals_.RegisterImage(
        InGameSkinAssetPath(L"Lane.png"));
    const auto tileSize = ScaledImageSize(
        screenVisuals_, laneImage_);
    laneWidth_ = tileSize.width;
    laneTileLength_ = tileSize.height;
    judgementLocalY_ = laneWidth_ * 0.5F;
    const float laneScreenLeft = GearMargin + inputPanelSize_.width;
    UpdateLaneSurfaceLayout(std::max(
        canvas_->LogicalSize().width - laneScreenLeft - GearRightMargin,
        1.0F));
}

void RhythmTestScene::UpdateLaneSurfaceLayout(const float laneLength)
{
    keyBeam_.SetLayout(laneWidth_, laneLength);
    if (laneRoot_ == nullptr || !laneImage_ || laneTileLength_ <= 0.0F)
    {
        return;
    }

    const std::size_t requiredTiles = static_cast<std::size_t>(
        std::ceil(laneLength / laneTileLength_));
    while (laneTiles_.size() < requiredTiles)
    {
        auto& tile = mrg::visual2d::CreateSprite(
            *laneRoot_, {}, laneImage_, "Lane.SurfaceTile");
        tile.SetZIndex(0);
        laneTiles_.push_back(&tile);
    }
    for (std::size_t index = 0; index < laneTiles_.size(); ++index)
    {
        mrg::visual2d::Visual2DNode& tile = *laneTiles_[index];
        const float start = static_cast<float>(index) * laneTileLength_;
        const float visibleLength = std::clamp(
            laneLength - start, 0.0F, laneTileLength_);
        tile.SetVisible(visibleLength > 0.0F);
        if (visibleLength <= 0.0F)
        {
            continue;
        }
        tile.SetBounds({0.0F, start, laneWidth_, visibleLength});
        RequireComponent<mrg::visual2d::SpriteVisualComponent>(tile).
            SetUvTransform(
                {1.0F, visibleLength / laneTileLength_},
                {0.0F, 0.0F});
    }
}

void RhythmTestScene::CreateMeasureLineVisuals()
{
    for (const finger_drum::rhythm::RhythmTime timing :
        session_->MeasureLines())
    {
        auto& line = mrg::visual2d::CreatePanel(
            *laneRoot_,
            {0.0F, 0.0F, laneWidth_, 4.0F},
            "Lane.MeasureLine");
        SetColor(line, {0.78F, 0.85F, 0.90F, 0.72F});
        line.SetZIndex(1);
        line.SetVisible(false);
        measureLineVisuals_.push_back({timing, &line});
    }
}

void RhythmTestScene::CreateKeyIndicators(
    mrg::visual2d::Visual2DNode& inputPanel)
{
    struct KeySpec
    {
        mrg::visual2d::Rect bounds;
        mrg::visual2d::Color baseColor;
        std::wstring_view image;
    };
    constexpr std::array<KeySpec, 4> keys{{
        {{16.0F, 68.0F, 39.0F, 70.0F},
         InputKeyBaseColors[0], L"KeyButtonLeftKat.png"},
        {{68.0F, 92.0F, 39.0F, 70.0F},
         InputKeyBaseColors[1], L"KeyButtonLeftDon.png"},
        {{120.0F, 92.0F, 39.0F, 70.0F},
         InputKeyBaseColors[2], L"KeyButtonRightDon.png"},
        {{172.0F, 68.0F, 39.0F, 70.0F},
         InputKeyBaseColors[3], L"KeyButtonRightKat.png"},
    }};

    strongKeyLightImage_ = screenVisuals_.RegisterImage(
        InGameSkinAssetPath(L"KeyLightStrong.png"));
    weakKeyLightImage_ = screenVisuals_.RegisterImage(
        InGameSkinAssetPath(L"KeyLightWeak.png"));
    keyPressFlashImage_ = screenVisuals_.RegisterImage(
        InGameSkinAssetPath(L"KeyPressFlash.png"));
    const auto lightSize = ScaledImageSize(
        screenVisuals_, strongKeyLightImage_);
    const auto pressFlashSize = ScaledImageSize(
        screenVisuals_, keyPressFlashImage_);
    for (std::size_t index = 0; index < keys.size(); ++index)
    {
        const mrg::visual2d::Rect bounds{
            keys[index].bounds.x * InGameAssetScale,
            inputPanelSize_.height -
                (keys[index].bounds.y + keys[index].bounds.height) *
                    InGameAssetScale,
            keys[index].bounds.width * InGameAssetScale,
            keys[index].bounds.height * InGameAssetScale};
        auto& glow = mrg::visual2d::CreateSprite(
            inputPanel,
            {bounds.x - (lightSize.width - bounds.width) * 0.5F,
             bounds.y - (lightSize.height - bounds.height) * 0.5F,
             lightSize.width,
             lightSize.height},
            strongKeyLightImage_,
            "Input.Key.Glow");
        RequireComponent<mrg::visual2d::SpriteVisualComponent>(glow).
            SetTint(keys[index].baseColor);
        glow.SetVisible(false);
        glow.SetZIndex(1);
        keyGlows_[index] = &glow;

        const auto keyImage = screenVisuals_.RegisterImage(
            InGameSkinAssetPath(keys[index].image));
        auto& face = mrg::visual2d::CreateSprite(
            inputPanel,
            bounds,
            keyImage,
            "Input.Key.Face");
        RequireComponent<mrg::visual2d::SpriteVisualComponent>(face).
            SetTint(keys[index].baseColor);
        face.SetZIndex(2);
        keyIndicators_[index] = &face;

        auto& pressFlash = mrg::visual2d::CreateSprite(
            inputPanel,
            {bounds.x - (pressFlashSize.width - bounds.width) * 0.5F,
             bounds.y - (pressFlashSize.height - bounds.height) * 0.5F,
             pressFlashSize.width,
             pressFlashSize.height},
            keyPressFlashImage_,
            "Input.Key.PressFlash");
        pressFlash.SetVisible(false);
        pressFlash.SetZIndex(3);
        keyPressFlashes_[index] = &pressFlash;
    }
}

void RhythmTestScene::CreateNoteVisuals()
{
    const auto noteImage = screenVisuals_.RegisterImage(
        InGameSkinAssetPath(L"note.png"));
    const auto noteOverlay = screenVisuals_.RegisterImage(
        InGameSkinAssetPath(L"noteoverlay.png"));
    const auto bigNoteImage = screenVisuals_.RegisterImage(
        InGameSkinAssetPath(L"bignote.png"));
    const auto bigOverlay = screenVisuals_.RegisterImage(
        InGameSkinAssetPath(L"bigcircleoverlay.png"));
    const auto bodyImage = screenVisuals_.RegisterImage(
        InGameSkinAssetPath(L"LNBody.png"));
    const auto bodyOverlayImage = screenVisuals_.RegisterImage(
        InGameSkinAssetPath(L"LNBodyOverlay.png"));
    const auto tailImage = screenVisuals_.RegisterImage(
        InGameSkinAssetPath(L"LNTail.png"));
    const auto tailOverlayImage = screenVisuals_.RegisterImage(
        InGameSkinAssetPath(L"LNTailOverlay.png"));
    const auto bigBodyImage = screenVisuals_.RegisterImage(
        InGameSkinAssetPath(L"BigLNBody.png"));
    const auto bigBodyOverlayImage = screenVisuals_.RegisterImage(
        InGameSkinAssetPath(L"BigLNBodyOverlay.png"));
    const auto bigTailImage = screenVisuals_.RegisterImage(
        InGameSkinAssetPath(L"BigLNTail.png"));
    const auto bigTailOverlayImage = screenVisuals_.RegisterImage(
        InGameSkinAssetPath(L"BigLNTailOverlay.png"));
    const auto tickImage = screenVisuals_.RegisterImage(
        InGameSkinAssetPath(L"TickMarker.png"));
    const auto buzzTickImage = screenVisuals_.RegisterImage(
        InGameSkinAssetPath(L"BuzzTickDiamond.png"));
    const auto balloonImage = screenVisuals_.RegisterImage(
        InGameSkinAssetPath(L"Balloon.png"));
    const auto dengDengImage = screenVisuals_.RegisterImage(
        InGameSkinAssetPath(L"DengDeng.png"));
    const auto balloonProcessingImage = screenVisuals_.RegisterImage(
        InGameSkinAssetPath(L"BalloonProcessing.png"));
    const auto balloonBurstImage = screenVisuals_.RegisterImage(
        InGameSkinAssetPath(L"BalloonBurst.png"));
    const auto dengDengProcessingImage = screenVisuals_.RegisterImage(
        InGameSkinAssetPath(L"DengDengProcessing.png"));
    const auto counterImage = screenVisuals_.RegisterImage(
        InGameSkinAssetPath(L"HitCounterCloud.png"));

    const auto noteSize = ScaledImageSize(
        screenVisuals_, noteImage);
    const auto bigNoteSize = ScaledImageSize(
        screenVisuals_, bigNoteImage);
    const auto bodySize = ScaledImageSize(
        screenVisuals_, bodyImage);
    const auto tailSize = ScaledImageSize(
        screenVisuals_, tailImage);
    const auto bigBodySize = ScaledImageSize(
        screenVisuals_, bigBodyImage);
    const auto bigTailSize = ScaledImageSize(
        screenVisuals_, bigTailImage);
    const auto tickSize = ScaledImageSize(
        screenVisuals_, tickImage);
    const auto buzzTickSize = ScaledImageSize(
        screenVisuals_, buzzTickImage);
    const auto balloonSize = ScaledImageSize(
        screenVisuals_, balloonImage);
    const auto dengDengSize = ScaledImageSize(
        screenVisuals_, dengDengImage);
    const auto balloonProcessingSize = ScaledImageSize(
        screenVisuals_, balloonProcessingImage);
    const auto balloonBurstSize = ScaledImageSize(
        screenVisuals_, balloonBurstImage);
    const auto dengDengProcessingSize = ScaledImageSize(
        screenVisuals_, dengDengProcessingImage);
    const auto counterSize = ScaledImageSize(
        screenVisuals_, counterImage);

    if (laneRoot_ == nullptr)
    {
        throw std::logic_error("Note visuals require a Lane root.");
    }
    for (const std::unique_ptr<finger_drum::rhythm::Lane>& lane :
        session_->Gear().Lanes())
    {
        for (const std::unique_ptr<finger_drum::rhythm::INote>& note :
            lane->Notes())
        {
            const finger_drum::mode::NotePresentationInfo* presentation =
                session_->FindNotePresentation(note->Id());
            const std::string_view visualId = presentation == nullptr
                ? std::string_view{"Taiko.Don"}
                : std::string_view{presentation->visualId};
            const bool big = IsBigVisual(visualId);
            const bool balloon = visualId == "Taiko.Balloon";
            const bool dengDeng = visualId == "Taiko.DengDeng";
            const bool customHead = balloon || dengDeng;
            const bool longNote = presentation != nullptr &&
                presentation->hasEndTime && IsLongVisual(visualId) &&
                !customHead;
            const bool bigLongParts = visualId == "Taiko.BigRoll";
            const bool buzz = visualId == "Taiko.Buzz.Don" ||
                visualId == "Taiko.Buzz.Kat";
            const auto headImage = balloon
                ? balloonImage
                : (dengDeng
                    ? dengDengImage
                    : (big ? bigNoteImage : noteImage));
            const auto headSize = balloon
                ? balloonSize
                : (dengDeng
                    ? dengDengSize
                    : (big ? bigNoteSize : noteSize));
            const float diameter = headSize.width;
            const mrg::visual2d::Color ambientColor = AmbientColor(visualId);

            NoteVisualLayers layers;
            layers.focusType = balloon
                ? FocusNoteType::Balloon
                : (dengDeng
                    ? FocusNoteType::DengDeng
                    : FocusNoteType::None);
            layers.root = &laneRoot_->CreateChild(
                std::format("Lane.Note.{}", note->Id()));
            layers.root->SetPivot({0.5F, 0.5F});
            layers.root->SetSize({diameter, diameter});
            layers.root->SetPosition({laneWidth_ * 0.5F, judgementLocalY_});
            layers.root->SetZIndex(3);
            layers.root->SetVisible(false);
            layers.diameter = diameter;

            // Body and tail are Ambient-colored and sit behind the circular
            // head. The white head overlay is a separate untinted draw packet.
            if (longNote)
            {
                const auto selectedBodyImage = bigLongParts
                    ? bigBodyImage
                    : bodyImage;
                const auto selectedBodyOverlayImage = bigLongParts
                    ? bigBodyOverlayImage
                    : bodyOverlayImage;
                const auto selectedTailImage = bigLongParts
                    ? bigTailImage
                    : tailImage;
                const auto selectedTailOverlayImage = bigLongParts
                    ? bigTailOverlayImage
                    : tailOverlayImage;
                const auto selectedBodySize = bigLongParts
                    ? bigBodySize
                    : bodySize;
                const auto selectedTailSize = bigLongParts
                    ? bigTailSize
                    : tailSize;
                layers.bodyWidth = selectedBodySize.width;
                layers.tailWidth = selectedTailSize.width;
                layers.tailHeight = selectedTailSize.height;
                const float bodyX =
                    (diameter - layers.bodyWidth) * 0.5F;
                const float tailX =
                    (diameter - layers.tailWidth) * 0.5F;
                layers.body = &mrg::visual2d::CreateSprite(
                    *layers.root,
                    {bodyX, diameter * 0.5F, layers.bodyWidth, 1.0F},
                    selectedBodyImage,
                    "AmbientBody");
                RequireComponent<mrg::visual2d::SpriteVisualComponent>(
                    *layers.body).SetTint(ambientColor);
                layers.body->SetZIndex(0);
                layers.bodyOverlay = &mrg::visual2d::CreateSprite(
                    *layers.root,
                    {bodyX, diameter * 0.5F, layers.bodyWidth, 1.0F},
                    selectedBodyOverlayImage,
                    "BodyOverlay");
                layers.bodyOverlay->SetZIndex(1);
                layers.tail = &mrg::visual2d::CreateSprite(
                    *layers.root,
                    {tailX, 0.0F, layers.tailWidth, layers.tailHeight},
                    selectedTailImage,
                    "AmbientTail");
                RequireComponent<mrg::visual2d::SpriteVisualComponent>(
                    *layers.tail).SetTint(ambientColor);
                layers.tail->SetZIndex(2);
                layers.tail->SetPivot({0.5F, 0.5F});
                layers.tail->SetPosition({
                    tailX + layers.tailWidth * 0.5F,
                    layers.tailHeight * 0.5F});
                layers.tailOverlay = &mrg::visual2d::CreateSprite(
                    *layers.root,
                    {tailX, 0.0F, layers.tailWidth, layers.tailHeight},
                    selectedTailOverlayImage,
                    "TailOverlay");
                layers.tailOverlay->SetZIndex(3);
                layers.tailOverlay->SetPivot({0.5F, 0.5F});
                layers.tailOverlay->SetPosition({
                    tailX + layers.tailWidth * 0.5F,
                    layers.tailHeight * 0.5F});

                if (presentation != nullptr)
                {
                    const auto selectedTickImage = buzz
                        ? buzzTickImage
                        : tickImage;
                    const auto selectedTickSize = buzz
                        ? buzzTickSize
                        : tickSize;
                    for (const auto tickTime : presentation->tickTimes)
                    {
                        if (tickTime <= note->Timing())
                        {
                            continue;
                        }
                        const float offset = static_cast<float>(
                            (tickTime - note->Timing()).count()) /
                            static_cast<float>(ApproachDuration.count()) *
                            TravelDistance;
                        auto& tick = mrg::visual2d::CreateSprite(
                            *layers.root,
                            {(diameter - selectedTickSize.width) * 0.5F,
                             diameter * 0.5F + offset -
                                 selectedTickSize.height * 0.5F,
                             selectedTickSize.width,
                             selectedTickSize.height},
                            selectedTickImage,
                            "LongNote.Tick");
                        tick.SetZIndex(4);
                        layers.ticks.push_back({tickTime, &tick});
                    }
                }
            }

            layers.ambient = &mrg::visual2d::CreateSprite(
                *layers.root,
                {0.0F, 0.0F, headSize.width, headSize.height},
                headImage,
                "AmbientHead");
            RequireComponent<mrg::visual2d::SpriteVisualComponent>(
                *layers.ambient).SetTint(customHead
                    ? mrg::visual2d::Color{1.0F, 1.0F, 1.0F, 1.0F}
                    : ambientColor);
            layers.ambient->SetZIndex(3);
            if (!customHead)
            {
                layers.overlay = &mrg::visual2d::CreateSprite(
                    *layers.root,
                    {0.0F, 0.0F, headSize.width, headSize.height},
                    big ? bigOverlay : noteOverlay,
                    "UntintedOverlay");
                RequireComponent<mrg::visual2d::SpriteVisualComponent>(
                    *layers.overlay).SetTint({1.0F, 1.0F, 1.0F, 1.0F});
                layers.overlay->SetZIndex(4);
            }
            if (customHead)
            {
                const auto processingImage = balloon
                    ? balloonProcessingImage
                    : dengDengProcessingImage;
                layers.processingSize = balloon
                    ? balloonProcessingSize
                    : dengDengProcessingSize;
                layers.processing = &mrg::visual2d::CreateSprite(
                    canvas_->Root(),
                    {0.0F,
                     0.0F,
                     layers.processingSize.width,
                     layers.processingSize.height},
                    processingImage,
                    std::format("Hud.NoteProcessing.{}", note->Id()));
                layers.processing->SetPivot({0.5F, 0.5F});
                layers.processing->SetZIndex(7);
                layers.processing->SetVisible(false);
                if (balloon)
                {
                    layers.successSize = balloonBurstSize;
                    layers.success = &mrg::visual2d::CreateSprite(
                        canvas_->Root(),
                        {0.0F,
                         0.0F,
                         balloonBurstSize.width,
                         balloonBurstSize.height},
                        balloonBurstImage,
                        std::format("Hud.NoteSuccess.{}", note->Id()));
                    layers.success->SetPivot({0.5F, 0.5F});
                    layers.success->SetZIndex(9);
                    layers.success->SetVisible(false);
                }
                layers.counter = &mrg::visual2d::CreateSprite(
                    canvas_->Root(),
                    {0.0F, 0.0F, counterSize.width, counterSize.height},
                    counterImage,
                    std::format("Hud.NoteCounter.{}", note->Id()));
                layers.counter->SetZIndex(8);
                layers.counter->SetVisible(false);
                layers.counterText = &mrg::visual2d::CreateLabel(
                    *layers.counter,
                    {0.0F,
                     counterSize.height * 0.22F,
                     counterSize.width,
                     counterSize.height * 0.58F},
                    L"",
                    "RemainingHits");
                auto& text = RequireComponent<
                    mrg::visual2d::TextVisualComponent>(*layers.counterText);
                text.SetFontSize(22.0F);
                text.SetTextColor({1.0F, 1.0F, 1.0F, 1.0F});
                text.SetHorizontalAlignment(
                    mrg::visual2d::TextAlignment::Center);
            }
            noteVisuals_.emplace(note->Id(), layers);
        }
    }
}

void RhythmTestScene::UpdatePresentationLayout()
{
    if (canvas_ == nullptr)
    {
        return;
    }

    const float logicalWidth = canvas_->LogicalSize().width;
    const float laneScreenLeft = GearMargin + inputPanelSize_.width;
    const float laneLength = std::max(
        logicalWidth - laneScreenLeft - GearRightMargin,
        1.0F);
    const float gearHeight = std::max(laneWidth_, inputPanelSize_.height);

    if (background_ != nullptr)
    {
        background_->SetBounds({
            -logicalWidth * 0.5F,
            -CanvasReferenceHeight * 0.5F,
            logicalWidth,
            CanvasReferenceHeight});
    }
    if (gameProgressBar_ != nullptr)
    {
        gameProgressBar_->SetBounds({
            -logicalWidth * 0.5F + GearMargin,
            CanvasReferenceHeight * 0.5F - 16.0F -
                gameProgressBar_->Bounds().height,
            std::max(logicalWidth - GearMargin * 2.0F, 1.0F),
            gameProgressBar_->Bounds().height});
    }
    if (accuracyIndicator_ != nullptr)
    {
        const auto bounds = accuracyIndicator_->Bounds();
        accuracyIndicator_->SetBounds({
            logicalWidth * 0.5F - GearMargin - bounds.width,
            bounds.y,
            bounds.width,
            bounds.height});
    }
    if (judgementIndicator_ != nullptr)
    {
        const auto bounds = judgementIndicator_->Bounds();
        judgementIndicator_->SetBounds({
            -bounds.width * 0.5F,
            bounds.y,
            bounds.width,
            bounds.height});
    }
    if (scrollGearBorder_ != nullptr)
    {
        scrollGearBorder_->SetBounds({
            -logicalWidth * 0.5F + GearMargin - GearPadding,
            LaneCenterY - gearHeight * 0.5F - GearPadding,
            std::max(
                logicalWidth - GearMargin - GearRightMargin +
                    GearPadding * 2.0F,
                1.0F),
            gearHeight + GearPadding * 2.0F});
    }
    if (scrollGearSurface_ != nullptr)
    {
        scrollGearSurface_->SetBounds({
            -logicalWidth * 0.5F + GearMargin,
            LaneCenterY - gearHeight * 0.5F,
            std::max(
                logicalWidth - GearMargin - GearRightMargin,
                1.0F),
            gearHeight});
    }
    if (laneRoot_ != nullptr)
    {
        laneRoot_->SetSize({laneWidth_, laneLength});
        laneRoot_->SetPosition({
            -logicalWidth * 0.5F + laneScreenLeft + laneLength * 0.5F,
            LaneCenterY});
        UpdateLaneSurfaceLayout(laneLength);
    }
    if (inputPresentationRoot_ != nullptr)
    {
        inputPresentationRoot_->SetPosition({
            -logicalWidth * 0.5F + GearMargin,
            LaneCenterY - inputPanelSize_.height * 0.5F});
    }
}

void RhythmTestScene::InitializeAudio(const mrg::EngineServices& services)
{
    std::string initializationError;
    if (!audioRouter_.Initialize(services.audio, initializationError))
    {
        PresentAudioError(initializationError);
        return;
    }

    std::string hitSoundError;
    RegisterTaikoSounds(hitSoundError);

    std::string musicError;
    if (launchRequest_->IsValid())
    {
        musicRegistered_ = audioRouter_.RegisterSound(
            "Music.Track",
            launchRequest_->musicPath,
            mrg::audio::AudioLoadMode::Stream,
            musicError);
    }

    if (!hitSoundError.empty() || !musicError.empty())
    {
        PresentAudioError(!musicError.empty() ? musicError : hitSoundError);
    }
}

void RhythmTestScene::RegisterTaikoSounds(std::string& errorMessage)
{
    struct SoundRegistration final
    {
        std::span<const finger_drum::rhythm::SoundId> ids;
        std::wstring_view file;
    };
    static const std::array<finger_drum::rhythm::SoundId, 3> DonIds{
        "Taiko.Don.Hit", "Taiko.LongNote.Tick", "Taiko.Don.FreeInput"};
    static const std::array<finger_drum::rhythm::SoundId, 2> KatIds{
        "Taiko.Kat.Hit", "Taiko.Kat.FreeInput"};
    static const std::array<finger_drum::rhythm::SoundId, 1> BigDonIds{
        "Taiko.BigDon.FirstHit"};
    static const std::array<finger_drum::rhythm::SoundId, 1> BigKatIds{
        "Taiko.BigKat.FirstHit"};
    const std::array<SoundRegistration, 4> registrations{{
        {DonIds, L"don.wav"},
        {KatIds, L"kat.wav"},
        {BigDonIds, L"bigdon.wav"},
        {BigKatIds, L"bigkat.wav"},
    }};
    std::string firstError;
    for (const SoundRegistration& registration : registrations)
    {
        std::string registrationError;
        if (!audioRouter_.RegisterSoundAliases(
                registration.ids,
                SkinAssetPath(registration.file),
                registrationError) && firstError.empty())
        {
            firstError = registration.ids.front() + ": " +
                registrationError;
        }
    }
    std::string balloonError;
    if (!audioRouter_.RegisterSound(
        "Taiko.Balloon.Pop",
        SoundAssetPath(L"pop.wav"),
        mrg::audio::AudioLoadMode::Sample,
        balloonError) && firstError.empty())
    {
        firstError = "Taiko.Balloon.Pop: " + balloonError;
    }
    errorMessage = std::move(firstError);
}

void RhythmTestScene::PresentAudioError(const std::string_view message)
{
    if (audioErrorLabel_ == nullptr || message.empty())
    {
        return;
    }
    RequireComponent<mrg::visual2d::TextVisualComponent>(
        *audioErrorLabel_).SetText(L"AUDIO: " + Utf8ToWide(message));
    audioErrorLabel_->SetVisible(true);
}

void RhythmTestScene::ScheduleMusic()
{
    if (!musicRegistered_)
    {
        return;
    }
    finger_drum::rhythm::AudioCueRequest cue;
    cue.sound = "Music.Track";
    cue.bus = "Music";
    cue.timelineTime = finger_drum::rhythm::RhythmTime::zero();
    const std::array cues{cue};
    audioRouter_.Schedule(cues, timer_);
}

void RhythmTestScene::StartTimeline(
    const mrg::audio::AudioClockSnapshot& clock)
{
    constexpr finger_drum::rhythm::RhythmTime LeadIn{-2'000'000};
    timer_.Start(
        clock.performanceCounterTicks,
        clock.performanceCounterFrequency,
        LeadIn);
    timer_.AnchorDspClock(LeadIn, clock.dspClock, clock.sampleRate);
    if (debugMode_ && ReferenceTimeDebug)
    {
        timer_.Pause(clock.performanceCounterTicks);
    }
}

void RhythmTestScene::ResetTimeline(
    const mrg::audio::AudioClockSnapshot& clock)
{
    keyBeam_.Reset();
    session_->Reset();
    completionEffects_.clear();
    audioRouter_.StopAllVoices();
    completedElapsedSeconds_ = 0.0;
    StartTimeline(clock);
    if (!debugMode_)
    {
        ScheduleMusic();
    }
}

void RhythmTestScene::ProcessControlKeys(
    const mrg::platform::InputState& input,
    const mrg::audio::AudioClockSnapshot& clock)
{
    if (input.WasKeyPressed(static_cast<std::uint16_t>('R')))
    {
        ResetTimeline(clock);
        return;
    }
    if (input.WasKeyPressed(VK_SPACE))
    {
        using State = finger_drum::rhythm::RhythmTimer::State;
        if (timer_.CurrentState() == State::Running)
        {
            timer_.Pause(clock.performanceCounterTicks);
            std::string ignoredError;
            static_cast<void>(audioRouter_.SetVoicesPaused(true, ignoredError));
        }
        else if (timer_.CurrentState() == State::Paused)
        {
            timer_.Resume(clock.performanceCounterTicks);
            timer_.AnchorDspClock(
                timer_.Now(clock.performanceCounterTicks),
                clock.dspClock,
                clock.sampleRate);
            std::string ignoredError;
            static_cast<void>(audioRouter_.SetVoicesPaused(false, ignoredError));
        }
    }
}

void RhythmTestScene::ProcessDebugTimeline(
    const mrg::platform::InputState& input,
    const mrg::audio::AudioClockSnapshot& clock,
    const double deltaSeconds)
{
    if (!debugMode_ || !ReferenceTimeDebug)
    {
        return;
    }

    if (input.WasKeyPressed(VK_OEM_MINUS))
    {
        debugSpeedMillisecondsPerSecond_ = std::max(
            debugSpeedMillisecondsPerSecond_ - 200.0,
            200.0);
    }
    if (input.WasKeyPressed(VK_OEM_PLUS))
    {
        debugSpeedMillisecondsPerSecond_ = std::min(
            debugSpeedMillisecondsPerSecond_ + 200.0,
            4000.0);
    }

    std::int64_t deltaMicroseconds{};
    if (input.IsKeyDown(static_cast<std::uint16_t>('1')))
    {
        deltaMicroseconds -= static_cast<std::int64_t>(std::llround(
            debugSpeedMillisecondsPerSecond_ * deltaSeconds * 1000.0));
    }
    if (input.IsKeyDown(static_cast<std::uint16_t>('2')))
    {
        deltaMicroseconds += static_cast<std::int64_t>(std::llround(
            debugSpeedMillisecondsPerSecond_ * deltaSeconds * 1000.0));
    }
    if (input.WasKeyPressed(static_cast<std::uint16_t>('3')))
    {
        deltaMicroseconds -= 1000;
    }
    if (input.WasKeyPressed(static_cast<std::uint16_t>('4')))
    {
        deltaMicroseconds += 1000;
    }
    if (deltaMicroseconds == 0)
    {
        return;
    }

    using TimerState = finger_drum::rhythm::RhythmTimer::State;
    const auto before = timer_.Now(clock.performanceCounterTicks);
    if (timer_.CurrentState() == TimerState::Running)
    {
        timer_.Pause(clock.performanceCounterTicks);
        std::string ignoredError;
        static_cast<void>(audioRouter_.SetVoicesPaused(true, ignoredError));
    }
    const auto after = before +
        finger_drum::rhythm::RhythmDuration{deltaMicroseconds};
    if (after < before)
    {
        keyBeam_.Reset();
        session_->Reset();
        completionEffects_.clear();
        audioRouter_.StopAllVoices();
    }
    timer_.Seek(after, clock.performanceCounterTicks);
    timer_.AnchorDspClock(after, clock.dspClock, clock.sampleRate);
}

void RhythmTestScene::ProcessRhythmInput(
    const mrg::platform::InputState& input)
{
    for (const mrg::platform::InputEvent& event : input.Events())
    {
        if (event.type != mrg::platform::InputEventType::KeyPressed &&
            event.type != mrg::platform::InputEventType::KeyReleased)
        {
            continue;
        }
        const auto inputBinding = std::ranges::find_if(
            finger_drum::mode::TaikoInputBindings,
            [&event](const finger_drum::mode::TaikoInputBinding& binding)
            {
                return binding.Contains(event.code);
            });
        if (inputBinding == finger_drum::mode::TaikoInputBindings.end())
        {
            continue;
        }
        const auto edge = event.type == mrg::platform::InputEventType::KeyPressed
            ? finger_drum::rhythm::InputEdge::Pressed
            : finger_drum::rhythm::InputEdge::Released;
        const auto eventTime = timer_.Now(event.performanceCounterTicks);
        finger_drum::rhythm::NoteProcessResult result =
            session_->ProcessInput(event.code, edge, eventTime);
        if (edge == finger_drum::rhythm::InputEdge::Pressed)
        {
            keyBeam_.OnKeyPressed(result);
        }
        ConsumeResult(std::move(result));
    }
}

void RhythmTestScene::UpdateInputPresentation(
    const mrg::platform::InputState& input)
{
    for (std::size_t index = 0; index < keyIndicators_.size(); ++index)
    {
        const finger_drum::mode::TaikoInputBinding& binding =
            finger_drum::mode::TaikoInputBindings[index];
        const bool primaryPressed = input.IsKeyDown(binding.primaryKey);
        const bool secondaryPressed = std::ranges::any_of(
            binding.secondaryKeys,
            [&input](const finger_drum::rhythm::PhysicalKey key)
            {
                return input.IsKeyDown(key);
            });
        const bool pressedThisFrame =
            input.WasKeyPressed(binding.primaryKey) ||
            std::ranges::any_of(
                binding.secondaryKeys,
                [&input](const finger_drum::rhythm::PhysicalKey key)
                {
                    return input.WasKeyPressed(key);
                });
        const mrg::visual2d::Color& faceColor = primaryPressed
            ? InputKeyStrongColors[index]
            : (secondaryPressed
                ? InputKeyWeakColors[index]
                : InputKeyBaseColors[index]);
        if (keyIndicators_[index] != nullptr)
        {
            RequireComponent<mrg::visual2d::SpriteVisualComponent>(
                *keyIndicators_[index]).SetTint(faceColor);
        }
        if (keyGlows_[index] != nullptr)
        {
            auto& glow = RequireComponent<
                mrg::visual2d::SpriteVisualComponent>(*keyGlows_[index]);
            glow.SetImage(primaryPressed
                ? strongKeyLightImage_
                : weakKeyLightImage_);
            glow.SetTint(InputKeyBaseColors[index]);
            keyGlows_[index]->SetVisible(
                primaryPressed || secondaryPressed);
        }
        if (keyPressFlashes_[index] != nullptr)
        {
            keyPressFlashes_[index]->SetVisible(pressedThisFrame);
        }
    }
}

void RhythmTestScene::UpdateSession(
    const mrg::platform::InputState& input,
    const finger_drum::rhythm::RhythmTime time)
{
    std::array<finger_drum::rhythm::PhysicalKey,
        finger_drum::mode::TaikoInputBindings.size() * 3> heldStorage{};
    std::size_t heldCount{};
    for (const finger_drum::mode::TaikoInputBinding& binding :
        finger_drum::mode::TaikoInputBindings)
    {
        if (input.IsKeyDown(binding.primaryKey))
        {
            heldStorage[heldCount++] = binding.primaryKey;
        }
        for (const finger_drum::rhythm::PhysicalKey secondaryKey :
            binding.secondaryKeys)
        {
            if (input.IsKeyDown(secondaryKey))
            {
                heldStorage[heldCount++] = secondaryKey;
            }
        }
    }
    ConsumeResult(session_->Update(
        time,
        std::span<const finger_drum::rhythm::PhysicalKey>{
            heldStorage.data(), heldCount}));
}

void RhythmTestScene::ConsumeResult(
    finger_drum::rhythm::NoteProcessResult result)
{
    for (const finger_drum::rhythm::NoteEvent& event : result.events)
    {
        if (event.type != finger_drum::rhythm::NoteEventType::Completed)
        {
            continue;
        }
        const auto visual = noteVisuals_.find(event.noteId);
        if (visual != noteVisuals_.end() &&
            visual->second.focusType != FocusNoteType::None)
        {
            completionEffects_[event.noteId] = event.eventTime;
        }
    }
    audioRouter_.PlayNow(result.audioCues);
}

void RhythmTestScene::HideTransientNoteVisuals()
{
    for (const finger_drum::rhythm::NoteId id : presentedNoteIds_)
    {
        const auto visual = noteVisuals_.find(id);
        if (visual == noteVisuals_.end())
        {
            continue;
        }
        NoteVisualLayers& layers = visual->second;
        layers.root->SetVisible(false);
        if (layers.processing != nullptr)
        {
            layers.processing->SetVisible(false);
        }
        if (layers.success != nullptr)
        {
            layers.success->SetVisible(false);
        }
        if (layers.counter != nullptr)
        {
            layers.counter->SetVisible(false);
        }
    }
    presentedNoteIds_.clear();
}

void RhythmTestScene::PresentFocusCounter(
    NoteVisualLayers& layers,
    const finger_drum::rhythm::NoteProgress& progress,
    const float visualHeight)
{
    if (canvas_ == nullptr || layers.counter == nullptr ||
        layers.counterText == nullptr)
    {
        return;
    }

    const auto bounds = layers.counter->Bounds();
    layers.counter->SetBounds({
        -bounds.width * 0.5F,
        visualHeight * 0.5F - bounds.height * 0.28F,
        bounds.width,
        bounds.height});
    layers.counter->SetVisible(true);
    const std::size_t remaining = progress.required > progress.accepted
        ? progress.required - progress.accepted
        : 0;
    RequireComponent<mrg::visual2d::TextVisualComponent>(
        *layers.counterText).SetText(std::to_wstring(remaining));
}

void RhythmTestScene::PresentFocusNoteProcessing(
    NoteVisualLayers& layers,
    const finger_drum::rhythm::NoteProgress& progress,
    const finger_drum::rhythm::RhythmTime time)
{
    if (canvas_ == nullptr || layers.processing == nullptr ||
        progress.required == 0)
    {
        return;
    }

    const float completion = std::clamp(
        static_cast<float>(progress.accepted) /
            static_cast<float>(progress.required),
        0.0F,
        1.0F);
    const float scale = layers.focusType == FocusNoteType::Balloon
        ? 0.72F + completion * 0.38F
        : 1.0F;
    const mrg::visual2d::Size size{
        layers.processingSize.width * scale,
        layers.processingSize.height * scale};
    layers.processing->SetSize(size);
    layers.processing->SetPosition({0.0F, 0.0F});
    if (layers.focusType == FocusNoteType::DengDeng)
    {
        const float turns = static_cast<float>(time.count()) / 600'000.0F;
        layers.processing->Transform().SetRotationRollPitchYaw(
            0.0F,
            0.0F,
            std::fmod(turns * TwoPi, TwoPi));
    }
    RequireComponent<mrg::visual2d::SpriteVisualComponent>(
        *layers.processing).SetTint({1.0F, 1.0F, 1.0F, 1.0F});
    layers.processing->SetVisible(true);
    PresentFocusCounter(layers, progress, size.height);
}

void RhythmTestScene::PresentCompletionEffect(
    const finger_drum::rhythm::RhythmTime time)
{
    auto selected = completionEffects_.end();
    for (auto effect = completionEffects_.begin();
         effect != completionEffects_.end();)
    {
        const auto elapsed = time - effect->second;
        if (elapsed >= FocusSuccessDuration)
        {
            effect = completionEffects_.erase(effect);
            continue;
        }
        if (elapsed >= finger_drum::rhythm::RhythmDuration::zero() &&
            (selected == completionEffects_.end() ||
             effect->second > selected->second))
        {
            selected = effect;
        }
        ++effect;
    }
    if (selected == completionEffects_.end() || canvas_ == nullptr)
    {
        return;
    }

    const auto visual = noteVisuals_.find(selected->first);
    if (visual == noteVisuals_.end())
    {
        return;
    }
    NoteVisualLayers& layers = visual->second;
    presentedNoteIds_.push_back(selected->first);
    const float progress = std::clamp(
        static_cast<float>((time - selected->second).count()) /
            static_cast<float>(FocusSuccessDuration.count()),
        0.0F,
        1.0F);
    mrg::visual2d::Visual2DNode* node = layers.processing;
    mrg::visual2d::Size size = layers.processingSize;
    if (layers.focusType == FocusNoteType::Balloon &&
        layers.success != nullptr)
    {
        node = layers.success;
        const float burstScale = 0.82F + progress * 0.28F;
        size = {
            layers.successSize.width * burstScale,
            layers.successSize.height * burstScale};
    }
    if (node == nullptr)
    {
        return;
    }

    node->SetSize(size);
    node->SetPosition({0.0F, 0.0F});
    if (layers.focusType == FocusNoteType::DengDeng)
    {
        const float turns = static_cast<float>(time.count()) / 600'000.0F;
        node->Transform().SetRotationRollPitchYaw(
            0.0F,
            0.0F,
            std::fmod(turns * TwoPi, TwoPi));
    }
    RequireComponent<mrg::visual2d::SpriteVisualComponent>(*node).SetTint({
        1.0F,
        1.0F,
        1.0F,
        1.0F - progress});
    node->SetVisible(true);
}

void RhythmTestScene::UpdatePresentation(
    const finger_drum::rhythm::RhythmTime time)
{
    const auto snapshot = session_->Gear().BuildSnapshot(
        time,
        ApproachDuration,
        MissedTravelDuration);
    HideTransientNoteVisuals();
    for (TimedVisual& measureLine : measureLineVisuals_)
    {
        const auto delta = measureLine.timing - time;
        const bool visible = delta <= ApproachDuration &&
            delta >= -MissedTravelDuration;
        measureLine.node->SetVisible(visible);
        if (visible)
        {
            const float localY = judgementLocalY_ +
                static_cast<float>(delta.count()) /
                    static_cast<float>(ApproachDuration.count()) *
                    TravelDistance;
            measureLine.node->SetBounds({0.0F, localY, laneWidth_, 4.0F});
        }
    }
    bool focusClaimed{};
    for (const finger_drum::rhythm::ScrollNoteSnapshot& note : snapshot.notes)
    {
        const auto found = noteVisuals_.find(note.noteId);
        if (found == noteVisuals_.end())
        {
            continue;
        }
        NoteVisualLayers& layers = found->second;
        presentedNoteIds_.push_back(note.noteId);
        const finger_drum::mode::NotePresentationInfo* presentation =
            session_->FindNotePresentation(note.noteId);
        const bool focusNote = layers.focusType != FocusNoteType::None;
        const bool longNote = !focusNote && presentation != nullptr &&
            presentation->hasEndTime;
        float normalizedTravel = longNote
            ? static_cast<float>(note.timeFromJudgement.count()) /
                static_cast<float>(ApproachDuration.count())
            : std::clamp(note.normalizedTravel, -0.05F, 1.0F);
        const bool missed = note.state == NoteState::Missed;
        const bool completed = note.state == NoteState::Completed;
        const bool processing = focusNote && !missed && !completed &&
            time >= note.timing && note.progress.has_value() &&
            note.progress->accepted > 0;
        if (focusNote && time >= note.timing && !missed)
        {
            normalizedTravel = 0.0F;
        }
        else if (focusNote && missed)
        {
            normalizedTravel = static_cast<float>(
                (note.expireTime - time).count()) /
                static_cast<float>(ApproachDuration.count());
        }
        const float localY = judgementLocalY_ +
            normalizedTravel * TravelDistance;
        layers.root->SetPosition({laneWidth_ * 0.5F, localY});
        const bool showLaneHead = focusNote
            ? !completed && (!processing || missed)
            : !completed && !missed;
        layers.root->SetVisible(showLaneHead);

        if (processing && !focusClaimed)
        {
            PresentFocusNoteProcessing(layers, *note.progress, time);
            focusClaimed = true;
        }

        if (presentation != nullptr && presentation->hasEndTime &&
            layers.body != nullptr && layers.tail != nullptr)
        {
            const auto endDelta = presentation->endTime - time;
            const float endTravel = std::clamp(
                static_cast<float>(endDelta.count()) /
                    static_cast<float>(ApproachDuration.count()),
                -0.05F,
                1.65F);
            const float tailLocalY =
                judgementLocalY_ + endTravel * TravelDistance;
            const float length = std::max(tailLocalY - localY, 1.0F);
            const float bodyX =
                (layers.diameter - layers.bodyWidth) * 0.5F;
            layers.body->SetBounds({
                bodyX,
                layers.diameter * 0.5F,
                layers.bodyWidth,
                length});
            if (layers.bodyOverlay != nullptr)
            {
                layers.bodyOverlay->SetBounds({
                    bodyX,
                    layers.diameter * 0.5F,
                    layers.bodyWidth,
                    length});
            }
            const float tailX =
                (layers.diameter - layers.tailWidth) * 0.5F;
            layers.tail->SetSize({layers.tailWidth, layers.tailHeight});
            layers.tail->SetPosition({
                tailX + layers.tailWidth * 0.5F,
                length + layers.tailHeight * 0.5F});
            if (layers.tailOverlay != nullptr)
            {
                layers.tailOverlay->SetSize({
                    layers.tailWidth,
                    layers.tailHeight});
                layers.tailOverlay->SetPosition({
                    tailX + layers.tailWidth * 0.5F,
                    length + layers.tailHeight * 0.5F});
            }
        }
    }
    PresentCompletionEffect(time);
}

bool RhythmTestScene::IsPatternComplete() const noexcept
{
    if (session_ == nullptr || session_->Gear().Lanes().empty())
    {
        return false;
    }
    return std::ranges::all_of(
        session_->Gear().Lanes(),
        [](const std::unique_ptr<finger_drum::rhythm::Lane>& lane)
        {
            return lane != nullptr && lane->CurrentNote() == nullptr;
        });
}

bool RhythmTestScene::ReturnToLobby(
    mrg::scene::SceneManager& scenes) const
{
    return scenes.ChangeScene(finger_drum::scene_ids::Lobby);
}
