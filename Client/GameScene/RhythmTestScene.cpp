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
    using finger_drum::rhythm::JudgementGrade;
    using finger_drum::rhythm::NoteState;

    constexpr mrg::visual2d::Color DeepBlue{0.12F, 0.29F, 0.45F, 1.0F};
    constexpr mrg::visual2d::Color DonRed{0.95F, 0.25F, 0.29F, 1.0F};
    constexpr mrg::visual2d::Color KatBlue{0.18F, 0.58F, 0.95F, 1.0F};
    constexpr mrg::visual2d::Color RollGold{1.0F, 0.64F, 0.12F, 1.0F};
    constexpr float LaneWidth = 180.0F;
    constexpr float LaneLength = 1040.0F;
    constexpr float LaneCenterX = 640.0F;
    constexpr float LaneCenterY = 330.0F;
    constexpr float JudgementLocalY = 30.0F;
    constexpr float TravelDistance = 920.0F;
    constexpr float LaneBackgroundSourceWidth = 200.0F;
    constexpr float LaneBackgroundSourceHeight = 80.0F;
    constexpr float LaneLightLength = 900.0F;
    constexpr finger_drum::rhythm::RhythmDuration ApproachDuration{2'000'000};

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

    [[nodiscard]] std::wstring GradeName(const JudgementGrade grade)
    {
        switch (grade)
        {
        case JudgementGrade::Max: return L"MAX";
        case JudgementGrade::Perfect: return L"PERFECT";
        case JudgementGrade::Great: return L"GREAT";
        case JudgementGrade::Good: return L"GOOD";
        case JudgementGrade::Bad: return L"BAD";
        case JudgementGrade::Miss: return L"MISS";
        default: return L"--";
        }
    }

    [[nodiscard]] std::wstring_view StateName(const NoteState state) noexcept
    {
        switch (state)
        {
        case NoteState::Pending: return L"Pending";
        case NoteState::Active: return L"Active";
        case NoteState::AwaitingAdditionalInput: return L"Awaiting input";
        case NoteState::Holding: return L"Holding";
        case NoteState::Completed: return L"Completed";
        case NoteState::Missed: return L"Missed";
        default: return L"Unknown";
        }
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
            visualId == "Taiko.BigRoll" || visualId == "Taiko.Balloon";
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
        if (visualId == "Taiko.Roll" || visualId == "Taiko.BigRoll")
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
    audioRouter_.Update();

    const auto currentTime = timer_.Now(clock.performanceCounterTicks);
    UpdateDebugText(currentTime);

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
        RequireComponent<mrg::visual2d::TextVisualComponent>(*resultLabel_).
            SetText(std::format(
                L"PATTERN COMPLETE   RETURNING IN {:.1f}",
                std::max(3.0 - completedElapsedSeconds_, 0.0)));
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
    }
}

