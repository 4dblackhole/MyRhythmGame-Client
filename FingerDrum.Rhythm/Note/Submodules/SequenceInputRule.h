#pragma once

#include "Note/Submodules/INoteRule.h"
#include <vector>

namespace finger_drum::rhythm
{
    class SequenceInputRule final : public INoteRule
    {
      public:
        [[nodiscard]] NoteAccuracyTarget AccuracyTarget() const noexcept override
        {
            return {NoteAccuracyKind::TimingHits, sequence_.size()};
        }
        SequenceInputRule(std::vector<NoteAction> sequence,
                          JudgementGrade maximumGrade = JudgementGrade::Good,
                          bool allowAnyOrder = false);

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
        std::vector<NoteAction> sequence_;
        bool allowAnyOrder_{};
        JudgementGrade maximumGrade_{JudgementGrade::Good};
        NoteState state_{NoteState::Pending};
        std::size_t nextActionIndex_{};
    };
} // namespace finger_drum::rhythm
