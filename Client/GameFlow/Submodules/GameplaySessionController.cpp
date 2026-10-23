#include "GameFlow/GameplaySessionController.h"
#include <algorithm>
namespace
{
    bool IsKeyDownBeforeExit(const InputState &input, std::uint16_t key, bool beforeExit)
    {
        bool down = input.IsKeyDown(key);
        if (!beforeExit)
            return down;
        // The InputState snapshot includes the whole frame. Undo edges after
        // ESC so later releases cannot fail a Buzz tick at the exit time.
        const auto events = input.Events();
        for (auto event = events.rbegin(); event != events.rend(); ++event)
        {
            if (event->type == InputEventType::KeyPressed && event->code == VK_ESCAPE)
                break;
            if (event->code != key)
                continue;
            if (event->type == InputEventType::KeyPressed)
                down = false;
            else if (event->type == InputEventType::KeyReleased)
                down = true;
        }
        return down;
    }
} // namespace

void GameplaySessionController::InitializeSession()
{
    if (sessionInitialized_)
        return;
    PrepareDebugLaunchRequest();
    if (!session_)
        session_ = CreateSession();
    heldKeys_.reserve(session_->InputMapping().size());
    time_ = initialTime_;
    sessionInitialized_ = true;
}

bool GameplaySessionController::PrepareSession()
{
    if (sessionInitialized_)
        return true;
    if (session_ || !launchRequest_.IsValid())
    {
        InitializeSession();
        return true;
    }
    if (!sessionLoader_.Busy())
        sessionLoader_.Start(launchRequest_);
    auto loaded = sessionLoader_.Poll();
    if (!loaded)
        return false;
    if (!loaded->Succeeded())
        throw std::runtime_error("Failed to load the selected pattern: " +
            (loaded->diagnostics.empty() ? std::string{"Unknown chart error."} : loaded->diagnostics.front().message));
    session_ = std::move(loaded->session);
    InitializeSession();
    return true;
}

AudioPreparationState GameplaySessionController::PrepareAudioResources(AudioSystem &audio)
{
    if (!audioInitialized_)
    {
        InitializeAudio(audio);
        audioInitialized_ = true;
    }
    if (!initializationError_.empty())
        return AudioPreparationState::Failed;
    return audioRouter_.PrepareResources();
}
void GameplaySessionController::Start(const EngineServices &services,
                                      const RhythmTime initialTime)
{
    initialTime_ = initialTime;
    session_->SetResultInitialTime(initialTime);
    if (state_ == PlayState::Ending)
        return;
    static_cast<void>(PrepareAudioResources(services.audio));
    time_ = initialTime_;
    audioInitialized_ = true;
    resourcesReady_ = false;
    resourceLoadFailed_ = false;
    playbackRequested_ = state_ == PlayState::Loading;
    nextAudioPreparationPoll_ = 0;
}
void GameplaySessionController::Update(const UpdateContext &context)
{
    feedback_.clear();
    const auto previousState = state_;
    switch (state_)
    {
    case PlayState::Loading:
        UpdateLoading(context);
        break;
    case PlayState::Gameplay:
        UpdateGameplay(context);
        break;
    case PlayState::Pause:
        UpdatePause(context);
        break;
    case PlayState::Ending:
        break;
    }
    // Keep events timestamped after the scheduled start even when this is the
    // update that finishes Loading/Pause. Earlier preparation input is ignored.
    if (previousState != PlayState::Gameplay && state_ == PlayState::Gameplay)
        UpdateGameplay(context);
    audioRouter_.Update();
}
void GameplaySessionController::UpdateGameplay(const UpdateContext &context)
{
    const auto clock = context.audio.CaptureClockSnapshot();
    ProcessControlKeys(context, clock);
    if (state_ != PlayState::Gameplay)
        return;
    if (debugMode_)
        ProcessDebugTimeline(context.input, clock, context.deltaSeconds);
    // Freeze gameplay at ESC's timestamp, not at the later frame
    // clock. Inputs after ESC must not turn unfinished notes into hits/misses.
    std::optional<std::int64_t> exitTicks;
    for (const auto &event : context.input.Events())
        if (event.type == InputEventType::KeyPressed && event.code == VK_ESCAPE &&
            event.performanceCounterTicks >= gameplayStartTicks_)
        {
            exitTicks = event.performanceCounterTicks;
            break;
        }
    time_ = timer_.Now(exitTicks.value_or(clock.performanceCounterTicks));
    if (debugMode_ || timer_.CurrentState() == RhythmTimer::State::Running)
    {
        ProcessRhythmInput(context.input, exitTicks);
        UpdateSession(context.input, time_, exitTicks.has_value());
        audioRouter_.ApplyAutomation(session_->EvaluateAutomation(time_));
        if (!exitTicks) ScheduleMetronome();
    }
    if (exitTicks)
        PauseAt(*exitTicks, true);
    if (!debugMode_ && !pauseMenuOpen_ && timer_.CurrentState() == RhythmTimer::State::Running &&
        IsPatternComplete())
    {
        completedElapsedSeconds_ += context.deltaSeconds;
        if (completedElapsedSeconds_ >= 3.0)
            EnterEnding(PlayEndReason::Completed);
    }
}
std::string_view GameplaySessionController::ErrorMessage() const noexcept
{
    if (session_ && !session_->AutomationError().empty())
        return session_->AutomationError();
    return audioRouter_.LastError().empty() ? initializationError_ : audioRouter_.LastError();
}
void GameplaySessionController::Shutdown() noexcept
{
    timer_.Stop();
    state_ = PlayState::Ending;
    result_.reset();
    playbackStart_.reset();
    audioInitialized_ = resourcesReady_ = playbackRequested_ = false;
    nextAudioPreparationPoll_ = 0;
    audioRouter_.Shutdown();
    session_.reset();
    sessionInitialized_ = false;
    musicRegistered_ = false;
    feedback_.clear();
    pauseMenuOpen_ = false;
    completedElapsedSeconds_ = 0;
}
std::unique_ptr<IPlayResult> GameplaySessionController::TakeResult()
{
    return std::move(result_);
}

void GameplaySessionController::UpdateSession(const InputState &input,
                                              const RhythmTime time, bool beforeExit)
{
    heldKeys_.clear();
    // Capacity was reserved when the session was initialized.
    for (const auto &[key, action] : session_->InputMapping())
        if (IsKeyDownBeforeExit(input, key, beforeExit))
            heldKeys_.push_back(key);
    ConsumeResult(session_->Update(time, heldKeys_));
}

bool GameplaySessionController::IsPatternComplete() const noexcept
{
    if (session_ == nullptr || session_->Gear().Lanes().empty())
    {
        return false;
    }
    return std::ranges::all_of(session_->Gear().Lanes(), [](const std::unique_ptr<Lane> &lane)
                               { return lane != nullptr && lane->CurrentNote() == nullptr; });
}

void GameplaySessionController::ConsumeResult(NoteProcessResult result, bool pressed)
{
    audioRouter_.PlayNow(result.audioCues);
    feedback_.push_back({false, pressed, std::move(result)});
}