void RhythmTestScene::Shutdown() noexcept
{
    timer_.Stop();
    audioRouter_.Shutdown();
    noteVisuals_.clear();
    laneRoot_ = nullptr;
    audioStatusLabel_ = nullptr;
    debugLabel_ = nullptr;
    resultLabel_ = nullptr;
    timelineLabel_ = nullptr;
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
    addLong(2, 0, NoteType::Balloon, {"HitCount=8"});
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
    auto& root = canvas_->CreateNode(
        mrg::visual2d::Anchor::Center,
        "RhythmTest.Root");
    root.SetPivot({0.5F, 0.5F});
    root.SetSize({1280.0F, 720.0F});

    auto& background = mrg::visual2d::CreatePanel(
        root,
        {0.0F, 0.0F, 1280.0F, 720.0F},
        "Background");
    SetColor(background, {0.918F, 0.965F, 1.0F, 1.0F});

    auto& header = mrg::visual2d::CreateLabel(
        root,
        {48.0F, 34.0F, 1184.0F, 52.0F},
        L"FINGERDRUM / TAIKO SINGLE-LANE DRIVER",
        "Header");
    auto& headerText = RequireComponent<mrg::visual2d::TextVisualComponent>(header);
    headerText.SetFontSize(24.0F);
    headerText.SetTextColor(DeepBlue);

    CreateLaneVisuals(services, root);

    timelineLabel_ = &mrg::visual2d::CreateLabel(
        root, {48.0F, 98.0F, 500.0F, 40.0F}, L"TIME", "Timeline");
    resultLabel_ = &mrg::visual2d::CreateLabel(
        root, {48.0F, 146.0F, 820.0F, 48.0F}, L"READY", "Result");
    audioStatusLabel_ = &mrg::visual2d::CreateLabel(
        root, {48.0F, 650.0F, 1184.0F, 34.0F},
        debugMode_
            ? L"AUDIO: manual timeline hitsounds (music disabled)"
            : L"AUDIO: DSP-clock scheduled music and hitsounds",
        "AudioStatus");
    for (mrg::visual2d::Visual2DNode* label :
        {timelineLabel_, resultLabel_, audioStatusLabel_})
    {
        auto& text = RequireComponent<mrg::visual2d::TextVisualComponent>(*label);
        text.SetTextColor(DeepBlue);
        text.SetFontSize(label == resultLabel_ ? 24.0F : 17.0F);
    }

    if (debugMode_)
    {
        debugLabel_ = &mrg::visual2d::CreateLabel(
            root,
            {760.0F, 98.0F, 472.0F, 126.0F},
            L"DEBUG",
            "DebugStatus");
        auto& debugText =
            RequireComponent<mrg::visual2d::TextVisualComponent>(*debugLabel_);
        debugText.SetTextColor(DeepBlue);
        debugText.SetFontSize(17.0F);
    }

    auto& instructions = mrg::visual2d::CreateLabel(
        root,
        {120.0F, 478.0F, 1040.0F, 116.0F},
        debugMode_
            ? L"D / K : KAT        F / J : DON        SPACE : Run / Pause\n"
              L"1 / 2 : Rewind / Advance continuously    3 / 4 : -1 ms / +1 ms\n"
              L"- / + : Debug speed    R : Restart    ESC : Song Select"
            : L"D / K : KAT (rim)        F / J : DON (center)\n"
              L"One Lane focuses Don, Kat, large notes and rolls in exact time order.\n"
              L"SPACE : Pause / Resume     R : Restart     ESC : Song Select",
        "Instructions");
    auto& instructionText =
        RequireComponent<mrg::visual2d::TextVisualComponent>(instructions);
    instructionText.SetFontSize(18.0F);
    instructionText.SetTextColor(DeepBlue);
    instructionText.SetHorizontalAlignment(mrg::visual2d::TextAlignment::Center);
}

void RhythmTestScene::CreateLaneVisuals(
    const mrg::EngineServices& services,
    mrg::visual2d::Visual2DNode& sceneRoot)
{
    // Author the lane in its reusable default orientation: local +Y is the
    // future-note direction and every lane-owned visual is a descendant.
    laneRoot_ = &sceneRoot.CreateChild("ScrollGear.Lane");
    laneRoot_->SetPivot({0.5F, 0.5F});
    laneRoot_->SetSize({LaneWidth, LaneLength});
    laneRoot_->SetPosition({LaneCenterX, LaneCenterY});
    laneRoot_->SetZIndex(1);

    // Taiko is a presentation of the same vertical lane rotated clockwise.
    // Notes continue to update only local Y, so changing this one transform
    // redirects the background, effects, judgement line and notes together.
    laneRoot_->Transform().SetRotationRollPitchYaw(
        0.0F,
        0.0F,
        -DirectX::XM_PIDIV2);

    CreateLaneBackgroundTiles(services);

    auto& laneLight = mrg::visual2d::CreateSprite(
        *laneRoot_,
        {0.0F, JudgementLocalY, LaneWidth, LaneLightLength},
        services.visual2DRendering.LoadImage(SkinAssetPath(L"LaneLight.png")),
        "Lane.Light");
    laneLight.SetZIndex(1);

    constexpr float judgementDiameter = 114.0F;
    auto& judgementLine = mrg::visual2d::CreateSprite(
        *laneRoot_,
        {(LaneWidth - judgementDiameter) * 0.5F,
         JudgementLocalY - judgementDiameter * 0.5F,
         judgementDiameter,
         judgementDiameter},
        services.visual2DRendering.LoadImage(SkinAssetPath(L"JudgeLine.png")),
        "Lane.JudgementLine");
    judgementLine.SetZIndex(2);
}

