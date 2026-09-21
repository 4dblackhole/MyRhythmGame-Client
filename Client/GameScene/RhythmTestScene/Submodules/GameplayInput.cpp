#include "GameplaySessionController.h"
#include "GameplaySupport.h"
using namespace gameplay;

void GameplaySessionController::StartTimeline(const mrg::audio::AudioClockSnapshot &clock)
{
    constexpr finger_drum::rhythm::RhythmTime LeadIn{-2'000'000};
    timer_.Start(clock.performanceCounterTicks, clock.performanceCounterFrequency, LeadIn);
    timer_.AnchorDspClock(LeadIn, clock.dspClock, clock.sampleRate);
    if (debugMode_ && ReferenceTimeDebug)
    {
        timer_.Pause(clock.performanceCounterTicks);
    }
}

void GameplaySessionController::ResetTimeline(const mrg::audio::AudioClockSnapshot &clock)
{
    feedback_.push_back({true});
    session_->Reset();
    audioRouter_.StopAllVoices();
    completedElapsedSeconds_ = 0.0;
    StartTimeline(clock);
    if (!debugMode_)
    {
        ScheduleMusic();
    }
}

void GameplaySessionController::ProcessControlKeys(const mrg::platform::InputState &input,
                                                   const mrg::audio::AudioClockSnapshot &clock)
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
            timer_.AnchorDspClock(timer_.Now(clock.performanceCounterTicks), clock.dspClock,
                                  clock.sampleRate);
            std::string ignoredError;
            static_cast<void>(audioRouter_.SetVoicesPaused(false, ignoredError));
        }
    }
}

void GameplaySessionController::ProcessDebugTimeline(const mrg::platform::InputState &input,
                                                     const mrg::audio::AudioClockSnapshot &clock,
                                                     const double deltaSeconds)
{
    if (!debugMode_ || !ReferenceTimeDebug)
    {
        return;
    }

    if (input.WasKeyPressed(VK_OEM_MINUS))
    {
        debugSpeedMillisecondsPerSecond_ =
            std::max(debugSpeedMillisecondsPerSecond_ - 200.0, 200.0);
    }
    if (input.WasKeyPressed(VK_OEM_PLUS))
    {
        debugSpeedMillisecondsPerSecond_ =
            std::min(debugSpeedMillisecondsPerSecond_ + 200.0, 4000.0);
    }

    std::int64_t deltaMicroseconds{};
    if (input.IsKeyDown(static_cast<std::uint16_t>('1')))
    {
        deltaMicroseconds -= static_cast<std::int64_t>(
            std::llround(debugSpeedMillisecondsPerSecond_ * deltaSeconds * 1000.0));
    }
    if (input.IsKeyDown(static_cast<std::uint16_t>('2')))
    {
        deltaMicroseconds += static_cast<std::int64_t>(
            std::llround(debugSpeedMillisecondsPerSecond_ * deltaSeconds * 1000.0));
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
    const auto after = before + finger_drum::rhythm::RhythmDuration{deltaMicroseconds};
    if (after < before)
    {
        feedback_.push_back({true});
        session_->Reset();
        audioRouter_.StopAllVoices();
    }
    timer_.Seek(after, clock.performanceCounterTicks);
    timer_.AnchorDspClock(after, clock.dspClock, clock.sampleRate);
}

void GameplaySessionController::ProcessRhythmInput(const mrg::platform::InputState &input)
{
    for (const mrg::platform::InputEvent &event : input.Events())
    {
        if (event.type != mrg::platform::InputEventType::KeyPressed &&
            event.type != mrg::platform::InputEventType::KeyReleased)
        {
            continue;
        }
        const auto inputBinding =
            std::ranges::find_if(finger_drum::mode::TaikoInputBindings,
                                 [&event](const finger_drum::mode::TaikoInputBinding &binding) {
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
        ConsumeResult(std::move(result), edge == finger_drum::rhythm::InputEdge::Pressed);
    }
}
