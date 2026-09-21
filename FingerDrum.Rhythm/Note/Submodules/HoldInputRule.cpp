#include "Note/Submodules/HoldInputRule.h"
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

    HoldInputRule::HoldInputRule(const NoteAction requiredAction, const RhythmTime endTime,
                                 std::vector<RhythmTime> tickTimes,
                                 const JudgementGrade startMaximumGrade)
        : requiredAction_(requiredAction), endTime_(endTime), tickTimes_(std::move(tickTimes)),
          startMaximumGrade_(startMaximumGrade)
    {
        std::ranges::sort(tickTimes_);
    }

    void HoldInputRule::Reset() noexcept
    {
        state_ = NoteState::Pending;
        nextTickIndex_ = 0;
        started_ = false;
        held_ = false;
        heldPhysicalKeys_.clear();
    }

    NoteState HoldInputRule::State() const noexcept
    {
        return state_;
    }

    bool HoldInputRule::CanAccept(const NoteRuleContext &, const RhythmInputEvent &input,
                                  const JudgementResult &judgement) const noexcept
    {
        if (IsTerminal(state_) || input.action != requiredAction_ || input.time > endTime_)
        {
            return false;
        }
        if (!started_)
        {
            return input.edge == InputEdge::Pressed &&
                   IsAtLeastAsAccurateAs(judgement.grade, startMaximumGrade_);
        }
        return true;
    }

    void HoldInputRule::ProcessInput(const NoteRuleContext &context, const RhythmInputEvent &input,
                                     const JudgementResult &judgement, NoteProcessResult &output)
    {
        if (!IsTerminal(state_))
        {
            // Preserve the held state before this timestamp. A press at a tick
            // accepts it; a release at that timestamp does not.
            AdvanceTicks(context, input.time - RhythmDuration{1}, output);
        }
        if (!CanAccept(context, input, judgement))
        {
            output.events.push_back(MakeEvent(context, NoteEventType::InputRejected, state_, state_,
                                              judgement, input.time));
            return;
        }

        const NoteState before = state_;
        if (input.edge == InputEdge::Pressed)
        {
            heldPhysicalKeys_.insert(input.physicalKey);
            held_ = true;
            state_ = NoteState::Holding;
            if (!started_)
            {
                started_ = true;
                output.events.push_back(MakeEvent(context, NoteEventType::HitAccepted, before,
                                                  state_, judgement, input.time));
                output.events.push_back(MakeEvent(context, NoteEventType::HoldStarted, before,
                                                  state_, judgement, input.time));
            }
            AdvanceTicks(context, input.time, output);
            return;
        }

        heldPhysicalKeys_.erase(input.physicalKey);
        held_ = !heldPhysicalKeys_.empty();
        state_ = held_ ? NoteState::Holding : NoteState::Active;
        output.events.push_back(
            MakeEvent(context, NoteEventType::HoldReleased, before, state_, judgement, input.time));
        AdvanceTicks(context, input.time, output);
    }

    void HoldInputRule::Update(const NoteRuleContext &context, const NoteUpdateContext &update,
                               NoteProcessResult &output)
    {
        if (IsTerminal(state_))
        {
            return;
        }
        if (started_)
        {
            held_ = update.IsHeld(requiredAction_);
            state_ = held_ ? NoteState::Holding : NoteState::Active;
        }

        AdvanceTicks(context, update.time, output);

        if (update.time < endTime_)
        {
            return;
        }
        const NoteState before = state_;
        state_ = started_ ? NoteState::Completed : NoteState::Missed;
        output.events.push_back(MakeEvent(
            context,
            state_ == NoteState::Completed ? NoteEventType::Completed : NoteEventType::Missed,
            before, state_,
            state_ == NoteState::Completed ? JudgementResult{JudgementGrade::Unjudged, {}, 1.0}
                                           : MissJudgement(context, update.time),
            endTime_));
    }

    void HoldInputRule::MarkMissed(const NoteRuleContext &context, const RhythmTime time,
                                   NoteProcessResult &output)
    {
        if (IsTerminal(state_))
        {
            return;
        }
        const NoteState before = state_;
        state_ = NoteState::Missed;
        output.events.push_back(MakeEvent(context, NoteEventType::Missed, before, state_,
                                          MissJudgement(context, time), time));
    }

    void HoldInputRule::AdvanceTicks(const NoteRuleContext &context, const RhythmTime time,
                                     NoteProcessResult &output)
    {
        while (nextTickIndex_ < tickTimes_.size() && tickTimes_[nextTickIndex_] <= time)
        {
            const std::size_t index = nextTickIndex_++;
            output.events.push_back(
                MakeEvent(context, held_ ? NoteEventType::TickAccepted : NoteEventType::TickMissed,
                          state_, state_, {JudgementGrade::Unjudged, {}, held_ ? 1.0 : 0.0},
                          tickTimes_[index], 0, index));
        }
    }

    RhythmTime HoldInputRule::ExpireTime(const NoteRuleContext &) const noexcept
    {
        return endTime_;
    }
} // namespace finger_drum::rhythm