void RhythmTestScene::CreateLaneBackgroundTiles(
    const mrg::EngineServices& services)
{
    if (laneRoot_ == nullptr)
    {
        throw std::logic_error("Lane background requires a Lane root.");
    }

    const mrg::visual2d::ImageHandle image =
        services.visual2DRendering.LoadImage(SkinAssetPath(L"LaneBackground.png"));
    const float tileHeight = LaneWidth *
        LaneBackgroundSourceHeight / LaneBackgroundSourceWidth;
    std::size_t tileIndex{};
    for (float localY = 0.0F; localY < LaneLength; localY += tileHeight)
    {
        const float visibleHeight = std::min(tileHeight, LaneLength - localY);
        auto& tile = mrg::visual2d::CreateSprite(
            *laneRoot_,
            {0.0F, localY, LaneWidth, visibleHeight},
            image,
            std::format("Lane.BackgroundTile.{}", tileIndex++));
        tile.SetZIndex(0);
        if (visibleHeight < tileHeight)
        {
            RequireComponent<mrg::visual2d::SpriteVisualComponent>(tile).
                SetUvTransform(
                    {1.0F, visibleHeight / tileHeight},
                    {0.0F, 0.0F});
        }
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
            const float diameter = big ? 76.0F : 56.0F;
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
                layers.body = &mrg::visual2d::CreateSprite(
                    *layers.root,
                    {diameter * 0.25F, diameter * 0.5F, diameter * 0.5F, 1.0F},
                    bodyImage,
                    "AmbientBody");
                RequireComponent<mrg::visual2d::SpriteVisualComponent>(
                    *layers.body).SetTint(ambientColor);
                layers.body->SetZIndex(0);
                layers.tail = &mrg::visual2d::CreateSprite(
                    *layers.root,
                    {0.0F, 0.0F, diameter, diameter},
                    tailImage,
                    "AmbientTail");
                RequireComponent<mrg::visual2d::SpriteVisualComponent>(
                    *layers.tail).SetTint(ambientColor);
                layers.tail->SetZIndex(1);
            }

            layers.ambient = &mrg::visual2d::CreateSprite(
                *layers.root,
                {0.0F, 0.0F, diameter, diameter},
                big ? bigNoteImage : noteImage,
                "AmbientHead");
            RequireComponent<mrg::visual2d::SpriteVisualComponent>(
                *layers.ambient).SetTint(ambientColor);
            layers.ambient->SetZIndex(2);
            layers.overlay = &mrg::visual2d::CreateSprite(
                *layers.root,
                {0.0F, 0.0F, diameter, diameter},
                big ? bigOverlay : noteOverlay,
                "UntintedOverlay");
            RequireComponent<mrg::visual2d::SpriteVisualComponent>(
                *layers.overlay).SetTint({1.0F, 1.0F, 1.0F, 1.0F});
            layers.overlay->SetZIndex(3);
            noteVisuals_.emplace(note->Id(), layers);
        }
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
    if (!audioError.empty() && audioStatusLabel_ != nullptr)
    {
        RequireComponent<mrg::visual2d::TextVisualComponent>(*audioStatusLabel_).
            SetText(L"AUDIO: " + std::wstring(audioError.begin(), audioError.end()));
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
    if (debugMode_)
    {
        timer_.Pause(clock.performanceCounterTicks);
    }
}

void RhythmTestScene::ResetTimeline(
    const mrg::audio::AudioClockSnapshot& clock)
{
    session_->Reset();
    audioRouter_.StopAllVoices();
    acceptedHitCount_ = 0;
    accumulatedScore_ = 0.0;
    completedElapsedSeconds_ = 0.0;
    StartTimeline(clock);
    if (!debugMode_)
    {
        ScheduleMusic();
    }
    RequireComponent<mrg::visual2d::TextVisualComponent>(*resultLabel_).
        SetText(L"RESTARTED");
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
    if (!debugMode_)
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
        acceptedHitCount_ = 0;
        accumulatedScore_ = 0.0;
        RequireComponent<mrg::visual2d::TextVisualComponent>(*resultLabel_).
            SetText(L"DEBUG REWIND / NOTE STATE RESET");
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
        if (event.code != 'D' && event.code != 'F' &&
            event.code != 'J' && event.code != 'K')
        {
            continue;
        }
        const auto edge = event.type == mrg::platform::InputEventType::KeyPressed
            ? finger_drum::rhythm::InputEdge::Pressed
            : finger_drum::rhythm::InputEdge::Released;
        ConsumeResult(session_->ProcessInput(
            event.code,
            edge,
            timer_.Now(event.performanceCounterTicks)));
    }
}

