#include "Note/Submodules/TickRollInputRule.h"
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

    TickRollInputRule::TickRollInputRule(std::vector<NoteAction> acceptedActions,
                                         const RhythmTime endTime,
                                         std::vector<RhythmTime> tickTimes,
                                         const JudgementGrade maximumGrade)
        : acceptedActions_(std::move(acceptedActions)), endTime_(endTime),
          tickTimes_(std::move(tickTimes)), maximumGrade_(maximumGrade)
    {
        if (acceptedActions_.empty())
        {
            throw std::invalid_argument("A tick roll requires at least one input action.");
        }
        std::ranges::sort(tickTimes_);
    }

    void TickRollInputRule::Reset() noexcept
    {
        state_ = NoteState::Active;
        nextTickIndex_ = 0;
    }

    NoteState TickRollInputRule::State() const noexcept
    {
        return state_;
    }

    bool TickRollInputRule::CanAccept(const NoteRuleContext &context, const RhythmInputEvent &input,
                                      const JudgementResult &) const noexcept
    {
        if (IsTerminal(state_) || input.edge != InputEdge::Pressed || input.time > endTime_ ||
            std::ranges::find(acceptedActions_, input.action) == acceptedActions_.end())
        {
            return false;
        }

        const RhythmDuration halfWindow = context.judgementProfile.HalfWindow(maximumGrade_);
        std::size_t candidate = nextTickIndex_;
        while (candidate < tickTimes_.size() && input.time > tickTimes_[candidate] + halfWindow)
        {
            ++candidate;
        }
        return candidate < tickTimes_.size() &&
               context.judgementProfile.IsWithin(maximumGrade_, tickTimes_[candidate], input.time);
    }

    void TickRollInputRule::ProcessInput(const NoteRuleContext &context,
                                         const RhythmInputEvent &input,
                                         const JudgementResult &judgement,
                                         NoteProcessResult &output)
    {
        AppendExpiredTicks(context, input.time, output);
        if (!CanAccept(context, input, judgement))
        {
            output.events.push_back(MakeEvent(context, NoteEventType::InputRejected, state_, state_,
                                              {JudgementGrade::Unjudged, {}, 0.0}, input.time, 0,
                                              nextTickIndex_));
            return;
        }

        const std::size_t acceptedIndex = nextTickIndex_++;
        output.events.push_back(
            MakeEvent(context, NoteEventType::TickAccepted, state_, state_,
                      {JudgementGrade::Unjudged, input.time - tickTimes_[acceptedIndex], 1.0},
                      input.time, 0, acceptedIndex));
    }

    void TickRollInputRule::Update(const NoteRuleContext &context, const NoteUpdateContext &update,
                                   NoteProcessResult &output)
    {
        if (IsTerminal(state_))
        {
            return;
        }
        AppendExpiredTicks(context, update.time, output);
        if (update.time < endTime_)
        {
            return;
        }

        while (nextTickIndex_ < tickTimes_.size())
        {
            const std::size_t missedIndex = nextTickIndex_++;
            output.events.push_back(MakeEvent(context, NoteEventType::TickMissed, state_, state_,
                                              MissJudgement(context, tickTimes_[missedIndex]),
                                              tickTimes_[missedIndex], 0, missedIndex));
        }
        const NoteState before = state_;
        state_ = NoteState::Completed;
        output.events.push_back(MakeEvent(context, NoteEventType::Completed, before, state_,
                                          {JudgementGrade::Unjudged, {}, 1.0}, endTime_, 0,
                                          nextTickIndex_));
    }

    void TickRollInputRule::MarkMissed(const NoteRuleContext &context, const RhythmTime time,
                                       NoteProcessResult &output)
    {
        if (IsTerminal(state_))
        {
            return;
        }
        const NoteState before = state_;
        state_ = NoteState::Missed;
        output.events.push_back(MakeEvent(context, NoteEventType::Missed, before, state_,
                                          MissJudgement(context, time), time, 0, nextTickIndex_));
    }

    RhythmTime TickRollInputRule::ExpireTime(const NoteRuleContext &) const noexcept
    {
        return endTime_;
    }

    void TickRollInputRule::AppendExpiredTicks(const NoteRuleContext &context,
                                               const RhythmTime time, NoteProcessResult &output)
    {
        const RhythmDuration halfWindow = context.judgementProfile.HalfWindow(maximumGrade_);
        while (nextTickIndex_ < tickTimes_.size() && time > tickTimes_[nextTickIndex_] + halfWindow)
        {
            const std::size_t missedIndex = nextTickIndex_++;
            output.events.push_back(MakeEvent(context, NoteEventType::TickMissed, state_, state_,
                                              MissJudgement(context, tickTimes_[missedIndex]),
                                              tickTimes_[missedIndex], 0, missedIndex));
        }
    }
} // namespace finger_drum::rhythm
