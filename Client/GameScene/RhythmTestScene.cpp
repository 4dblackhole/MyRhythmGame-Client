#include "RhythmTestScene.h"

#include "GameFlow/FingerDrumSceneIds.h"
#include "Model/ChartDocument.h"
#include "Taiko/TaikoMode.h"

#include <Windows.h>

#include <algorithm>
#include <array>
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
    constexpr float JudgementX = 150.0F;
    constexpr float NoteCenterY = 330.0F;
    constexpr float TravelDistance = 920.0F;
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

    void AddPatternNote(
        finger_drum::chart::PatternDocument& pattern,
        const std::int64_t measure,
        const std::int64_t numerator,
        const std::int64_t denominator,
        const finger_drum::mode::TaikoNoteType type,
        const finger_drum::mode::TaikoPatternAction action)
    {
        PatternNote note;
        note.position = MusicalPosition{measure, Rational{numerator, denominator}};
        note.keyType = static_cast<int>(type);
        note.actionType = static_cast<int>(action);
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
            visualId == "Taiko.Balloon";
    }

    [[nodiscard]] mrg::visual2d::Color AmbientColor(
        const std::string_view visualId) noexcept
    {
        if (visualId == "Taiko.Kat" || visualId == "Taiko.BigKat")
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
    std::shared_ptr<finger_drum::GameplayLaunchRequest> launchRequest)
    : launchRequest_(std::move(launchRequest))
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
    ScheduleMusic();
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
    if (timer_.CurrentState() == finger_drum::rhythm::RhythmTimer::State::Running)
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

    // DestroyOnExit removes this complete mode instance after the deferred
    // transition. Only Lobby and its selected catalog data remain alive.
    if (IsPatternComplete())
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
    audioStatusLabel_ = nullptr;
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
        return CreateDemoSession();
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

    // Legacy lane art is stretched to the current scroll field. Its authored
    // texture remains unmodified; tinting is reserved for note Ambient layers.
    auto& track = mrg::visual2d::CreateSprite(
        root,
        {120.0F, 240.0F, 1040.0F, 180.0F},
        services.visual2DRendering.LoadImage(SkinAssetPath(L"LaneBackground.png")),
        "ScrollGear.Skin");
    track.SetZIndex(1);
    auto& laneLight = mrg::visual2d::CreateSprite(
        root,
        {JudgementX - 25.0F, 205.0F, 50.0F, 250.0F},
        services.visual2DRendering.LoadImage(SkinAssetPath(L"LaneLight.png")),
        "ScrollGear.Light");
    laneLight.SetZIndex(2);
    auto& judgementLine = mrg::visual2d::CreateSprite(
        root,
        {JudgementX - 57.0F, NoteCenterY - 57.0F, 114.0F, 114.0F},
        services.visual2DRendering.LoadImage(SkinAssetPath(L"JudgeLine.png")),
        "ScrollGear.JudgementLine");
    judgementLine.SetZIndex(3);

    timelineLabel_ = &mrg::visual2d::CreateLabel(
        root, {48.0F, 98.0F, 500.0F, 40.0F}, L"TIME", "Timeline");
    resultLabel_ = &mrg::visual2d::CreateLabel(
        root, {48.0F, 146.0F, 820.0F, 48.0F}, L"READY", "Result");
    audioStatusLabel_ = &mrg::visual2d::CreateLabel(
        root, {48.0F, 650.0F, 1184.0F, 34.0F},
        L"AUDIO: DSP-clock scheduled music and hitsounds", "AudioStatus");
    for (mrg::visual2d::Visual2DNode* label :
        {timelineLabel_, resultLabel_, audioStatusLabel_})
    {
        auto& text = RequireComponent<mrg::visual2d::TextVisualComponent>(*label);
        text.SetTextColor(DeepBlue);
        text.SetFontSize(label == resultLabel_ ? 24.0F : 17.0F);
    }

    auto& instructions = mrg::visual2d::CreateLabel(
        root,
        {120.0F, 478.0F, 1040.0F, 116.0F},
        L"D / K : KAT (rim)        F / J : DON (center)\n"
        L"One Lane focuses Don, Kat, large notes and rolls in exact time order.\n"
        L"SPACE : Pause / Resume     R : Restart     ESC : Song Select",
        "Instructions");
    auto& instructionText =
        RequireComponent<mrg::visual2d::TextVisualComponent>(instructions);
    instructionText.SetFontSize(18.0F);
    instructionText.SetTextColor(DeepBlue);
    instructionText.SetHorizontalAlignment(mrg::visual2d::TextAlignment::Center);
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

    auto& root = canvas_->AnchorNode(mrg::visual2d::Anchor::Center);
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
            layers.root = &root.CreateChild(std::format("RhythmNote.{}", note->Id()));
            layers.root->SetPivot({0.5F, 0.5F});
            layers.root->SetSize({diameter, diameter});
            layers.root->SetZIndex(5);
            layers.root->SetVisible(false);
            layers.diameter = diameter;

            // Body and tail are Ambient-colored and sit behind the circular
            // head. The white head overlay is a separate untinted draw packet.
            if (longNote)
            {
                layers.body = &mrg::visual2d::CreateSprite(
                    *layers.root,
                    {diameter * 0.5F, diameter * 0.25F, 1.0F, diameter * 0.5F},
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
    ScheduleMusic();
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
        const float x = JudgementX +
            std::clamp(note.normalizedTravel, -0.05F, 1.0F) * TravelDistance;
        layers.root->SetPosition({x - 640.0F, NoteCenterY - 360.0F});
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
            const float tailX = JudgementX + endTravel * TravelDistance;
            const float length = std::max(tailX - x, 1.0F);
            layers.body->SetBounds({
                layers.diameter * 0.5F,
                layers.diameter * 0.25F,
                length,
                layers.diameter * 0.5F});
            layers.tail->SetBounds({
                length,
                0.0F,
                layers.diameter,
                layers.diameter});
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
