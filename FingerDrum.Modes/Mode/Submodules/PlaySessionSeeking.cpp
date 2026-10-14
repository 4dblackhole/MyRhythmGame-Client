#include "PlaySession.h"
#include <set>
#include <stdexcept>

namespace finger_drum::mode
{
    void PlaySession::RecordControl(PlayControlEvent event)
    {
        pauseMenuUsed_ |= event.pauseMenuShown;
        if (resultCollector_)
            resultCollector_->RecordControl(event);
    }
    void PlaySession::RewindJudgements(rhythm::RhythmTime target)
    {
        if (target == rhythm::RhythmTime::min())
            throw std::invalid_argument("The rewind target cannot be the minimum clock value.");
        // Keep strictly earlier operations, in their original execution order.
        // Resume may have preserved state while moving the clock backwards, so
        // history is deliberately not assumed to be sorted by chart time.
        std::erase_if(judgementHistory_, [target](const auto &step) { return step.time >= target; });
        ResetNoteState();
        if (resultCollector_)
            resultCollector_->ResetJudgements();
        lastRecordedHeld_.clear();
        std::set<rhythm::PhysicalKey> held;
        for (const auto &step : judgementHistory_)
        {
            rhythm::NoteProcessResult result;
            if (step.input)
            {
                result = gear_.Lanes().front()->ProcessInput(*step.input);
                if (step.input->edge == rhythm::InputEdge::Pressed)
                    held.insert(step.input->physicalKey);
                else
                    held.erase(step.input->physicalKey);
            }
            else
            {
                ResolveHeldActions(step.heldKeys);
                result = gear_.Update(step.time, heldActions_);
                held = std::set<rhythm::PhysicalKey>(step.heldKeys.begin(), step.heldKeys.end());
            }
            AccumulateAccuracy(result, step.input && step.input->edge == rhythm::InputEdge::Pressed);
        }
        // Catch up to the open left boundary without grading target-time notes.
        const auto before = target - rhythm::RhythmDuration{1};
        const std::vector<rhythm::PhysicalKey> keys{held.begin(), held.end()};
        ResolveHeldActions(keys);
        lastRecordedHeld_ = heldActions_;
        auto result = gear_.Update(before, heldActions_);
        if (!result.events.empty() || !result.finalizedAccuracies.empty())
            judgementHistory_.push_back({before, {}, keys});
        AccumulateAccuracy(result);
    }
    void PlaySession::RetireRemaining(rhythm::RhythmTime time)
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
} // namespace finger_drum::mode
