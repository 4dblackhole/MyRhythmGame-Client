#pragma once

#include "Note/Submodules/INoteRule.h"
#include <unordered_set>

namespace finger_drum::rhythm
{
    class CountedHitInputRule final : public INoteRule
    {
      public:
        [[nodiscard]] NoteAccuracyTarget AccuracyTarget() const noexcept override
        {
            return {NoteAccuracyKind::TimingHits, requiredHitCount_};
        }
        CountedHitInputRule(NoteAction requiredAction, std::size_t requiredHitCount,
                            JudgementGrade maximumGrade = JudgementGrade::Good,
                            bool requireDistinctPhysicalKeys = false,
                            RhythmDuration maximumInterHitDuration = RhythmDuration::max());

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
        std::size_t requiredHitCount_{1};
        JudgementGrade maximumGrade_{JudgementGrade::Good};
        bool requireDistinctPhysicalKeys_{};
        RhythmDuration maximumInterHitDuration_{RhythmDuration::max()};
        NoteState state_{NoteState::Pending};
        std::size_t acceptedHitCount_{};
        RhythmTime firstHitTime_{};
        std::unordered_set<PhysicalKey> acceptedPhysicalKeys_;
    };
} // namespace finger_drum::rhythm
