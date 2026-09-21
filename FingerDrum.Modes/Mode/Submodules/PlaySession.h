#pragma once
#include "Mode/Submodules/PlaySessionTypes.h"

namespace finger_drum::mode
{
    class PlaySession final
    {
      public:
        rhythm::ScrollGear &Gear() noexcept;
        [[nodiscard]] const rhythm::ScrollGear &Gear() const noexcept;
        void SetInputMapping(std::map<rhythm::PhysicalKey, rhythm::NoteAction> mapping);
        void SetFreeInputCue(rhythm::NoteAction action, rhythm::AudioCueRequest cue);
        void SetEffects(std::vector<chart::CompiledEffectCommand> effects);
        void SetHitSoundFiles(std::map<rhythm::SoundId, std::filesystem::path> files);
        [[nodiscard]] const std::map<rhythm::SoundId, std::filesystem::path> &HitSoundFiles()
            const noexcept;
        void SetSoundOverrides(rhythm::SoundId source, std::vector<TimedSoundOverride> changes);
        void SetMeasureLines(std::vector<rhythm::RhythmTime> measureLines);
        [[nodiscard]] const std::vector<rhythm::RhythmTime> &MeasureLines() const noexcept;
        void SetNotePresentation(rhythm::NoteId noteId, NotePresentationInfo presentation);
        [[nodiscard]] const NotePresentationInfo *FindNotePresentation(
            rhythm::NoteId noteId) const noexcept;
        void SetNoteScrollMultiplier(rhythm::NoteId noteId, double multiplier);
        [[nodiscard]] double MinimumScrollMultiplier() const noexcept
        {
            return minimumScrollMultiplier_;
        }

        [[nodiscard]] rhythm::NoteProcessResult ProcessInput(rhythm::PhysicalKey physicalKey,
                                                             rhythm::InputEdge edge,
                                                             rhythm::RhythmTime time);
        [[nodiscard]] rhythm::NoteProcessResult Update(
            rhythm::RhythmTime time, std::span<const rhythm::PhysicalKey> heldPhysicalKeys = {});
        [[nodiscard]] std::vector<AutomationValue> EvaluateAutomation(
            rhythm::RhythmTime time) const;
        void Reset() noexcept;
        [[nodiscard]] std::optional<double> AccuracyRate() const noexcept;
        [[nodiscard]] std::size_t FinalizedNoteCount() const noexcept;
        [[nodiscard]] const std::optional<rhythm::NoteAccuracy> &LastNoteAccuracy() const noexcept;

      private:
        void AccumulateAccuracy(const rhythm::NoteProcessResult &result);
        void ResolveSoundOverrides(rhythm::NoteProcessResult &result) const;
        [[nodiscard]] static double Interpolate(const chart::CompiledEffectCommand &command,
                                                rhythm::RhythmTime time) noexcept;

        rhythm::ScrollGear gear_;
        std::map<rhythm::PhysicalKey, rhythm::NoteAction> inputMapping_;
        std::map<rhythm::NoteAction, rhythm::AudioCueRequest> freeInputCues_;
        std::map<rhythm::NoteId, NotePresentationInfo> notePresentation_;
        std::vector<chart::CompiledEffectCommand> effects_;
        std::vector<rhythm::RhythmTime> measureLines_;
        std::map<rhythm::SoundId, std::filesystem::path> hitSoundFiles_;
        std::map<rhythm::SoundId, std::vector<TimedSoundOverride>, std::less<>> soundOverrides_;
        double accuracySum_{};
        double minimumScrollMultiplier_{1};
        std::size_t finalizedNoteCount_{};
        std::optional<rhythm::NoteAccuracy> lastNoteAccuracy_;
    };
} // namespace finger_drum::mode
