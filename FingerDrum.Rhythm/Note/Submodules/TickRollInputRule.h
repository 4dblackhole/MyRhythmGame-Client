#pragma once

#include "Note/Submodules/INoteRule.h"
#include <vector>

namespace finger_drum::rhythm
{
    class TickRollInputRule final : public INoteRule
    {
      public:
        [[nodiscard]] NoteAccuracyTarget AccuracyTarget() const noexcept override
        {
            return {NoteAccuracyKind::Ticks, 0, tickTimes_.size()};
        }
        TickRollInputRule(std::vector<NoteAction> acceptedActions, RhythmTime endTime,
                          std::vector<RhythmTime> tickTimes,
                          JudgementGrade maximumGrade = JudgementGrade::Good);

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
        void AppendExpiredTicks(const NoteRuleContext &context, RhythmTime time,
                                NoteProcessResult &output);

        std::vector<NoteAction> acceptedActions_;
        RhythmTime endTime_{};
        std::vector<RhythmTime> tickTimes_;
        JudgementGrade maximumGrade_{JudgementGrade::Good};
        NoteState state_{NoteState::Active};
        std::size_t nextTickIndex_{};
    };
} // namespace finger_drum::rhythm
