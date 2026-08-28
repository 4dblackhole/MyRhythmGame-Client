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
    constexpr float LaneWidth = 180.0F;
    constexpr float LaneScreenLeft = 394.0F;
    constexpr float LaneScreenRightMargin = 40.0F;
    constexpr float LaneCenterY = 360.0F;
    constexpr float JudgementLocalY = 30.0F;
    constexpr float TravelDistance = 920.0F;
    // The legacy Taiko presentation uses 90/144 logical-pixel circles at
    // 1280x720. With this approach time, 180 BPM sixteenth notes are 85 px
    // apart, leaving only their white outlines slightly overlapped.
    constexpr float CircleDiameter = 90.0F;
    constexpr float LargeCircleDiameter = 144.0F;
    constexpr float TickDiameter = CircleDiameter * 3.0F / 7.0F;
    constexpr finger_drum::rhythm::RhythmDuration ApproachDuration{900'000};

    [[nodiscard]] float LaneLengthForWidth(const float logicalWidth) noexcept
    {
        return std::max(
            logicalWidth - LaneScreenLeft - LaneScreenRightMargin,
            1.0F);
    }

    [[nodiscard]] float ImageHeightRatio(
        const mrg::visual2d::Size imageSize)
    {
        if (imageSize.width <= 0.0F || imageSize.height <= 0.0F)
        {
            throw std::logic_error(
                "A long-note image must have a non-zero native size.");
        }
        return imageSize.height / imageSize.width;
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

    [[nodiscard]] std::filesystem::path SoundAssetPath(
        const std::filesystem::path& file)
    {
        return mrg::platform::ResolveExecutableRelativePath(
            std::filesystem::path(L"assets\\sounds") / file);
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
    const bool debugMode)
    : launchRequest_(std::move(launchRequest)),
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
    canvas_ = std::make_unique<mrg::visual2d::Visual2DCanvas>(
        mrg::visual2d::Size{1280.0F, 720.0F},
        mrg::visual2d::CanvasScaleMode::FixedHeight);
    canvas_->SetViewportSize({
        static_cast<float>(width_),
        static_cast<float>(height_)});
    CreatePresentation(services);
    CreateNoteVisuals(services);
    InitializeAudio(services);
    StartTimeline(services.audio.CaptureClockSnapshot());
    if (!debugMode_)
    {
        ScheduleMusic();
    }
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
    canvas_->Update(context.deltaSeconds);
}

void RhythmTestScene::Render(const mrg::graphics::RenderContext& context)
{
    if (canvas_ != nullptr && context.visual2DRendering != nullptr)
    {
        context.visual2DRendering->SubmitScreen(*canvas_, context);
    }
}

void RhythmTestScene::OnResize(
    const std::uint32_t width,
    const std::uint32_t height)
{
    width_ = width;
    height_ = height;
    if (canvas_ != nullptr)
    {
        canvas_->SetViewportSize({
            static_cast<float>(width_),
            static_cast<float>(height_)});
        UpdatePresentationLayout();
    }
}

void RhythmTestScene::Shutdown() noexcept
{
    timer_.Stop();
    audioRouter_.Shutdown();
    noteVisuals_.clear();
    measureLineVisuals_.clear();
    keyIndicators_.fill(nullptr);
    keyGlows_.fill(nullptr);
    background_ = nullptr;
    scrollGearBorder_ = nullptr;
    scrollGearSurface_ = nullptr;
    inputPresentationRoot_ = nullptr;
    laneRoot_ = nullptr;
    laneSurface_ = nullptr;
    laneCenterGuide_ = nullptr;
    canvas_.reset();
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

void RhythmTestScene::CreatePresentation(
    const mrg::EngineServices& services)
{
    auto& root = canvas_->Root();

    background_ = &mrg::visual2d::CreatePanel(
        root,
        {0.0F, 0.0F, CanvasReferenceWidth, CanvasReferenceHeight},
        "Background");
    SetColor(*background_, {0.941F, 0.973F, 1.0F, 1.0F});

    scrollGearBorder_ = &mrg::visual2d::CreatePanel(
        root,
        {40.0F, 230.0F, 1200.0F, 260.0F},
        "ScrollGear.Border");
    SetColor(*scrollGearBorder_, {0.49F, 0.66F, 1.0F, 1.0F});
    scrollGearSurface_ = &mrg::visual2d::CreatePanel(
        root,
        {46.0F, 236.0F, 1188.0F, 248.0F},
        "ScrollGear.Surface");
    SetColor(*scrollGearSurface_, {0.66F, 0.78F, 1.0F, 1.0F});

    CreateLaneVisuals(services, root);
    inputPresentationRoot_ = &root.CreateChild("InputPresentation");
    inputPresentationRoot_->SetZIndex(2);
    auto& inputFrame = mrg::visual2d::CreatePanel(
        *inputPresentationRoot_,
        {62.0F, 246.0F, 332.0F, 220.0F},
        "Input.Layout.Frame");
    SetColor(inputFrame, {0.01F, 0.02F, 0.04F, 1.0F});
    inputFrame.SetZIndex(0);
    auto& inputSurface = mrg::visual2d::CreatePanel(
        *inputPresentationRoot_,
        {66.0F, 250.0F, 324.0F, 212.0F},
        "Input.Layout.Surface");
    SetColor(inputSurface, {0.985F, 0.99F, 1.0F, 1.0F});
    inputSurface.SetZIndex(1);
    CreateKeyIndicators(*inputPresentationRoot_);

    UpdatePresentationLayout();
}

void RhythmTestScene::CreateLaneVisuals(
    const mrg::EngineServices& services,
    mrg::visual2d::Visual2DNode& sceneRoot)
{
    // Author the lane in its reusable default orientation: local +Y is the
    // future-note direction and every lane-owned visual is a descendant.
    const float laneLength = LaneLengthForWidth(canvas_->LogicalSize().width);
    laneRoot_ = &sceneRoot.CreateChild("ScrollGear.Lane");
    laneRoot_->SetPivot({0.5F, 0.5F});
    laneRoot_->SetSize({LaneWidth, laneLength});
    laneRoot_->SetPosition({
        LaneScreenLeft + laneLength * 0.5F,
        LaneCenterY});
    laneRoot_->SetZIndex(1);

    // Taiko is a presentation of the same vertical lane rotated clockwise.
    // Notes continue to update only local Y, so changing this one transform
    // redirects the background, effects, judgement line and notes together.
    laneRoot_->Transform().SetRotationRollPitchYaw(
        0.0F,
        0.0F,
        -DirectX::XM_PIDIV2);

    CreateLaneSurface(services);
    CreateMeasureLineVisuals(services);

    auto& judgementLine = mrg::visual2d::CreateSprite(
        *laneRoot_,
        {(LaneWidth - LargeCircleDiameter) * 0.5F,
         JudgementLocalY - LargeCircleDiameter * 0.5F,
         LargeCircleDiameter,
         LargeCircleDiameter},
        services.visual2DRendering.LoadImage(SkinAssetPath(L"JudgeLine.png")),
        "Lane.JudgementLine");
    judgementLine.SetZIndex(4);
}

void RhythmTestScene::CreateLaneSurface(
    const mrg::EngineServices&)
{
    if (laneRoot_ == nullptr)
    {
        throw std::logic_error("Lane background requires a Lane root.");
    }

    const float laneLength = LaneLengthForWidth(canvas_->LogicalSize().width);
    laneSurface_ = &mrg::visual2d::CreatePanel(
        *laneRoot_,
        {0.0F, 0.0F, LaneWidth, laneLength},
        "Lane.DarkSurface");
    SetColor(*laneSurface_, {0.067F, 0.098F, 0.298F, 1.0F});
    laneSurface_->SetZIndex(0);

    laneCenterGuide_ = &mrg::visual2d::CreatePanel(
        *laneRoot_,
        {LaneWidth * 0.5F - 1.0F, 0.0F, 2.0F, laneLength},
        "Lane.CenterGuide");
    SetColor(*laneCenterGuide_, {0.78F, 0.84F, 1.0F, 0.16F});
    laneCenterGuide_->SetZIndex(1);
}

void RhythmTestScene::CreateMeasureLineVisuals(
    const mrg::EngineServices& services)
{
    const auto image = services.visual2DRendering.LoadImage(
        SkinAssetPath(L"MeasureLine.png"));
    for (const finger_drum::rhythm::RhythmTime timing :
        session_->MeasureLines())
    {
        auto& line = mrg::visual2d::CreateSprite(
            *laneRoot_,
            {0.0F, 0.0F, LaneWidth, 8.0F},
            image,
            "Lane.MeasureLine");
        RequireComponent<mrg::visual2d::SpriteVisualComponent>(line).
            SetTint({0.78F, 0.85F, 0.90F, 0.72F});
        line.SetZIndex(1);
        line.SetVisible(false);
        measureLineVisuals_.push_back({timing, &line});
    }
}

void RhythmTestScene::CreateKeyIndicators(
    mrg::visual2d::Visual2DNode& sceneRoot)
{
    struct KeySpec
    {
        mrg::visual2d::Rect bounds;
        mrg::visual2d::Color baseColor;
    };
    constexpr std::array<KeySpec, 4> keys{{
        {{90.0F, 278.0F, 56.0F, 96.0F}, InputKeyBaseColors[0]},
        {{165.0F, 342.0F, 62.0F, 96.0F}, InputKeyBaseColors[1]},
        {{241.0F, 342.0F, 62.0F, 96.0F}, InputKeyBaseColors[2]},
        {{320.0F, 272.0F, 56.0F, 96.0F}, InputKeyBaseColors[3]},
    }};
    for (std::size_t index = 0; index < keys.size(); ++index)
    {
        const mrg::visual2d::Rect& bounds = keys[index].bounds;
        auto& glow = mrg::visual2d::CreatePanel(
            sceneRoot,
            {bounds.x - 9.0F,
             bounds.y - 9.0F,
             bounds.width + 18.0F,
             bounds.height + 18.0F},
            "Input.Key.Glow");
        SetColor(glow, {keys[index].baseColor.red,
                        keys[index].baseColor.green,
                        keys[index].baseColor.blue,
                        0.0F});
        glow.SetZIndex(2);
        keyGlows_[index] = &glow;

        auto& frame = mrg::visual2d::CreatePanel(
            sceneRoot,
            bounds,
            "Input.Key.Frame");
        SetColor(frame, {0.015F, 0.026F, 0.045F, 1.0F});
        frame.SetZIndex(3);

        auto& face = mrg::visual2d::CreatePanel(
            frame,
            {4.0F, 4.0F, bounds.width - 8.0F, bounds.height - 8.0F},
            "Input.Key.Face");
        SetColor(face, keys[index].baseColor);
        keyIndicators_[index] = &face;
    }
}

void RhythmTestScene::CreateNoteVisuals(
    const mrg::EngineServices& services)
{
    const auto noteImage = services.visual2DRendering.LoadImage(
        SkinAssetPath(L"note.png"));
    const auto noteOverlay = services.visual2DRendering.LoadImage(
        SkinAssetPath(L"noteoverlay.png"));
    const auto bigNoteImage = services.visual2DRendering.LoadImage(
        SkinAssetPath(L"bignote.png"));
    const auto bigOverlay = services.visual2DRendering.LoadImage(
        SkinAssetPath(L"bigcircleoverlay.png"));
    const auto bodyImage = services.visual2DRendering.LoadImage(
        SkinAssetPath(L"LNBody.png"));
    const auto tailImage = services.visual2DRendering.LoadImage(
        SkinAssetPath(L"LNTail.png"));
    const float tailHeightRatio = ImageHeightRatio(
        services.visual2DRendering.GetImageSize(tailImage));
    const auto tickImage = services.visual2DRendering.LoadImage(
        SkinAssetPath(L"TickMarker.png"));
    const auto balloonImage = services.visual2DRendering.LoadImage(
        SkinAssetPath(L"Balloon.png"));
    const auto dengDengImage = services.visual2DRendering.LoadImage(
        SkinAssetPath(L"DengDeng.png"));

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
            const bool longNote = presentation != nullptr &&
                presentation->hasEndTime && IsLongVisual(visualId);
            const bool balloon = visualId == "Taiko.Balloon";
            const bool dengDeng = visualId == "Taiko.DengDeng";
            const bool customHead = balloon || dengDeng;
            const float diameter = big ? LargeCircleDiameter : CircleDiameter;
            const mrg::visual2d::Color ambientColor = AmbientColor(visualId);

            NoteVisualLayers layers;
            layers.root = &laneRoot_->CreateChild(
                std::format("Lane.Note.{}", note->Id()));
            layers.root->SetPivot({0.5F, 0.5F});
            layers.root->SetSize({diameter, diameter});
            layers.root->SetPosition({LaneWidth * 0.5F, JudgementLocalY});
            layers.root->SetZIndex(3);
            layers.root->SetVisible(false);
            layers.diameter = diameter;

            // Body and tail are Ambient-colored and sit behind the circular
            // head. The white head overlay is a separate untinted draw packet.
            if (longNote)
            {
                layers.tailHeightRatio = tailHeightRatio;
                layers.body = &mrg::visual2d::CreateSprite(
                    *layers.root,
                    {0.0F, diameter * 0.5F, diameter, 1.0F},
                    bodyImage,
                    "AmbientBody");
                RequireComponent<mrg::visual2d::SpriteVisualComponent>(
                    *layers.body).SetTint(ambientColor);
                layers.body->SetZIndex(0);
                layers.tail = &mrg::visual2d::CreateSprite(
                    *layers.root,
                    {0.0F,
                     0.0F,
                     diameter,
                     diameter * layers.tailHeightRatio},
                    tailImage,
                    "AmbientTail");
                RequireComponent<mrg::visual2d::SpriteVisualComponent>(
                    *layers.tail).SetTint(ambientColor);
                layers.tail->SetZIndex(1);

                if (presentation != nullptr)
                {
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
                            {(diameter - TickDiameter) * 0.5F,
                             diameter * 0.5F + offset - TickDiameter * 0.5F,
                             TickDiameter,
                             TickDiameter},
                            tickImage,
                            "LongNote.Tick");
                        tick.SetZIndex(2);
                        layers.ticks.push_back({tickTime, &tick});
                    }
                }
            }

            const auto headImage = balloon
                ? balloonImage
                : (dengDeng
                    ? dengDengImage
                    : (big ? bigNoteImage : noteImage));
            layers.ambient = &mrg::visual2d::CreateSprite(
                *layers.root,
                {0.0F, 0.0F, diameter, diameter},
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
                    {0.0F, 0.0F, diameter, diameter},
                    big ? bigOverlay : noteOverlay,
                    "UntintedOverlay");
                RequireComponent<mrg::visual2d::SpriteVisualComponent>(
                    *layers.overlay).SetTint({1.0F, 1.0F, 1.0F, 1.0F});
                layers.overlay->SetZIndex(4);
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
    const auto widthBetween =
        [logicalWidth](const float horizontalMargin) noexcept
        {
            return std::max(logicalWidth - horizontalMargin * 2.0F, 1.0F);
        };

    if (background_ != nullptr)
    {
        background_->SetBounds({
            0.0F,
            0.0F,
            logicalWidth,
            CanvasReferenceHeight});
    }
    if (scrollGearBorder_ != nullptr)
    {
        scrollGearBorder_->SetBounds({
            40.0F,
            230.0F,
            widthBetween(40.0F),
            260.0F});
    }
    if (scrollGearSurface_ != nullptr)
    {
        scrollGearSurface_->SetBounds({
            46.0F,
            236.0F,
            widthBetween(46.0F),
            248.0F});
    }
    if (laneRoot_ != nullptr)
    {
        const float laneLength = LaneLengthForWidth(logicalWidth);
        laneRoot_->SetSize({LaneWidth, laneLength});
        laneRoot_->SetPosition({
            LaneScreenLeft + laneLength * 0.5F,
            LaneCenterY});
        if (laneSurface_ != nullptr)
        {
            laneSurface_->SetBounds({0.0F, 0.0F, LaneWidth, laneLength});
        }
        if (laneCenterGuide_ != nullptr)
        {
            laneCenterGuide_->SetBounds({
                LaneWidth * 0.5F - 1.0F,
                0.0F,
                2.0F,
                laneLength});
        }
    }
    if (inputPresentationRoot_ != nullptr)
    {
        inputPresentationRoot_->SetPosition({0.0F, 0.0F});
    }
}

