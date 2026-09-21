#include "Note/Submodules/TapInputRule.h"
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

    TapInputRule::TapInputRule(const NoteAction requiredAction,
                               const JudgementGrade maximumGrade) noexcept
        : requiredAction_(requiredAction), maximumGrade_(maximumGrade)
    {
    }

    void TapInputRule::Reset() noexcept
    {
        state_ = NoteState::Pending;
    }

    NoteState TapInputRule::State() const noexcept
    {
        return state_;
    }

    bool TapInputRule::CanAccept(const NoteRuleContext &, const RhythmInputEvent &input,
                                 const JudgementResult &judgement) const noexcept
    {
        return !IsTerminal(state_) && input.edge == InputEdge::Pressed &&
               input.action == requiredAction_ &&
               IsAtLeastAsAccurateAs(judgement.grade, maximumGrade_);
    }

    void TapInputRule::ProcessInput(const NoteRuleContext &context, const RhythmInputEvent &input,
                                    const JudgementResult &judgement, NoteProcessResult &output)
    {
        if (!CanAccept(context, input, judgement))
        {
            output.events.push_back(MakeEvent(context, NoteEventType::InputRejected, state_, state_,
                                              judgement, input.time));
            return;
        }

        const NoteState before = state_;
        state_ = NoteState::Completed;
        output.events.push_back(
            MakeEvent(context, NoteEventType::HitAccepted, before, state_, judgement, input.time));
        output.events.push_back(
            MakeEvent(context, NoteEventType::Completed, before, state_, judgement, input.time));
    }

    void TapInputRule::Update(const NoteRuleContext &, const NoteUpdateContext &,
                              NoteProcessResult &)
    {
    }

    void TapInputRule::MarkMissed(const NoteRuleContext &context, const RhythmTime time,
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

    RhythmTime TapInputRule::ExpireTime(const NoteRuleContext &context) const noexcept
    {
        return context.noteTime + context.judgementProfile.HalfWindow(JudgementGrade::Bad);
    }
} // namespace finger_drum::rhythm
