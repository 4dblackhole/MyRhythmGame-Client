#pragma once

#include "Note/Submodules/INoteRule.h"

namespace finger_drum::rhythm
{
    class TapInputRule final : public INoteRule
    {
      public:
        explicit TapInputRule(NoteAction requiredAction,
                              JudgementGrade maximumGrade = JudgementGrade::Bad) noexcept;

        void Reset() noexcept override;
        [[nodiscard]] NoteState State() const noexcept override;
        [[nodiscard]] bool CanAccept(const NoteRuleContext &context, const RhythmInputEvent &input,
                                     const JudgementResult &judgement) const noexcept override;
        void ProcessInput(const NoteRuleContext &context, const RhythmInputEvent &input,
                          const JudgementResult &judgement, NoteProcessResult &output) override;
        void Update(const NoteRuleContext &context, const NoteUpdateContext &update,
                    NoteProcessResult &output) override;
        void MarkMissed(const NoteRuleContext &context, RhythmTime time,
                        NoteProcessResult &output) override;
        [[nodiscard]] RhythmTime ExpireTime(const NoteRuleContext &context) const noexcept override;

      private:
        NoteAction requiredAction_{};
        JudgementGrade maximumGrade_{JudgementGrade::Bad};
        NoteState state_{NoteState::Pending};
    };
} // namespace finger_drum::rhythm