void RhythmTestScene::UpdateSession(
    const mrg::platform::InputState& input,
    const finger_drum::rhythm::RhythmTime time)
{
    std::array<finger_drum::rhythm::PhysicalKey, 4> heldStorage{};
    std::size_t heldCount{};
    for (const std::uint16_t key : {'D', 'F', 'J', 'K'})
    {
        if (input.IsKeyDown(key))
        {
            heldStorage[heldCount++] = key;
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
    for (const finger_drum::rhythm::NoteEvent& event : result.events)
    {
        if (event.type == finger_drum::rhythm::NoteEventType::HitAccepted ||
            event.type == finger_drum::rhythm::NoteEventType::TickAccepted)
        {
            ++acceptedHitCount_;
            accumulatedScore_ += event.judgement.scoreRate * 100.0;
            const double average = accumulatedScore_ /
                static_cast<double>(acceptedHitCount_);
            RequireComponent<mrg::visual2d::TextVisualComponent>(*resultLabel_).
                SetText(std::format(
                    L"{}   {:+.3f} ms   AVG {:.2f}",
                    GradeName(event.judgement.grade),
                    static_cast<double>(event.judgement.signedError.count()) /
                        1000.0,
                    average));
        }
        else if (event.type == finger_drum::rhythm::NoteEventType::Missed)
        {
            RequireComponent<mrg::visual2d::TextVisualComponent>(*resultLabel_).
                SetText(L"MISS");
        }
    }
}

void RhythmTestScene::UpdatePresentation(
    const finger_drum::rhythm::RhythmTime time)
{
    RequireComponent<mrg::visual2d::TextVisualComponent>(*timelineLabel_).
        SetText(std::format(
            L"TIME {:+.3f} s   /   ONE LANE",
            static_cast<double>(time.count()) / 1'000'000.0));

    const auto snapshot = session_->Gear().BuildSnapshot(
        time,
        ApproachDuration,
        finger_drum::rhythm::RhythmDuration{220'000});
    for (auto& [id, layers] : noteVisuals_)
    {
        static_cast<void>(id);
        layers.root->SetVisible(false);
    }
    for (const finger_drum::rhythm::ScrollNoteSnapshot& note : snapshot.notes)
    {
        const auto found = noteVisuals_.find(note.noteId);
        if (found == noteVisuals_.end())
        {
            continue;
        }
        NoteVisualLayers& layers = found->second;
        const float localY = JudgementLocalY +
            std::clamp(note.normalizedTravel, -0.05F, 1.0F) * TravelDistance;
        layers.root->SetPosition({LaneWidth * 0.5F, localY});
        layers.root->SetVisible(
            note.state != NoteState::Completed &&
            note.state != NoteState::Missed);

        const finger_drum::mode::NotePresentationInfo* presentation =
            session_->FindNotePresentation(note.noteId);
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
                layers.diameter * 0.25F,
                layers.diameter * 0.5F,
                layers.diameter * 0.5F,
                length});
            layers.tail->SetBounds({
                0.0F,
                length,
                layers.diameter,
                layers.diameter});
        }
    }
}

void RhythmTestScene::UpdateDebugText(
    const finger_drum::rhythm::RhythmTime time)
{
    if (!debugMode_ || debugLabel_ == nullptr || session_ == nullptr ||
        session_->Gear().Lanes().empty())
    {
        return;
    }

    const finger_drum::rhythm::INote* const note =
        session_->Gear().Lanes().front()->CurrentNote();
    std::wstring text = std::format(
        L"DEBUG TIMER: {:+.3f} ms\nSPEED: {:.0f} ms/s    HITS: {}",
        static_cast<double>(time.count()) / 1000.0,
        debugSpeedMillisecondsPerSecond_,
        acceptedHitCount_);
    if (note == nullptr)
    {
        text += L"\nTARGET: none";
    }
    else
    {
        const finger_drum::mode::NotePresentationInfo* presentation =
            session_->FindNotePresentation(note->Id());
        const std::string_view visual = presentation == nullptr
            ? std::string_view{"Unknown"}
            : std::string_view{presentation->visualId};
        const std::wstring visualText(visual.begin(), visual.end());
        const auto difference = time - note->Timing();
        text += std::format(
            L"\nTARGET: #{} {} / {} / DIFF {:+.3f} ms",
            note->Id(),
            visualText,
            StateName(note->State()),
            static_cast<double>(difference.count()) / 1000.0);
    }
    RequireComponent<mrg::visual2d::TextVisualComponent>(*debugLabel_).
        SetText(std::move(text));
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
