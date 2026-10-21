#include "PlaySession.h"
#include <stdexcept>

void PlaySession::RecordControl(PlayControlEvent event)
{
    pauseMenuUsed_ |= event.pauseMenuShown;
    if (resultCollector_)
        resultCollector_->RecordControl(event);
}
void PlaySession::RewindJudgements(RhythmTime target)
{
    if (target == RhythmTime::min())
        throw std::invalid_argument("The rewind target cannot be the minimum clock value.");
    // Keep strictly earlier operations, in their original execution order.
    // Resume may have preserved state while moving the clock backwards, so
    // history is deliberately not assumed to be sorted by chart time.
    std::erase_if(judgementHistory_, [target](const auto &step) { return step.time >= target; });
    ResetNoteState();
    if (resultCollector_)
        resultCollector_->ResetJudgements();
    lastRecordedHeldKeys_.clear();
    for (const auto &step : judgementHistory_)
    {
        NoteProcessResult result;
        if (step.input)
        {
            result = gear_.Lanes().front()->ProcessInput(*step.input, heldActions_);
            ApplyPhysicalInput(step.input->physicalKey, step.input->edge);
            RefreshHeldActions();
        }
        else
        {
            ResolveHeldActions(step.heldKeys);
            result = gear_.Update(step.time, heldActions_);
        }
        AccumulateAccuracy(result, step.input && step.input->edge == InputEdge::Pressed);
    }
    // Catch up to the open left boundary without grading target-time notes.
    const auto before = target - RhythmDuration{1};
    lastRecordedHeldKeys_ = heldPhysicalKeys_;
    auto result = gear_.Update(before, heldActions_);
    if (!result.events.empty() || !result.finalizedAccuracies.empty())
        judgementHistory_.push_back({before, {}, heldPhysicalKeys_});
    AccumulateAccuracy(result);
}
void PlaySession::RetireRemaining(RhythmTime time)
{
    for (auto &lane : gear_.Lanes())
        for (const auto &note : lane->Notes())
        {
            if (note->Accuracy().finalized)
                continue;
            const auto result = note->MarkMissed(time);
            for (const auto &accuracy : result.finalizedAccuracies)
            {
                if (resultCollector_)
                    resultCollector_->RecordRetirement(accuracy);
                ++finalizedNoteCount_; // Zero contribution, including partially played long notes.
            }
        }
    static_cast<void>(gear_.Update(time)); // Advance each lane past its terminal notes.
    lastNoteAccuracy_.reset();
}
