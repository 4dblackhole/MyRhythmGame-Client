#include "GameFlow/GameplaySessionController.h"
#include <algorithm>

bool GameplaySessionController::PrepareResources(const mrg::UpdateContext &context)
{
    if (resourcesReady_)
        return true;
    if (!audioInitialized_ || context.totalSeconds < nextAudioPreparationPoll_)
        return false;
    nextAudioPreparationPoll_ = context.totalSeconds + 0.002;
    resourcesReady_ = audioRouter_.Ready();
    return resourcesReady_;
}

void GameplaySessionController::UpdateLoading(const mrg::UpdateContext &context)
{
    if (context.input.WasKeyPressed(VK_ESCAPE))
    {
        OpenPauseMenu(context);
        return;
    }
    if (PrepareResources(context))
        PrepareGameplay(context);
}

void GameplaySessionController::UpdatePause(const mrg::UpdateContext &context)
{
    if (!PrepareResources(context))
        return;
    if (playbackRequested_)
    {
        // ESC cancels the pending start at its frozen target. Record the newly
        // shown menu too, including a Space resume that was not yet practice.
        if (context.input.WasKeyPressed(VK_ESCAPE))
        {
            PauseAt(context.audio.CaptureClockSnapshot().performanceCounterTicks, true);
            return;
        }
        PrepareGameplay(context);
    }
    else if (!pauseMenuOpen_)
    {
        if (context.input.WasKeyPressed(VK_ESCAPE))
            OpenPauseMenu(context);
        else
            ProcessControlKeys(context, context.audio.CaptureClockSnapshot());
    }
}

void GameplaySessionController::PrepareGameplay(const mrg::UpdateContext &context)
{
    using Preparation = finger_drum::audio::GameplayAudioRouter::MusicPreparation;
    if (!playbackRequested_)
        return;
    if (playbackStart_)
    {
        const auto now = context.audio.CaptureClockSnapshot();
        if (now.performanceCounterTicks < playbackStart_->performanceCounterTicks)
            return;
        // QPC and DSP were scheduled for the same future instant. Frames that
        // arrive after it retain the actual elapsed time rather than restarting.
        timer_.Resume(playbackStart_->performanceCounterTicks);
        gameplayStartTicks_ = playbackStart_->performanceCounterTicks;
        playbackStart_.reset();
        playbackRequested_ = false;
        pauseMenuOpen_ = false;
        state_ = PlayState::Gameplay;
        return;
    }
    if (context.totalSeconds < nextAudioPreparationPoll_)
        return;
    nextAudioPreparationPoll_ = context.totalSeconds + 0.002;
    if (!debugMode_ && musicRegistered_)
    {
        const auto prepared = audioRouter_.PrepareMusicAt(time_);
        if (prepared == Preparation::Loading)
            return;
        if (prepared == Preparation::Failed)
        {
            FailPlaybackPreparation();
            return;
        }
    }
    ScheduleGameplay(context);
}

void GameplaySessionController::FailPlaybackPreparation()
{
    playbackRequested_ = false;
    playbackStart_.reset();
    audioRouter_.StopAllVoices();
    state_ = PlayState::Pause;
    pauseMenuOpen_ = true;
    session_->RecordControl({finger_drum::mode::PlayControlType::Pause, time_, time_, 0, true});
}

void GameplaySessionController::ScheduleGameplay(const mrg::UpdateContext &context)
{
    audioRouter_.ApplyAutomation(session_->EvaluateAutomation(time_));
    auto start = context.audio.CaptureClockSnapshot();
    // Reserve the same 20 ms on both clocks; a DSP-only lead makes music late.
    if (!debugMode_)
    {
        start.performanceCounterTicks += start.performanceCounterFrequency / 50;
        start.dspClock += static_cast<std::uint64_t>(std::max(start.sampleRate, 0)) / 50;
    }
    timer_.Start(start.performanceCounterTicks, start.performanceCounterFrequency, time_);
    timer_.Pause(start.performanceCounterTicks);
    timer_.AnchorDspClock(time_, start.dspClock, start.sampleRate);
    if (!debugMode_ && musicRegistered_ && !audioRouter_.StartPreparedMusic(time_, timer_))
    {
        FailPlaybackPreparation();
        return;
    }
    if (debugMode_)
    {
        if (!ReferenceTimeDebug)
            timer_.Resume(start.performanceCounterTicks);
        gameplayStartTicks_ = start.performanceCounterTicks;
        playbackRequested_ = false;
        pauseMenuOpen_ = false;
        state_ = PlayState::Gameplay;
    }
    else
        playbackStart_ = start;
}

void GameplaySessionController::EnterEnding(finger_drum::mode::PlayEndReason reason)
{
    if (state_ == PlayState::Ending)
        return;
    state_ = PlayState::Ending;
    playbackRequested_ = false;
    playbackStart_.reset();
    pauseMenuOpen_ = false;
    audioRouter_.StopAllVoices();
    // Preserve the displayed end time, while stopping the judgement clock.
    timer_.Stop();
    result_ = session_->TakeResult(reason, time_);
}
