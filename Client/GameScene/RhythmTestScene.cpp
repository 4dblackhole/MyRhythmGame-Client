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
    constexpr mrg::visual2d::Color DonRed{0.95F, 0.29F, 0.32F, 1.0F};
    constexpr mrg::visual2d::Color KatBlue{0.23F, 0.63F, 0.92F, 1.0F};
    constexpr mrg::visual2d::Color TrackBlue{0.82F, 0.92F, 0.98F, 0.96F};
    constexpr float JudgementX = 150.0F;
    constexpr float TravelDistance = 920.0F;

    template <typename ComponentType>
    [[nodiscard]] ComponentType& RequireComponent(
        mrg::visual2d::Visual2DNode& node)
    {
        ComponentType* const component = node.GetComponent<ComponentType>();
        if (component == nullptr)
        {
            throw std::logic_error("A rhythm-test node is missing a component.");
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

    [[nodiscard]] std::filesystem::path PopSoundPath()
    {
        return mrg::platform::ResolveExecutableRelativePath(
            L"assets\\sounds\\pop.wav");
    }
}

void RhythmTestScene::Initialize(const mrg::EngineServices& services)
{
    width_ = services.windowWidth;
    height_ = services.windowHeight;
    session_ = CreateDemoSession();
    canvas_ = std::make_unique<mrg::visual2d::Visual2DCanvas>(
        mrg::visual2d::Size{1280.0F, 720.0F},
        mrg::visual2d::CanvasScaleMode::FixedHeight);
    canvas_->SetViewportSize({
        static_cast<float>(width_),
        static_cast<float>(height_)});
    CreatePresentation();
    CreateNoteVisuals();

    // 예제는 하나의 wav를 여러 의미 ID에 연결한다. 실제 Client에서는
    // 각 ID를 서로 다른 파일로 등록해도 판정/노트 코드는 바뀌지 않는다.
    std::string audioError;
    if (audioRouter_.Initialize(services.audio, audioError))
    {
        const std::array<finger_drum::rhythm::SoundId, 7> soundIds{
            "Taiko.Don.Hit",
            "Taiko.Kat.Hit",
            "Taiko.BigDon.FirstHit",
            "Taiko.BigKat.FirstHit",
            "Taiko.LongNote.Tick",
            "Taiko.Don.FreeInput",
            "Taiko.Kat.FreeInput"};
        if (!audioRouter_.RegisterSoundAliases(
                soundIds,
                PopSoundPath(),
                audioError))
        {
            // The test remains usable without sound; the on-screen status
            // reports the backend or asset error instead of aborting startup.
        }
    }
    if (!audioError.empty() && audioStatusLabel_ != nullptr)
    {
        RequireComponent<mrg::visual2d::TextVisualComponent>(*audioStatusLabel_).
            SetText(L"AUDIO: " + std::wstring(audioError.begin(), audioError.end()));
    }
    StartTimeline(services.audio.CaptureClockSnapshot());
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

    // Completion does not retain the gameplay object as a hidden Scene.
    // After a short result-viewing delay the deferred transition ends this
    // update, calls Shutdown, and destroys the transient Scene instance.
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
    AddPatternNote(pattern, 1, 0, 1, NoteType::Don, Action::Down);
    AddPatternNote(pattern, 1, 1, 4, NoteType::Kat, Action::Down);
    AddPatternNote(pattern, 1, 2, 4, NoteType::Don, Action::Down);
    AddPatternNote(pattern, 1, 3, 4, NoteType::Kat, Action::Down);
    AddPatternNote(pattern, 2, 0, 1, NoteType::TickRoll, Action::LongNoteStart);
    AddPatternNote(pattern, 3, 0, 1, NoteType::TickRoll, Action::LongNoteEnd);

    finger_drum::chart::EffectDocument effects;
    effects.commands.push_back({
        .position = {0, Rational{0, 1}},
        .type = finger_drum::chart::EffectCommandType::BusVolume,
        .target = "HitSound",
        .beginValue = 0.65,
        .endValue = 1.0,
        .durationMilliseconds = 2000.0,
        .curve = finger_drum::chart::AutomationCurve::Linear});
    effects.commands.push_back({
        .position = {2, Rational{0, 1}},
        .type = finger_drum::chart::EffectCommandType::ReverbSend,
        .target = "TickSound",
        .beginValue = 0.0,
        .endValue = 0.35,
        .durationMilliseconds = 1800.0,
        .curve = finger_drum::chart::AutomationCurve::Smoothstep});

    finger_drum::mode::TaikoMode mode;
    finger_drum::mode::ModeLoadResult result =
        mode.CreateSession(pattern, effects);
    if (!result.Succeeded())
    {
        throw std::runtime_error("Failed to create the Taiko test session.");
    }
    return std::move(result.session);
}

void RhythmTestScene::CreatePresentation()
{
    auto& root = canvas_->CreateNode(
        mrg::visual2d::Anchor::Center, "RhythmTest.Root");
    root.SetPivot({0.5F, 0.5F});
    root.SetSize({1280.0F, 720.0F});

    auto& background = mrg::visual2d::CreatePanel(
        root, {0.0F, 0.0F, 1280.0F, 720.0F}, "Background");
    SetColor(background, {0.918F, 0.965F, 1.0F, 1.0F});
    auto& header = mrg::visual2d::CreateLabel(
        root, {48.0F, 34.0F, 1184.0F, 52.0F},
        L"FINGERDRUM · RHYTHM ARCHITECTURE DRIVER", "Header");
    auto& headerText = RequireComponent<mrg::visual2d::TextVisualComponent>(header);
    headerText.SetFontSize(24.0F);
    headerText.SetTextColor(DeepBlue);

    auto& track = mrg::visual2d::CreatePanel(
        root, {120.0F, 240.0F, 1040.0F, 180.0F}, "ScrollGear");
    SetColor(track, TrackBlue);
    auto& judgementLine = mrg::visual2d::CreatePanel(
        root, {JudgementX, 226.0F, 8.0F, 208.0F}, "JudgementLine");
    SetColor(judgementLine, {1.0F, 0.80F, 0.30F, 1.0F});

    timelineLabel_ = &mrg::visual2d::CreateLabel(
        root, {48.0F, 98.0F, 500.0F, 40.0F}, L"TIME", "Timeline");
    resultLabel_ = &mrg::visual2d::CreateLabel(
        root, {48.0F, 146.0F, 820.0F, 48.0F}, L"READY", "Result");
    audioStatusLabel_ = &mrg::visual2d::CreateLabel(
        root, {48.0F, 650.0F, 1184.0F, 34.0F},
        L"AUDIO: DSP-clock scheduled hitsounds", "AudioStatus");
    for (mrg::visual2d::Visual2DNode* label :
        {timelineLabel_, resultLabel_, audioStatusLabel_})
    {
        auto& text = RequireComponent<mrg::visual2d::TextVisualComponent>(*label);
        text.SetTextColor(DeepBlue);
        text.SetFontSize(label == resultLabel_ ? 24.0F : 17.0F);
    }

    auto& instructions = mrg::visual2d::CreateLabel(
        root, {120.0F, 478.0F, 1040.0F, 116.0F},
        L"D / K : KAT (rim)        F / J : DON (center)\n"
        L"Big notes require two Good-or-better hits. Hold F/J for the tick roll.\n"
        L"SPACE : Pause / Resume     R : Restart     ESC : Song Select",
        "Instructions");
    auto& instructionText =
        RequireComponent<mrg::visual2d::TextVisualComponent>(instructions);
    instructionText.SetFontSize(18.0F);
    instructionText.SetTextColor(DeepBlue);
    instructionText.SetHorizontalAlignment(mrg::visual2d::TextAlignment::Center);
}

void RhythmTestScene::CreateNoteVisuals()
{
    auto& root = canvas_->AnchorNode(mrg::visual2d::Anchor::Center);
    for (const std::unique_ptr<finger_drum::rhythm::Lane>& lane :
        session_->Gear().Lanes())
    {
        for (const std::unique_ptr<finger_drum::rhythm::INote>& note :
            lane->Notes())
        {
            auto& visual = mrg::visual2d::CreatePanel(
                root, {0.0F, 0.0F, 38.0F, 38.0F}, "RhythmNote");
            visual.SetPivot({0.5F, 0.5F});
            visual.SetZIndex(5);
            SetColor(visual, note->Id() % 2 == 0 ? KatBlue : DonRed);
            noteVisuals_.emplace(note->Id(), &visual);
        }
    }
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
    acceptedHitCount_ = 0;
    accumulatedScore_ = 0.0;
    completedElapsedSeconds_ = 0.0;
    StartTimeline(clock);
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
        }
        else if (timer_.CurrentState() == State::Paused)
        {
            timer_.Resume(clock.performanceCounterTicks);
            // QPC paused with the chart while the mixer clock continued.
            // Re-anchoring keeps resumed hitsounds on the same absolute DSP
            // timeline instead of delaying them by the pause duration.
            timer_.AnchorDspClock(
                timer_.Now(clock.performanceCounterTicks),
                clock.dspClock,
                clock.sampleRate);
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
            L"TIME {:+.3f} s   ·   LEVEL 50",
            static_cast<double>(time.count()) / 1'000'000.0));

    const auto snapshot = session_->Gear().BuildSnapshot(
        time,
        finger_drum::rhythm::RhythmDuration{2'000'000},
        finger_drum::rhythm::RhythmDuration{220'000});
    for (auto& [id, visual] : noteVisuals_)
    {
        visual->SetVisible(false);
    }
    for (const finger_drum::rhythm::ScrollNoteSnapshot& note : snapshot.notes)
    {
        const auto found = noteVisuals_.find(note.noteId);
        if (found == noteVisuals_.end())
        {
            continue;
        }
        mrg::visual2d::Visual2DNode& visual = *found->second;
        const float x = JudgementX +
            std::clamp(note.normalizedTravel, -0.05F, 1.0F) * TravelDistance;
        visual.SetPosition({x - 640.0F, 330.0F - 360.0F});
        visual.SetVisible(
            note.state != NoteState::Completed && note.state != NoteState::Missed);
        if (note.state == NoteState::AwaitingAdditionalInput ||
            note.state == NoteState::Holding)
        {
            visual.SetSize({54.0F, 54.0F});
        }
        else
        {
            visual.SetSize({38.0F, 38.0F});
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
