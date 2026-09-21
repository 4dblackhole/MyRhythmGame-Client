#include "Note/Submodules/TimedSequenceInputRule.h"
#include "Note/Submodules/RuleHelpers.h"
#include <algorithm>
#include <format>
#include <iterator>
#include <numeric>
#include <stdexcept>
#include <utility>

namespace finger_drum::rhythm
{
    using namespace detail;

    TimedSequenceInputRule::TimedSequenceInputRule(std::vector<NoteAction> repeatingSequence,
                                                   const std::size_t requiredHitCount,
                                                   const RhythmTime endTime)
        : repeatingSequence_(std::move(repeatingSequence)),
          requiredHitCount_(std::max<std::size_t>(requiredHitCount, 1)), endTime_(endTime)
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
    }

    NoteState TimedSequenceInputRule::State() const noexcept
    {
        return state_;
    }

    bool TimedSequenceInputRule::CanAccept(const NoteRuleContext &context,
                                           const RhythmInputEvent &input,
                                           const JudgementResult &) const noexcept
    {
        return !IsTerminal(state_) && input.edge == InputEdge::Pressed &&
               input.time >= context.noteTime && input.time <= endTime_ &&
               input.action == repeatingSequence_[acceptedHitCount_ % repeatingSequence_.size()];
    }

    void TimedSequenceInputRule::ProcessInput(const NoteRuleContext &context,
                                              const RhythmInputEvent &input,
                                              const JudgementResult &judgement,
                                              NoteProcessResult &output)
    {
        if (!CanAccept(context, input, judgement))
        {
            output.events.push_back(MakeEvent(context, NoteEventType::InputRejected, state_, state_,
                                              judgement, input.time, acceptedHitCount_));
            return;
        }

        const NoteState before = state_;
        const std::size_t acceptedIndex = acceptedHitCount_++;
        state_ = acceptedHitCount_ >= requiredHitCount_ ? NoteState::Completed
                                                        : NoteState::AwaitingAdditionalInput;
        const JudgementResult accepted{JudgementGrade::Unjudged, {}, 1.0};
        output.events.push_back(MakeEvent(context, NoteEventType::HitAccepted, before, state_,
                                          accepted, input.time, acceptedIndex));
        output.events.push_back(MakeEvent(context,
                                          state_ == NoteState::Completed
                                              ? NoteEventType::Completed
                                              : NoteEventType::StageAdvanced,
                                          before, state_, accepted, input.time, acceptedIndex));
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
