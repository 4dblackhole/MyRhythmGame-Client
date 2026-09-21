#include "Note/Submodules/SequenceInputRule.h"
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

    SequenceInputRule::SequenceInputRule(std::vector<NoteAction> sequence,
                                         const JudgementGrade maximumGrade,
                                         const bool allowAnyOrder)
        : sequence_(std::move(sequence)), allowAnyOrder_(allowAnyOrder), maximumGrade_(maximumGrade)
    {
        if (sequence_.empty())
        {
            throw std::invalid_argument("An input sequence cannot be empty.");
        }
    }

    void SequenceInputRule::Reset() noexcept
    {
        state_ = NoteState::Pending;
        nextActionIndex_ = 0;
    }

    NoteState SequenceInputRule::State() const noexcept
    {
        return state_;
    }

    bool SequenceInputRule::CanAccept(const NoteRuleContext &, const RhythmInputEvent &input,
                                      const JudgementResult &judgement) const noexcept
    {
        return !IsTerminal(state_) && input.edge == InputEdge::Pressed &&
               (allowAnyOrder_ ? std::find(sequence_.begin() + nextActionIndex_, sequence_.end(),
                                           input.action) != sequence_.end()
                               : input.action == sequence_[nextActionIndex_]) &&
               IsAtLeastAsAccurateAs(judgement.grade, maximumGrade_);
    }

    void SequenceInputRule::ProcessInput(const NoteRuleContext &context,
                                         const RhythmInputEvent &input,
                                         const JudgementResult &judgement,
                                         NoteProcessResult &output)
    {
        if (!CanAccept(context, input, judgement))
        {
            output.events.push_back(MakeEvent(context, NoteEventType::InputRejected, state_, state_,
                                              judgement, input.time, nextActionIndex_));
            return;
        }

        const NoteState before = state_;
        if (allowAnyOrder_)
        {
            const auto next = sequence_.begin() + nextActionIndex_;
            std::iter_swap(next, std::find(next, sequence_.end(), input.action));
        }
        const std::size_t acceptedIndex = nextActionIndex_++;
        state_ = nextActionIndex_ == sequence_.size() ? NoteState::Completed
                                                      : NoteState::AwaitingAdditionalInput;
        output.events.push_back(MakeEvent(context, NoteEventType::HitAccepted, before, state_,
                                          judgement, input.time, acceptedIndex));
        output.events.push_back(MakeEvent(context,
                                          state_ == NoteState::Completed
                                              ? NoteEventType::Completed
                                              : NoteEventType::StageAdvanced,
                                          before, state_, judgement, input.time, acceptedIndex));
    }

    void SequenceInputRule::Update(const NoteRuleContext &, const NoteUpdateContext &,
                                   NoteProcessResult &)
    {
    }

    void SequenceInputRule::MarkMissed(const NoteRuleContext &context, const RhythmTime time,
                                       NoteProcessResult &output)
    {
        if (IsTerminal(state_))
        {
            return;
        }
        const NoteState before = state_;
        state_ = NoteState::Missed;
        output.events.push_back(MakeEvent(context, NoteEventType::Missed, before, state_,
                                          MissJudgement(context, time), time, nextActionIndex_));
    }

    RhythmTime SequenceInputRule::ExpireTime(const NoteRuleContext &context) const noexcept
    {
        return context.noteTime + context.judgementProfile.HalfWindow(maximumGrade_);
    }
} // namespace finger_drum::rhythm
