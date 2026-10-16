#include "GameFlow/GameplaySessionController.h"
#include <algorithm>
#include <cmath>

void GameplaySessionController::ResetTimeline()
{
    feedback_.push_back({true});
    session_->Reset();
    audioRouter_.StopAllVoices();
    audioRouter_.ResetAutomation();
    timer_.Stop();
    time_ = initialTime_;
    state_ = PlayState::Loading;
    result_.reset();
    playbackStart_.reset();
    playbackRequested_ = true;
    pauseMenuOpen_ = false;
    completedElapsedSeconds_ = 0;
    nextAudioPreparationPoll_ = 0;
}

void GameplaySessionController::ProcessControlKeys(const mrg::UpdateContext &context,
                                                   const mrg::audio::AudioClockSnapshot &clock)
{
    const auto &input = context.input;
    const auto pressed = [&](std::uint16_t key)
    {
        return std::ranges::any_of(
            input.Events(),
            [&](const auto &event)
            {
                return event.type == mrg::platform::InputEventType::KeyPressed && event.code == key &&
                       (state_ != PlayState::Gameplay || event.performanceCounterTicks >= gameplayStartTicks_);
            });
    };
    if (pressed('R'))
    {
        ResetTimeline();
        return;
    }
    if (pressed(VK_SPACE))
    {
        using State = finger_drum::rhythm::RhythmTimer::State;
        if (timer_.CurrentState() == State::Running)
        {
            PauseAt(clock.performanceCounterTicks, false);
        }
        else if (timer_.CurrentState() == State::Paused)
        {
            if (debugMode_ && state_ == PlayState::Gameplay)
            {
                timer_.Resume(clock.performanceCounterTicks);
                timer_.AnchorDspClock(time_, clock.dspClock, clock.sampleRate);
                return;
            }
            static_cast<void>(ContinueAt(context, time_, finger_drum::mode::PlayControlType::Resume));
        }
    }
}

void GameplaySessionController::ProcessDebugTimeline(const mrg::platform::InputState &input,
                                                     const mrg::audio::AudioClockSnapshot &clock,
                                                     const double deltaSeconds)
{
    if constexpr (!ReferenceTimeDebug)
        return;
    if (!debugMode_)
        return;

    if (input.WasKeyPressed(VK_OEM_MINUS))
    {
        debugSpeedMillisecondsPerSecond_ = std::max(debugSpeedMillisecondsPerSecond_ - 200.0, 200.0);
    }
    if (input.WasKeyPressed(VK_OEM_PLUS))
    {
        debugSpeedMillisecondsPerSecond_ = std::min(debugSpeedMillisecondsPerSecond_ + 200.0, 4000.0);
    }

    std::int64_t deltaMicroseconds{};
    if (input.IsKeyDown(static_cast<std::uint16_t>('1')))
    {
        deltaMicroseconds -=
            static_cast<std::int64_t>(std::llround(debugSpeedMillisecondsPerSecond_ * deltaSeconds * 1000.0));
    }
    if (input.IsKeyDown(static_cast<std::uint16_t>('2')))
    {
        deltaMicroseconds +=
            static_cast<std::int64_t>(std::llround(debugSpeedMillisecondsPerSecond_ * deltaSeconds * 1000.0));
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
        audioRouter_.StopAllVoices();
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

void GameplaySessionController::ProcessRhythmInput(const mrg::platform::InputState &input,
                                                   std::optional<std::int64_t> exitTicks)
{
    for (const mrg::platform::InputEvent &event : input.Events())
    {
        if (event.performanceCounterTicks < gameplayStartTicks_)
            continue;
        if ((exitTicks && event.performanceCounterTicks > *exitTicks) ||
            (event.type == mrg::platform::InputEventType::KeyPressed && event.code == VK_ESCAPE))
            break;
        if (event.type != mrg::platform::InputEventType::KeyPressed &&
            event.type != mrg::platform::InputEventType::KeyReleased)
        {
            continue;
        }
        if (!session_->InputMapping().contains(event.code))
            continue;
        const auto edge = event.type == mrg::platform::InputEventType::KeyPressed
                              ? finger_drum::rhythm::InputEdge::Pressed
                              : finger_drum::rhythm::InputEdge::Released;
        const auto eventTime = timer_.Now(event.performanceCounterTicks);
        finger_drum::rhythm::NoteProcessResult result = session_->ProcessInput(event.code, edge, eventTime);
        ConsumeResult(std::move(result), edge == finger_drum::rhythm::InputEdge::Pressed);
    }
}
