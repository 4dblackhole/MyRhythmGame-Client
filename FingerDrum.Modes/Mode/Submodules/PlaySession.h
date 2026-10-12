#pragma once
#include "Automation/ScrollAutomation.h"
#include "Mode/Results/IPlayResultCollector.h"
#include "Mode/Submodules/PlaySessionTypes.h"
#include <algorithm>

namespace finger_drum::mode
{
    class PlaySession final
    {
      public:
        rhythm::ScrollGear &Gear() noexcept;
        [[nodiscard]] const rhythm::ScrollGear &Gear() const noexcept;
        void SetTimeline(chart::MusicalTimeline timeline);
        [[nodiscard]] const chart::MusicalTimeline &Timeline() const;
        void SetInputMapping(std::map<rhythm::PhysicalKey, rhythm::NoteAction> mapping);
        void SetFreeInputCue(rhythm::NoteAction action, rhythm::AudioCueRequest cue);
        void SetEffects(std::vector<chart::CompiledEffectCommand> effects);
        const std::vector<chart::CompiledEffectCommand> &Effects() const noexcept { return effects_; }
        void SetHitSoundFiles(std::map<rhythm::SoundId, std::filesystem::path> files);
        [[nodiscard]] const std::map<rhythm::SoundId, std::filesystem::path> &HitSoundFiles()
            const noexcept;
        void SetSoundOverrides(rhythm::SoundId source, std::vector<TimedSoundOverride> changes);
        void SetMeasureLines(std::vector<MeasureLinePresentationInfo> measureLines);
        [[nodiscard]] const std::vector<MeasureLinePresentationInfo> &MeasureLines() const noexcept;
        void SetNotePresentation(rhythm::NoteId noteId, NotePresentationInfo presentation);
        [[nodiscard]] const NotePresentationInfo *FindNotePresentation(
            rhythm::NoteId noteId) const noexcept;
        void SetNoteScrollMultiplier(rhythm::NoteId noteId, double multiplier);
        [[nodiscard]] double MinimumScrollMultiplier() const noexcept
        {
            return std::min(minimumScrollMultiplier_, minimumMeasureScrollMultiplier_) * scroll_.MinimumMultiplier();
        }
        long double ScrollDistance(rhythm::RhythmTime target, rhythm::RhythmTime current) const
        { return scroll_.Distance(target, current); }
        bool IsKiaiActive(rhythm::RhythmTime time) const;

        [[nodiscard]] rhythm::NoteProcessResult ProcessInput(rhythm::PhysicalKey physicalKey,
                                                             rhythm::InputEdge edge,
                                                             rhythm::RhythmTime time);
        [[nodiscard]] rhythm::NoteProcessResult Update(
            rhythm::RhythmTime time, std::span<const rhythm::PhysicalKey> heldPhysicalKeys = {});
        // The reference remains valid until the next evaluation or SetEffects.
        [[nodiscard]] const std::vector<AutomationValue> &EvaluateAutomation(
            rhythm::RhythmTime time) const;
        [[nodiscard]] std::string_view AutomationError() const noexcept
        {
            return automationError_;
        }
        void Reset() noexcept;
        void SetResultCollector(std::unique_ptr<IPlayResultCollector> collector);
        void SetResultInitialTime(rhythm::RhythmTime time) noexcept;
        std::unique_ptr<IPlayResult> TakeResult(PlayEndReason reason, rhythm::RhythmTime time);
        [[nodiscard]] std::optional<double> AccuracyRate() const noexcept;
        [[nodiscard]] std::size_t FinalizedNoteCount() const noexcept;
        [[nodiscard]] const std::optional<rhythm::NoteAccuracy> &LastNoteAccuracy() const noexcept;

      private:
        void AccumulateAccuracy(const rhythm::NoteProcessResult &result, bool keyPressed = false);
        void ResolveSoundOverrides(rhythm::NoteProcessResult &result) const;
        [[nodiscard]] static double Interpolate(const chart::CompiledEffectCommand &command,
                                                rhythm::RhythmTime time);
        double EvaluateTrack(std::size_t effectIndex, std::size_t trackIndex, rhythm::RhythmTime time) const;

        rhythm::ScrollGear gear_;
        std::optional<chart::MusicalTimeline> timeline_;
        std::map<rhythm::PhysicalKey, rhythm::NoteAction> inputMapping_;
        std::vector<rhythm::NoteAction> heldActions_;
        std::map<rhythm::NoteAction, rhythm::AudioCueRequest> freeInputCues_;
        std::map<rhythm::NoteId, NotePresentationInfo> notePresentation_;
        std::vector<chart::CompiledEffectCommand> effects_;
        chart::ScrollAutomation scroll_;
        std::map<std::pair<chart::EffectCommandType, std::string>, std::vector<std::size_t>> effectTracks_;
        mutable std::optional<rhythm::RhythmTime> automationTime_;
        mutable std::vector<AutomationValue> automationValues_;
        mutable std::vector<std::pair<std::size_t, std::size_t>> automationIndices_;
        mutable std::vector<std::optional<double>> automationLastValues_;
        mutable std::string automationError_;
        std::vector<MeasureLinePresentationInfo> measureLines_;
        std::map<rhythm::SoundId, std::filesystem::path> hitSoundFiles_;
        std::map<rhythm::SoundId, std::vector<TimedSoundOverride>, std::less<>> soundOverrides_;
        std::uint64_t accuracySum_{};
        std::unique_ptr<IPlayResultCollector> resultCollector_;
        double minimumScrollMultiplier_{1};
        double minimumMeasureScrollMultiplier_{1};
        std::size_t finalizedNoteCount_{};
        std::optional<rhythm::NoteAccuracy> lastNoteAccuracy_;
    };
} // namespace finger_drum::mode
