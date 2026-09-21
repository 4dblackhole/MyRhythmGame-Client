#pragma once

#include "Note/Submodules/NoteTypes.h"
#include <span>

namespace finger_drum::rhythm
{
    struct NoteRuleContext
    {
        NoteId noteId{};
        RhythmTime noteTime{};
        const JudgementProfile &judgementProfile;
    };

    struct NoteUpdateContext
    {
        RhythmTime time{};
        std::span<const NoteAction> heldActions;

        [[nodiscard]] bool IsHeld(NoteAction action) const noexcept;
    };

    struct NoteProgress
    {
        std::size_t accepted{};
        std::size_t required{};

        [[nodiscard]] bool operator==(const NoteProgress &) const noexcept = default;
    };

    class INoteRule
    {
      public:
        virtual ~INoteRule() = default;
        [[nodiscard]] virtual NoteAccuracyTarget AccuracyTarget() const noexcept
        {
            return {};
        }
        virtual void Reset() noexcept = 0;
        [[nodiscard]] virtual NoteState State() const noexcept = 0;
        [[nodiscard]] virtual bool CanAccept(const NoteRuleContext &context,
                                             const RhythmInputEvent &input,
                                             const JudgementResult &judgement) const noexcept = 0;
        virtual void ProcessInput(const NoteRuleContext &context, const RhythmInputEvent &input,
                                  const JudgementResult &judgement, NoteProcessResult &output) = 0;
        virtual void Update(const NoteRuleContext &context, const NoteUpdateContext &update,
                            NoteProcessResult &output) = 0;
        virtual void MarkMissed(const NoteRuleContext &context, RhythmTime time,
                                NoteProcessResult &output) = 0;
        [[nodiscard]] virtual RhythmTime ExpireTime(
            const NoteRuleContext &context) const noexcept = 0;
        [[nodiscard]] virtual std::optional<NoteProgress> Progress() const noexcept
        {
            return std::nullopt;
        }
    };
} // namespace finger_drum::rhythm
