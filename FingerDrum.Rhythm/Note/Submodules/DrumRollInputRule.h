#pragma once

#include "Note/Submodules/INoteRule.h"
#include <vector>

namespace finger_drum::rhythm
{
    class DrumRollInputRule final : public INoteRule
    {
      public:
        [[nodiscard]] NoteAccuracyTarget AccuracyTarget() const noexcept override
        {
            return {NoteAccuracyKind::HitCount, requiredHitCount_};
        }
        DrumRollInputRule(std::vector<NoteAction> acceptedActions, RhythmTime endTime,
                          std::size_t requiredHitCount = 1);

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
        std::vector<NoteAction> acceptedActions_;
        RhythmTime endTime_{};
        NoteState state_{NoteState::Active};
        std::size_t tickCount_{};
        std::size_t requiredHitCount_{1};
    };
} // namespace finger_drum::rhythm
