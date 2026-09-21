#pragma once
#include "Note/Submodules/INoteRule.h"
#include <string_view>

namespace finger_drum::rhythm::detail
{
    [[nodiscard]] inline NoteEvent MakeEvent(const NoteRuleContext &context,
                                             const NoteEventType type, const NoteState before,
                                             const NoteState after, const JudgementResult judgement,
                                             const RhythmTime eventTime,
                                             const std::size_t hitIndex = 0,
                                             const std::size_t tickIndex = 0) noexcept
    {
        return NoteEvent{context.noteId, type,      before,   after,
                         judgement,      eventTime, hitIndex, tickIndex};
    }

    [[nodiscard]] inline JudgementResult MissJudgement(const NoteRuleContext &context,
                                                       const RhythmTime time) noexcept
    {
        return {JudgementGrade::Miss, time - context.noteTime, 0.0};
    }

    [[nodiscard]] inline bool IsTerminal(const NoteState state) noexcept
    {
        return state == NoteState::Completed || state == NoteState::Missed;
    }

#if defined(_DEBUG)
    [[nodiscard]] inline std::wstring_view StateName(const NoteState state) noexcept
    {
        switch (state)
        {
        case NoteState::Pending:
            return L"Pending";
        case NoteState::Active:
            return L"Active";
        case NoteState::AwaitingAdditionalInput:
            return L"Awaiting input";
        case NoteState::Holding:
            return L"Holding";
        case NoteState::Completed:
            return L"Completed";
        case NoteState::Missed:
            return L"Missed";
        default:
            return L"Unknown";
        }
    }
#endif
} // namespace finger_drum::rhythm::detail
