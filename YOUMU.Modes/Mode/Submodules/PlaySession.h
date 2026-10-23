#pragma once
#include "Automation/ScrollAutomation.h"
#include "Timing/MetronomeBeats.h"
#include "Mode/ModeInputLayout.h"
#include "Mode/Results/IPlayResultCollector.h"
#include "Mode/Results/PlayControlEvent.h"
#include "Mode/Submodules/PlaySessionTypes.h"
#include <algorithm>
#include <optional>
#include <vector>

class PlaySession final
{
  public:
    void SetPlayOptions(PlayOptions options, double baseBpm)
    { scrollMultiplier_ = options.ScrollMultiplier(baseBpm); playOptions_ = std::move(options); }
    const PlayOptions &Options() const noexcept { return playOptions_; }
    ScrollGear &Gear() noexcept;
    [[nodiscard]] const ScrollGear &Gear() const noexcept;
    void SetTimeline(MusicalTimeline timeline);
    std::vector<MetronomeBeat> metronomeBeats;
    [[nodiscard]] const MusicalTimeline &Timeline() const;
    void SetInputLayout(ModeInputLayout layout)
    {
        inputLayout_ = std::move(layout);
    }
    const ModeInputLayout &InputLayout() const noexcept
    {
        return inputLayout_;
    }
    const auto &InputMapping() const noexcept
    {
        return inputMapping_;
    }
    void SetInputMapping(std::map<PhysicalKey, NoteAction> mapping);
    void SetFreeInputCue(NoteAction action, AudioCueRequest cue);
    void SetEffects(std::vector<CompiledEffectCommand> effects);
    const std::vector<CompiledEffectCommand> &Effects() const noexcept { return effects_; }
    void SetHitSoundFiles(std::map<SoundId, std::filesystem::path> files);
    [[nodiscard]] const std::map<SoundId, std::filesystem::path> &HitSoundFiles()
        const noexcept;
    void SetSoundOverrides(SoundId source, std::vector<TimedSoundOverride> changes);
    void SetMeasureLines(std::vector<MeasureLinePresentationInfo> measureLines);
    [[nodiscard]] const std::vector<MeasureLinePresentationInfo> &MeasureLines() const noexcept;
    void SetNotePresentation(NoteId noteId, NotePresentationInfo presentation);
    [[nodiscard]] const NotePresentationInfo *FindNotePresentation(
        NoteId noteId) const noexcept;
    void SetNoteScrollMultiplier(NoteId noteId, double multiplier);
    std::uint64_t PresentationRevision() const noexcept
    {
        return presentationRevision_;
    }
    [[nodiscard]] double MinimumScrollMultiplier() const noexcept
    {
        return std::min(minimumScrollMultiplier_, minimumMeasureScrollMultiplier_) * scroll_.MinimumMultiplier() * scrollMultiplier_;
    }
    long double ScrollDistance(RhythmTime target, RhythmTime current) const
    { return scroll_.Distance(target, current) * scrollMultiplier_; }
    bool IsKiaiActive(RhythmTime time) const;

    [[nodiscard]] NoteProcessResult ProcessInput(PhysicalKey physicalKey,
                                                         InputEdge edge,
                                                         RhythmTime time);
    [[nodiscard]] NoteProcessResult Update(
        RhythmTime time, std::span<const PhysicalKey> heldPhysicalKeys = {});
    // The reference remains valid until the next evaluation or SetEffects.
    [[nodiscard]] const std::vector<AutomationValue> &EvaluateAutomation(
        RhythmTime time) const;
    [[nodiscard]] std::string_view AutomationError() const noexcept
    {
        return automationError_;
    }
    void Reset() noexcept;
    void RewindJudgements(RhythmTime target);
    void RetireRemaining(RhythmTime time);
    void RecordControl(PlayControlEvent event);
    bool CanUseHp() const noexcept
    {
        return !pauseMenuUsed_;
    }
    void SetResultCollector(std::unique_ptr<IPlayResultCollector> collector);
    void SetResultInitialTime(RhythmTime time) noexcept;
    std::unique_ptr<IPlayResult> TakeResult(PlayEndReason reason, RhythmTime time);
    [[nodiscard]] std::optional<double> AccuracyRate() const noexcept;
    [[nodiscard]] std::size_t FinalizedNoteCount() const noexcept;
    [[nodiscard]] const std::optional<NoteAccuracy> &LastNoteAccuracy() const noexcept;

  private:
    struct JudgementStep
    {
        RhythmTime time{};
        std::optional<RhythmInputEvent> input;
        std::vector<PhysicalKey> heldKeys;
    };
    void ResetNoteState() noexcept;
    void ApplyPhysicalInput(PhysicalKey key, InputEdge edge);
    void ResolveHeldActions(std::span<const PhysicalKey> keys);
    void RefreshHeldActions();
    std::vector<JudgementStep> judgementHistory_;
    std::vector<PhysicalKey> lastRecordedHeldKeys_;
    bool pauseMenuUsed_{};
    void AccumulateAccuracy(const NoteProcessResult &result, bool keyPressed = false);
    void ResolveSoundOverrides(NoteProcessResult &result) const;
    [[nodiscard]] static double Interpolate(const CompiledEffectCommand &command,
                                            RhythmTime time);
    double EvaluateTrack(std::size_t effectIndex, std::size_t trackIndex, RhythmTime time) const;

    PlayOptions playOptions_;
    double scrollMultiplier_{1.0};
    ModeInputLayout inputLayout_;
    ScrollGear gear_;
    std::optional<MusicalTimeline> timeline_;
    std::map<PhysicalKey, NoteAction> inputMapping_;
    std::vector<NoteAction> heldActions_;
    std::vector<PhysicalKey> heldPhysicalKeys_;
    std::map<NoteAction, AudioCueRequest> freeInputCues_;
    std::map<NoteId, NotePresentationInfo> notePresentation_;
    std::vector<CompiledEffectCommand> effects_;
    ScrollAutomation scroll_;
    std::map<std::pair<EffectCommandType, std::string>, std::vector<std::size_t>> effectTracks_;
    mutable std::optional<RhythmTime> automationTime_;
    mutable std::vector<AutomationValue> automationValues_;
    mutable std::vector<std::pair<std::size_t, std::size_t>> automationIndices_;
    mutable std::vector<std::optional<double>> automationLastValues_;
    mutable std::string automationError_;
    std::vector<MeasureLinePresentationInfo> measureLines_;
    std::map<SoundId, std::filesystem::path> hitSoundFiles_;
    std::map<SoundId, std::vector<TimedSoundOverride>, std::less<>> soundOverrides_;
    std::uint64_t accuracySum_{};
    std::unique_ptr<IPlayResultCollector> resultCollector_;
    double minimumScrollMultiplier_{1};
    double minimumMeasureScrollMultiplier_{1};
    std::size_t finalizedNoteCount_{};
    std::optional<NoteAccuracy> lastNoteAccuracy_;
    std::uint64_t presentationRevision_{1};
};
