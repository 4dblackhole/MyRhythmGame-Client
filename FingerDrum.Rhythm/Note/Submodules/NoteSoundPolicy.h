#pragma once

#include "Note/Submodules/NoteTypes.h"
#include <vector>

namespace finger_drum::rhythm
{
    class INoteSoundPolicy
    {
      public:
        virtual ~INoteSoundPolicy() = default;
        virtual void AppendAudioCues(const NoteEvent &event,
                                     std::vector<AudioCueRequest> &output) const = 0;
    };

    struct SoundBinding
    {
        NoteEventType eventType{NoteEventType::HitAccepted};
        std::optional<NoteState> stateBefore;
        std::optional<NoteState> stateAfter;
        std::optional<JudgementGrade> maximumGrade;
        std::optional<std::size_t> hitIndex;
        std::optional<std::size_t> tickIndex;
        AudioCueRequest cue;
        bool stopAfterMatch{};
        std::optional<NoteAction> inputAction;
    };

    class MappedNoteSoundPolicy final : public INoteSoundPolicy
    {
      public:
        MappedNoteSoundPolicy &Bind(SoundBinding binding);
        void AppendAudioCues(const NoteEvent &event,
                             std::vector<AudioCueRequest> &output) const override;

      private:
        [[nodiscard]] static bool Matches(const SoundBinding &binding,
                                          const NoteEvent &event) noexcept;

        std::vector<SoundBinding> bindings_;
    };
} // namespace finger_drum::rhythm
