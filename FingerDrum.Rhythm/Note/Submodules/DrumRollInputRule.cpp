#include "Note/Submodules/DrumRollInputRule.h"
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

    DrumRollInputRule::DrumRollInputRule(std::vector<NoteAction> acceptedActions,
                                         const RhythmTime endTime,
                                         const std::size_t requiredHitCount)
        : acceptedActions_(std::move(acceptedActions)), endTime_(endTime),
          requiredHitCount_(std::max<std::size_t>(requiredHitCount, 1))
    {
        if (acceptedActions_.empty())
        {
            throw std::invalid_argument("A drum-roll rule needs at least one input action.");
        }
    }

    void DrumRollInputRule::Reset() noexcept
    {
        state_ = NoteState::Active;
        tickCount_ = 0;
    }

    NoteState DrumRollInputRule::State() const noexcept
    {
        return state_;
    }

    bool DrumRollInputRule::CanAccept(const NoteRuleContext &context, const RhythmInputEvent &input,
                                      const JudgementResult &) const noexcept
    {
        return !IsTerminal(state_) && input.edge == InputEdge::Pressed &&
               input.time >= context.noteTime && input.time <= endTime_ &&
               std::ranges::find(acceptedActions_, input.action) != acceptedActions_.end();
    }

    void DrumRollInputRule::ProcessInput(const NoteRuleContext &context,
                                         const RhythmInputEvent &input,
                                         const JudgementResult &judgement,
                                         NoteProcessResult &output)
    {
        if (!CanAccept(context, input, judgement))
        {
            output.events.push_back(MakeEvent(context, NoteEventType::InputRejected, state_, state_,
                                              judgement, input.time));
            return;
        }
        output.events.push_back(MakeEvent(context, NoteEventType::TickAccepted, state_, state_,
                                          {JudgementGrade::Unjudged, {}, 1.0}, input.time, 0,
                                          tickCount_++));
    }

    void DrumRollInputRule::Update(const NoteRuleContext &context, const NoteUpdateContext &update,
                                   NoteProcessResult &output)
    {
        if (IsTerminal(state_) || update.time < endTime_)
        {
            return;
        }
        const NoteState before = state_;
        state_ = tickCount_ >= requiredHitCount_ ? NoteState::Completed : NoteState::Missed;
        output.events.push_back(MakeEvent(
            context,
            state_ == NoteState::Completed ? NoteEventType::Completed : NoteEventType::Missed,
            before, state_, {JudgementGrade::Unjudged, {}, 1.0}, endTime_, tickCount_));
    }

    void DrumRollInputRule::MarkMissed(const NoteRuleContext &context, const RhythmTime time,
                                       NoteProcessResult &output)
    {
        if (IsTerminal(state_))
        {
            return;
        }
        const NoteState before = state_;
        state_ = NoteState::Missed;
        output.events.push_back(MakeEvent(context, NoteEventType::Missed, before, state_,
                                          MissJudgement(context, time), time, tickCount_));
    }

    RhythmTime DrumRollInputRule::ExpireTime(const NoteRuleContext &) const noexcept
    {
        return endTime_;
    }
} // namespace finger_drum::rhythm
