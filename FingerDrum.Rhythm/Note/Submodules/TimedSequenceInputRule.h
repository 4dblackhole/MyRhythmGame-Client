#pragma once

#include "Note/Submodules/INoteRule.h"
#include <vector>

namespace finger_drum::rhythm
{
    // Accepts a fixed number of presses during a long-note interval. The
    // sequence repeats, so {Don} models a Balloon and {Don, Kat} models an
    // alternating long note without introducing game-mode concepts here.
    class TimedSequenceInputRule final : public INoteRule
    {
      public:
        [[nodiscard]] NoteAccuracyTarget AccuracyTarget() const noexcept override
        {
            return {NoteAccuracyKind::HitCount, requiredHitCount_};
        }
        TimedSequenceInputRule(std::vector<NoteAction> repeatingSequence,
                               std::size_t requiredHitCount, RhythmTime endTime);

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
        [[nodiscard]] std::optional<NoteProgress> Progress() const noexcept override;

      private:
        std::vector<NoteAction> repeatingSequence_;
        std::size_t requiredHitCount_{1};
        RhythmTime endTime_{};
        NoteState state_{NoteState::Active};
        std::size_t acceptedHitCount_{};
    };

    // Caps a roll to one accepted press per authored tick. Each press is
    // evaluated against its tick with the supplied judgement window.
} // namespace finger_drum::rhythm
