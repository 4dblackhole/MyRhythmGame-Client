#include "Note/Submodules/TimedSequenceInputRule.h"
#include "Note/Submodules/RuleHelpers.h"
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace finger_drum::rhythm
{
    using namespace detail;

    TimedSequenceInputRule::TimedSequenceInputRule(std::vector<NoteAction> repeatingSequence,
                                                   const std::size_t requiredHitCount, const RhythmTime endTime,
                                                   const bool allowAnyStartingAction)
        : repeatingSequence_(std::move(repeatingSequence)),
          requiredHitCount_(std::max<std::size_t>(requiredHitCount, 1)), endTime_(endTime),
          allowAnyStartingAction_(allowAnyStartingAction)
    {
        if (repeatingSequence_.empty())
        {
            throw std::invalid_argument("A timed sequence requires at least one input action.");
        }
    }

    void TimedSequenceInputRule::Reset() noexcept
    {
        state_ = NoteState::Active;
        acceptedHitCount_ = 0;
        nextActionIndex_ = 0;
    }

    NoteState TimedSequenceInputRule::State() const noexcept
    {
        return state_;
    }

    bool TimedSequenceInputRule::CanAccept(const NoteRuleContext &context,
                                           const RhythmInputEvent &input,
                                           const JudgementResult &) const noexcept
    {
        if (IsTerminal(state_) || input.edge != InputEdge::Pressed || input.time < context.noteTime ||
            input.time > endTime_)
            return false;
        if (allowAnyStartingAction_ && acceptedHitCount_ == 0)
            return std::ranges::find(repeatingSequence_, input.action) != repeatingSequence_.end();
        return input.action == repeatingSequence_[nextActionIndex_];
    }

    void TimedSequenceInputRule::ProcessInput(const NoteRuleContext &context, const RhythmInputEvent &input,
                                              const JudgementResult &, NoteProcessResult &output)
    {
        const JudgementResult ungraded{JudgementGrade::Unjudged, {}, 1.0};
        if (!CanAccept(context, input, ungraded))
        {
            output.events.push_back(MakeEvent(context, NoteEventType::InputRejected, state_, state_, ungraded,
                                              input.time, acceptedHitCount_));
            return;
        }

        const NoteState before = state_;
        // Only an accepted first press chooses the phase. Rejected presses leave
        // both the count and the next expected action unchanged.
        if (allowAnyStartingAction_ && acceptedHitCount_ == 0)
            nextActionIndex_ = static_cast<std::size_t>(std::ranges::find(repeatingSequence_, input.action) -
                                                        repeatingSequence_.begin());
        nextActionIndex_ = (nextActionIndex_ + 1) % repeatingSequence_.size();
        const std::size_t acceptedIndex = acceptedHitCount_++;
        state_ = acceptedHitCount_ >= requiredHitCount_ ? NoteState::Completed
                                                        : NoteState::AwaitingAdditionalInput;
        output.events.push_back(
            MakeEvent(context, NoteEventType::HitAccepted, before, state_, ungraded, input.time, acceptedIndex));
        output.events.push_back(
            MakeEvent(context, state_ == NoteState::Completed ? NoteEventType::Completed : NoteEventType::StageAdvanced,
                      before, state_, ungraded, input.time, acceptedIndex));
    }

    void TimedSequenceInputRule::Update(const NoteRuleContext &context,
                                        const NoteUpdateContext &update, NoteProcessResult &output)
    {
        if (IsTerminal(state_) || update.time < endTime_)
        {
            return;
        }
        MarkMissed(context, endTime_, output);
    }

    void TimedSequenceInputRule::MarkMissed(const NoteRuleContext &context, const RhythmTime time,
                                            NoteProcessResult &output)
    {
        if (IsTerminal(state_))
        {
            return;
        }
        const NoteState before = state_;
        state_ = NoteState::Missed;
        output.events.push_back(MakeEvent(context, NoteEventType::Missed, before, state_,
                                          MissJudgement(context, time), time, acceptedHitCount_));
    }

    RhythmTime TimedSequenceInputRule::ExpireTime(const NoteRuleContext &) const noexcept
    {
        return endTime_;
    }

    std::optional<NoteProgress> TimedSequenceInputRule::Progress() const noexcept
    {
        return NoteProgress{acceptedHitCount_, requiredHitCount_};
    }
} // namespace finger_drum::rhythm
