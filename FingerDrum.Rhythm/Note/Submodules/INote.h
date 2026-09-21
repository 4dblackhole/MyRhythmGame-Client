#pragma once

#include "Note/Submodules/INoteRule.h"

namespace finger_drum::rhythm
{
    class INote
    {
      public:
        virtual ~INote() = default;
        [[nodiscard]] virtual const NoteAccuracy &Accuracy() const noexcept = 0;
        [[nodiscard]] virtual NoteId Id() const noexcept = 0;
        [[nodiscard]] virtual RhythmTime Timing() const noexcept = 0;
        [[nodiscard]] virtual RhythmTime ExpireTime() const noexcept = 0;
        [[nodiscard]] virtual NoteState State() const noexcept = 0;
        [[nodiscard]] virtual std::optional<NoteProgress> Progress() const noexcept = 0;
#if defined(_DEBUG)
        [[nodiscard]] virtual std::wstring DebugText() const = 0;
#endif
        [[nodiscard]] virtual const JudgementProfile &Profile() const noexcept = 0;
        [[nodiscard]] virtual JudgementResult Preview(
            const RhythmInputEvent &input) const noexcept = 0;
        [[nodiscard]] virtual bool CanAccept(const RhythmInputEvent &input) const noexcept = 0;
        [[nodiscard]] virtual NoteProcessResult ProcessInput(const RhythmInputEvent &input) = 0;
        [[nodiscard]] virtual NoteProcessResult Update(const NoteUpdateContext &update) = 0;
        [[nodiscard]] virtual NoteProcessResult MarkMissed(RhythmTime time) = 0;
        virtual void Reset() noexcept = 0;
    };
} // namespace finger_drum::rhythm
