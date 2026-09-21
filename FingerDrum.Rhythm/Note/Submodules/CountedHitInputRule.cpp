#include "Note/Submodules/CountedHitInputRule.h"
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

    CountedHitInputRule::CountedHitInputRule(const NoteAction requiredAction,
                                             const std::size_t requiredHitCount,
                                             const JudgementGrade maximumGrade,
                                             const bool requireDistinctPhysicalKeys,
                                             const RhythmDuration maximumInterHitDuration)
        : requiredAction_(requiredAction),
          requiredHitCount_(std::max<std::size_t>(requiredHitCount, 1)),
          maximumGrade_(maximumGrade), requireDistinctPhysicalKeys_(requireDistinctPhysicalKeys),
          maximumInterHitDuration_(maximumInterHitDuration)
    {
    }

    void CountedHitInputRule::Reset() noexcept
    {
        state_ = NoteState::Pending;
        acceptedHitCount_ = 0;
        firstHitTime_ = RhythmTime::zero();
        acceptedPhysicalKeys_.clear();
    }

    NoteState CountedHitInputRule::State() const noexcept
    {
        return state_;
    }

    bool CountedHitInputRule::CanAccept(const NoteRuleContext &, const RhythmInputEvent &input,
                                        const JudgementResult &judgement) const noexcept
    {
        if (IsTerminal(state_) || input.edge != InputEdge::Pressed ||
            input.action != requiredAction_ ||
            !IsAtLeastAsAccurateAs(judgement.grade, maximumGrade_))
        {
            return false;
        }
        if (acceptedHitCount_ > 0 && input.time - firstHitTime_ > maximumInterHitDuration_)
        {
            return false;
        }
        return !requireDistinctPhysicalKeys_ || !acceptedPhysicalKeys_.contains(input.physicalKey);
    }

    void CountedHitInputRule::ProcessInput(const NoteRuleContext &context,
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
        if (acceptedHitCount_ == 0)
        {
            firstHitTime_ = input.time;
        }
        acceptedPhysicalKeys_.insert(input.physicalKey);
        const std::size_t acceptedIndex = acceptedHitCount_++;
        state_ = acceptedHitCount_ >= requiredHitCount_ ? NoteState::Completed
                                                        : NoteState::AwaitingAdditionalInput;
        output.events.push_back(MakeEvent(context, NoteEventType::HitAccepted, before, state_,
                                          judgement, input.time, acceptedIndex));
        output.events.push_back(MakeEvent(context,
                                          state_ == NoteState::Completed
                                              ? NoteEventType::Completed
                                              : NoteEventType::StageAdvanced,
                                          before, state_, judgement, input.time, acceptedIndex));
    }

    void CountedHitInputRule::Update(const NoteRuleContext &, const NoteUpdateContext &,
                                     NoteProcessResult &)
    {
    }

    void CountedHitInputRule::MarkMissed(const NoteRuleContext &context, const RhythmTime time,
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

    RhythmTime CountedHitInputRule::ExpireTime(const NoteRuleContext &context) const noexcept
    {
        return context.noteTime + context.judgementProfile.HalfWindow(maximumGrade_);
    }
} // namespace finger_drum::rhythm
