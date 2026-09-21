#pragma once

#include "Note/Submodules/INoteRule.h"
#include <unordered_set>
#include <vector>

namespace finger_drum::rhythm
{
    class HoldInputRule final : public INoteRule
    {
      public:
        [[nodiscard]] NoteAccuracyTarget AccuracyTarget() const noexcept override
        {
            return {NoteAccuracyKind::Hold, 1, tickTimes_.size()};
        }
        HoldInputRule(NoteAction requiredAction, RhythmTime endTime,
                      std::vector<RhythmTime> tickTimes,
                      JudgementGrade startMaximumGrade = JudgementGrade::Good);

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
        RhythmTime endTime_{};
        std::vector<RhythmTime> tickTimes_;
        JudgementGrade startMaximumGrade_{JudgementGrade::Good};
        NoteState state_{NoteState::Pending};
        std::size_t nextTickIndex_{};
        bool started_{};
        bool held_{};
        std::unordered_set<PhysicalKey> heldPhysicalKeys_;
        void AdvanceTicks(const NoteRuleContext &context, RhythmTime time,
                          NoteProcessResult &output);
    };
} // namespace finger_drum::rhythm
