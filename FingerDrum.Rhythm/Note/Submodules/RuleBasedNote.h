#pragma once

#include "Note/Submodules/INote.h"
#include "Note/Submodules/NoteSoundPolicy.h"
#include <memory>

namespace finger_drum::rhythm
{
    class RuleBasedNote final : public INote
    {
      public:
        RuleBasedNote(NoteId id, RhythmTime timing, std::shared_ptr<const JudgementProfile> profile,
                      std::unique_ptr<INoteRule> rule,
                      std::shared_ptr<const INoteSoundPolicy> soundPolicy = {});
        ~RuleBasedNote() override;
        [[nodiscard]] const NoteAccuracy &Accuracy() const noexcept override;

        [[nodiscard]] NoteId Id() const noexcept override;
        [[nodiscard]] RhythmTime Timing() const noexcept override;
        [[nodiscard]] RhythmTime ExpireTime() const noexcept override;
        [[nodiscard]] NoteState State() const noexcept override;
        [[nodiscard]] std::optional<NoteProgress> Progress() const noexcept override;
#if defined(_DEBUG)
        [[nodiscard]] std::wstring DebugText() const override;
#endif
        [[nodiscard]] const JudgementProfile &Profile() const noexcept override;
        [[nodiscard]] JudgementResult Preview(
            const RhythmInputEvent &input) const noexcept override;
        [[nodiscard]] bool CanAccept(const RhythmInputEvent &input) const noexcept override;
        [[nodiscard]] NoteProcessResult ProcessInput(const RhythmInputEvent &input) override;
        [[nodiscard]] NoteProcessResult Update(const NoteUpdateContext &update) override;
        [[nodiscard]] NoteProcessResult MarkMissed(RhythmTime time) override;
        void Reset() noexcept override;

      private:
        [[nodiscard]] NoteRuleContext Context() const noexcept;
        void AppendAudioCues(NoteProcessResult &result) const;
        void AccumulateAccuracy(NoteProcessResult &result);

        NoteId id_{};
        RhythmTime timing_{};
        std::shared_ptr<const JudgementProfile> profile_;
        std::unique_ptr<INoteRule> rule_;
        std::shared_ptr<const INoteSoundPolicy> soundPolicy_;
        NoteAccuracy accuracy_;
    };
} // namespace finger_drum::rhythm
