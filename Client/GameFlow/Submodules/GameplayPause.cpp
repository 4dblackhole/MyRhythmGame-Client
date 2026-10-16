#include "GameFlow/GameplaySessionController.h"
#include <algorithm>
#include <stdexcept>
using namespace finger_drum;

void GameplaySessionController::OpenPauseMenu(const mrg::UpdateContext &context)
{
    if (pauseMenuOpen_ || state_ == PlayState::Ending)
        return;
    PauseAt(context.audio.CaptureClockSnapshot().performanceCounterTicks, true);
}
void GameplaySessionController::PauseAt(std::int64_t ticks, bool menuShown)
{
    if (state_ == PlayState::Gameplay)
    {
        time_ = timer_.Now(ticks);
        timer_.Pause(ticks);
    }
    state_ = PlayState::Pause;
    playbackRequested_ = false;
    playbackStart_.reset();
    // Transient samples are discarded, never resumed as background music.
    audioRouter_.StopAllVoices();
    pauseMenuOpen_ = menuShown;
    session_->RecordControl({mode::PlayControlType::Pause, time_, time_, 0, menuShown});
}
bool GameplaySessionController::ResumeFromPause(const mrg::UpdateContext &context, rhythm::RhythmDuration leadIn)
{
    if (leadIn < rhythm::RhythmDuration{500'000} || leadIn > rhythm::RhythmDuration{3'000'000})
        throw std::invalid_argument("Resume lead-in must be between 0.5 and 3 seconds.");
    if (!pauseMenuOpen_ || !CanContinue())
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
    time_ = target;
    session_->RecordControl({action, from, target});
    audioRouter_.ApplyAutomation(session_->EvaluateAutomation(target));
    feedback_.push_back({true}); // Invalidate visuals only; Resume keeps domain state.
    completedElapsedSeconds_ = 0;
    playbackRequested_ = true;
    playbackStart_.reset();
    nextAudioPreparationPoll_ = 0;
    pauseMenuOpen_ = false;
    PrepareGameplay(context);
    return true; // Accepted once; seek completion is polled by the Pause state.
}
bool GameplaySessionController::RewindFromPause(const mrg::UpdateContext &context, std::size_t measures)
{
    if (!pauseMenuOpen_ || !CanContinue())
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
void GameplaySessionController::RetryFromPause(const mrg::UpdateContext &)
{
    if (state_ == PlayState::Pause)
        ResetTimeline();
}
void GameplaySessionController::RetireFromPause()
{
    if (state_ != PlayState::Pause)
        return;
    session_->RecordControl({mode::PlayControlType::Retire, time_, time_});
    session_->RetireRemaining(time_);
    EnterEnding(mode::PlayEndReason::Interrupted);
}
