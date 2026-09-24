#include "GameplaySessionController.h"
#include "GameplaySupport.h"
using namespace gameplay;

void GameplaySessionController::InitializeSession()
{
    PrepareDebugLaunchRequest();
    session_ = CreateSession();
}
void GameplaySessionController::Start(const mrg::EngineServices &services,
                                      const finger_drum::rhythm::RhythmTime initialTime)
{
    initialTime_ = initialTime;
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
    time_ = timer_.Now(clock.performanceCounterTicks);
    if (debugMode_ || timer_.CurrentState() == finger_drum::rhythm::RhythmTimer::State::Running)
    {
        ProcessRhythmInput(context.input);
        UpdateSession(context.input, time_);
        audioRouter_.ApplyAutomation(session_->EvaluateAutomation(time_));
    }
    audioRouter_.Update();
    if (!debugMode_ && IsPatternComplete())
        completedElapsedSeconds_ += context.deltaSeconds;
}
std::string_view GameplaySessionController::AudioError() const noexcept
{
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

void GameplaySessionController::UpdateSession(const mrg::platform::InputState &input,
                                              const finger_drum::rhythm::RhythmTime time)
{
    std::array<finger_drum::rhythm::PhysicalKey, finger_drum::mode::TaikoInputBindings.size() * 3>
        heldStorage{};
    std::size_t heldCount{};
    for (const finger_drum::mode::TaikoInputBinding &binding :
         finger_drum::mode::TaikoInputBindings)
    {
        if (input.IsKeyDown(binding.primaryKey))
        {
            heldStorage[heldCount++] = binding.primaryKey;
        }
        for (const finger_drum::rhythm::PhysicalKey secondaryKey : binding.secondaryKeys)
        {
            if (input.IsKeyDown(secondaryKey))
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
