#include "GameplaySessionController.h"
#include <algorithm>
#include <stdexcept>
using namespace finger_drum;

void GameplaySessionController::OpenPauseMenu(const mrg::UpdateContext &context)
{
    if (pauseMenuOpen_)
        return;
    PauseAt(context.audio.CaptureClockSnapshot().performanceCounterTicks, true);
}
void GameplaySessionController::PauseAt(std::int64_t ticks, bool menuShown)
{
    if (timelineStarted_)
    {
        time_ = timer_.Now(ticks);
        if (timer_.CurrentState() == rhythm::RhythmTimer::State::Running)
            timer_.Pause(ticks);
    }
    std::string error;
    static_cast<void>(audioRouter_.SetVoicesPaused(true, error));
    pauseMenuOpen_ = menuShown;
    session_->RecordControl({mode::PlayControlType::Pause, time_, time_, 0, menuShown});
}
bool GameplaySessionController::ResumeFromPause(const mrg::UpdateContext &context, rhythm::RhythmDuration leadIn)
{
    if (leadIn < rhythm::RhythmDuration{500'000} || leadIn > rhythm::RhythmDuration{3'000'000})
        throw std::invalid_argument("Resume lead-in must be between 0.5 and 3 seconds.");
    if (!pauseMenuOpen_ || !timelineStarted_)
        return false;
    // Preserve note rules, judgement history and score. Only the transport moves.
    return ContinueAt(context, time_ - leadIn, mode::PlayControlType::Resume);
}
bool GameplaySessionController::ContinueAt(const mrg::UpdateContext &context, rhythm::RhythmTime target,
                                           mode::PlayControlType action)
{
    const auto from = time_;
    audioRouter_.StopAllVoices();
    audioRouter_.ResetAutomation();
    const auto clock = context.audio.CaptureClockSnapshot();
    timer_.Seek(target, clock.performanceCounterTicks);
    timer_.AnchorDspClock(target, clock.dspClock, clock.sampleRate);
    time_ = target;
    session_->RecordControl({action, from, target});
    audioRouter_.ApplyAutomation(session_->EvaluateAutomation(target));
    feedback_.push_back({true}); // Invalidate visuals only; Resume keeps domain state.
    completedElapsedSeconds_ = 0;
    if (!debugMode_ && musicRegistered_ && !audioRouter_.StartMusicAt(target, timer_))
        return false;
    timer_.Resume(clock.performanceCounterTicks);
    pauseMenuOpen_ = false;
    return true;
}
bool GameplaySessionController::RewindFromPause(const mrg::UpdateContext &context, std::size_t measures)
{
    if (!pauseMenuOpen_ || !timelineStarted_)
        return false;
    if (measures == 0)
        throw std::invalid_argument("Rewind requires at least one measure.");
    const auto &spans = session_->Timeline().MeasureSpans();
    rhythm::RhythmTime target = std::min(initialTime_, time_);
    if (!spans.empty() && time_ >= spans.front().begin)
    {
        const auto next = std::ranges::upper_bound(spans, time_, {}, &chart::MusicalTimeline::MeasureTimeSpan::begin);
        const auto index = static_cast<std::size_t>(std::distance(spans.begin(), next) - 1);
        target = spans[index > measures ? index - measures : 0].begin;
    }
    // Use compiled measure starts, not a fixed duration or the current BPM.
    session_->RewindJudgements(target);
    return ContinueAt(context, target, mode::PlayControlType::Rewind);
}
void GameplaySessionController::RetryFromPause(const mrg::UpdateContext &context)
{
    pauseMenuOpen_ = false;
    retired_ = false;
    ResetTimeline(context.audio.CaptureClockSnapshot());
    if (!timelineStarted_)
        time_ = initialTime_;
}
void GameplaySessionController::RetireFromPause()
{
    session_->RecordControl({mode::PlayControlType::Retire, time_, time_});
    session_->RetireRemaining(time_);
    audioRouter_.StopAllVoices();
    retired_ = true;
    pauseMenuOpen_ = false;
}