void RhythmTestScene::InitializeAudio(const mrg::EngineServices& services)
{
    std::string audioError;
    if (audioRouter_.Initialize(services.audio, audioError))
    {
        RegisterTaikoSounds(audioError);
        if (audioError.empty() && launchRequest_->IsValid())
        {
            musicRegistered_ = audioRouter_.RegisterSound(
                "Music.Track",
                launchRequest_->musicPath,
                mrg::audio::AudioLoadMode::Stream,
                audioError);
        }
    }
}

void RhythmTestScene::RegisterTaikoSounds(std::string& errorMessage)
{
    struct SoundRegistration
    {
        std::string_view id;
        std::wstring_view file;
    };
    constexpr std::array<SoundRegistration, 7> registrations{{
        {"Taiko.Don.Hit", L"don.wav"},
        {"Taiko.Kat.Hit", L"kat.wav"},
        {"Taiko.BigDon.FirstHit", L"bigdon.wav"},
        {"Taiko.BigKat.FirstHit", L"bigkat.wav"},
        {"Taiko.LongNote.Tick", L"don.wav"},
        {"Taiko.Don.FreeInput", L"don.wav"},
        {"Taiko.Kat.FreeInput", L"kat.wav"},
    }};
    for (const SoundRegistration& registration : registrations)
    {
        if (!audioRouter_.RegisterSound(
                std::string(registration.id),
                SkinAssetPath(registration.file),
                mrg::audio::AudioLoadMode::Sample,
                errorMessage))
        {
            return;
        }
    }
    static_cast<void>(audioRouter_.RegisterSound(
        "Taiko.Balloon.Pop",
        SoundAssetPath(L"pop.wav"),
        mrg::audio::AudioLoadMode::Sample,
        errorMessage));
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
    audioRouter_.Route(cues, timer_);
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
    session_->Reset();
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
        session_->Reset();
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
        const mrg::visual2d::Color& faceColor = primaryPressed
            ? InputKeyStrongColors[index]
            : (secondaryPressed
                ? InputKeyWeakColors[index]
                : InputKeyBaseColors[index]);
        const float glowOpacity = primaryPressed
            ? 0.82F
            : (secondaryPressed ? 0.38F : 0.0F);
        if (keyIndicators_[index] != nullptr)
        {
            SetColor(*keyIndicators_[index], faceColor);
        }
        if (keyGlows_[index] != nullptr)
        {
            SetColor(*keyGlows_[index], {
                faceColor.red,
                faceColor.green,
                faceColor.blue,
                glowOpacity});
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
    audioRouter_.Route(result.audioCues, timer_);
}

void RhythmTestScene::UpdatePresentation(
    const finger_drum::rhythm::RhythmTime time)
{
    const auto snapshot = session_->Gear().BuildSnapshot(
        time,
        ApproachDuration,
        finger_drum::rhythm::RhythmDuration{220'000});
    for (auto& [id, layers] : noteVisuals_)
    {
        static_cast<void>(id);
        layers.root->SetVisible(false);
    }
    for (TimedVisual& measureLine : measureLineVisuals_)
    {
        const auto delta = measureLine.timing - time;
        const bool visible = delta <= ApproachDuration &&
            delta >= finger_drum::rhythm::RhythmDuration{-220'000};
        measureLine.node->SetVisible(visible);
        if (visible)
        {
            const float localY = JudgementLocalY +
                static_cast<float>(delta.count()) /
                    static_cast<float>(ApproachDuration.count()) *
                    TravelDistance;
            measureLine.node->SetBounds({0.0F, localY, LaneWidth, 8.0F});
        }
    }
    for (const finger_drum::rhythm::ScrollNoteSnapshot& note : snapshot.notes)
    {
        const auto found = noteVisuals_.find(note.noteId);
        if (found == noteVisuals_.end())
        {
            continue;
        }
        NoteVisualLayers& layers = found->second;
        const finger_drum::mode::NotePresentationInfo* presentation =
            session_->FindNotePresentation(note.noteId);
        const bool longNote = presentation != nullptr &&
            presentation->hasEndTime;
        const float normalizedTravel = longNote
            ? static_cast<float>(note.timeFromJudgement.count()) /
                static_cast<float>(ApproachDuration.count())
            : std::clamp(note.normalizedTravel, -0.05F, 1.0F);
        const float localY = JudgementLocalY +
            normalizedTravel * TravelDistance;
        layers.root->SetPosition({LaneWidth * 0.5F, localY});
        layers.root->SetVisible(
            note.state != NoteState::Completed &&
            note.state != NoteState::Missed);

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
                JudgementLocalY + endTravel * TravelDistance;
            const float length = std::max(tailLocalY - localY, 1.0F);
            layers.body->SetBounds({
                0.0F,
                layers.diameter * 0.5F,
                layers.diameter,
                length});
            layers.tail->SetBounds({
                0.0F,
                length,
                layers.diameter,
                layers.diameter * layers.tailHeightRatio});
        }
    }
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
