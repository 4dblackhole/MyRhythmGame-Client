#include "GameplaySessionController.h"
#include "GameplaySupport.h"
using namespace gameplay;
namespace
{
    bool IsKeyDownBeforeExit(const mrg::platform::InputState &input, std::uint16_t key, bool beforeExit)
    {
        bool down = input.IsKeyDown(key);
        if (!beforeExit) return down;
        // The InputState snapshot includes the whole frame. Undo edges after
        // ESC so later releases cannot fail a Buzz tick at the exit time.
        const auto events = input.Events();
        for (auto event = events.rbegin(); event != events.rend(); ++event)
        {
            if (event->type == mrg::platform::InputEventType::KeyPressed && event->code == VK_ESCAPE) break;
            if (event->code != key) continue;
            if (event->type == mrg::platform::InputEventType::KeyPressed) down = false;
            else if (event->type == mrg::platform::InputEventType::KeyReleased) down = true;
        }
        return down;
    }
}

void GameplaySessionController::InitializeSession()
{
    PrepareDebugLaunchRequest();
    if (!session_)
        session_ = CreateSession();
}
void GameplaySessionController::Start(const mrg::EngineServices &services,
                                      const finger_drum::rhythm::RhythmTime initialTime)
{
    initialTime_ = initialTime;
    session_->SetResultInitialTime(initialTime);
    InitializeAudio(services);
    StartTimeline(services.audio.CaptureClockSnapshot());
    if (!debugMode_)
        ScheduleMusic();
}
void GameplaySessionController::Update(const mrg::UpdateContext &context)
{
    feedback_.clear();
    const auto clock = context.audio.CaptureClockSnapshot();
    ProcessControlKeys(context.input, clock);
    if (debugMode_)
        ProcessDebugTimeline(context.input, clock, context.deltaSeconds);
    // Freeze an interrupted result at ESC's timestamp, not at the later frame
    // clock. Inputs after ESC must not turn unfinished notes into hits/misses.
    std::optional<std::int64_t> exitTicks;
    for (const auto &event : context.input.Events())
        if (event.type == mrg::platform::InputEventType::KeyPressed && event.code == VK_ESCAPE)
        {
            exitTicks = event.performanceCounterTicks;
            break;
        }
    time_ = timer_.Now(exitTicks.value_or(clock.performanceCounterTicks));
    if (debugMode_ || timer_.CurrentState() == finger_drum::rhythm::RhythmTimer::State::Running)
    {
        ProcessRhythmInput(context.input, exitTicks);
        UpdateSession(context.input, time_, exitTicks.has_value());
        audioRouter_.ApplyAutomation(session_->EvaluateAutomation(time_));
    }
    audioRouter_.Update();
    if (!debugMode_ && IsPatternComplete())
        completedElapsedSeconds_ += context.deltaSeconds;
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
    audioRouter_.Shutdown();
    session_.reset();
    musicRegistered_ = false;
    feedback_.clear();
}
std::unique_ptr<finger_drum::mode::IPlayResult> GameplaySessionController::TakeResult()
{
    return session_->TakeResult(IsPatternComplete() ? finger_drum::mode::PlayEndReason::Completed
                                                   : finger_drum::mode::PlayEndReason::Interrupted, time_);
}

void GameplaySessionController::UpdateSession(const mrg::platform::InputState &input,
                                              const finger_drum::rhythm::RhythmTime time, bool beforeExit)
{
    std::array<finger_drum::rhythm::PhysicalKey, finger_drum::mode::TaikoInputBindings.size() * 3>
        heldStorage{};
    std::size_t heldCount{};
    for (const finger_drum::mode::TaikoInputBinding &binding :
         finger_drum::mode::TaikoInputBindings)
    {
        if (IsKeyDownBeforeExit(input, binding.primaryKey, beforeExit))
        {
            heldStorage[heldCount++] = binding.primaryKey;
        }
        for (const finger_drum::rhythm::PhysicalKey secondaryKey : binding.secondaryKeys)
        {
            if (IsKeyDownBeforeExit(input, secondaryKey, beforeExit))
            {
                heldStorage[heldCount++] = secondaryKey;
            }
        }
    }
    ConsumeResult(session_->Update(
        time, std::span<const finger_drum::rhythm::PhysicalKey>{heldStorage.data(), heldCount}));
}

bool GameplaySessionController::IsPatternComplete() const noexcept
{
    if (session_ == nullptr || session_->Gear().Lanes().empty())
    {
        return false;
    }
    return std::ranges::all_of(session_->Gear().Lanes(),
                               [](const std::unique_ptr<finger_drum::rhythm::Lane> &lane) {
                                   return lane != nullptr && lane->CurrentNote() == nullptr;
                               });
}

void GameplaySessionController::ConsumeResult(finger_drum::rhythm::NoteProcessResult result,
                                              bool pressed)
{
    audioRouter_.PlayNow(result.audioCues);
    feedback_.push_back({false, pressed, std::move(result)});
}
