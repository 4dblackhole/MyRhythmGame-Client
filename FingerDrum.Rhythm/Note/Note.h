#pragma once

#include "Common/RhythmTypes.h"
#include "Judgement/JudgementProfile.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <unordered_set>
#include <vector>

namespace finger_drum::rhythm
{
    enum class NoteState : std::uint8_t
    {
        Pending,
        Active,
        AwaitingAdditionalInput,
        Holding,
        Completed,
        Missed,
    };

    enum class InputEdge : std::uint8_t
    {
        Pressed,
        Released,
    };

    struct RhythmInputEvent
    {
        RhythmTime time{};
        NoteAction action{};
        PhysicalKey physicalKey{};
        InputEdge edge{InputEdge::Pressed};
    };

    enum class NoteEventType : std::uint8_t
    {
        InputRejected,
        HitAccepted,
        StageAdvanced,
        HoldStarted,
        HoldReleased,
        TickAccepted,
        TickMissed,
        Completed,
        Missed,
    };

    struct NoteEvent
    {
        NoteId noteId{};
        NoteEventType type{};
        NoteState stateBefore{NoteState::Pending};
        NoteState stateAfter{NoteState::Pending};
        JudgementResult judgement{};
        RhythmTime eventTime{};
        std::size_t hitIndex{};
        std::size_t tickIndex{};
        std::optional<NoteAction> inputAction;
    };

    struct AudioCueRequest
    {
        SoundId sound;
        AudioBusId bus{"HitSound"};
        RhythmTime timelineTime{};
        float volume{1.0F};
        float pitch{1.0F};
        float pan{};
        float reverbSend{};
        int priority{};
    };

    enum class NoteAccuracyKind : std::uint8_t
    {
        TimingHits,
        HitCount,
        Ticks,
        Hold,
    };

    struct NoteAccuracyTarget
    {
        NoteAccuracyKind kind{NoteAccuracyKind::TimingHits};
        std::size_t hits{1};
        std::size_t ticks{};
    };

    // A logical note contributes once, independently of its number of inputs.
    struct NoteAccuracy
    {
        NoteId noteId{};
        NoteAccuracyTarget target;
        std::vector<double> hitScoreRates;
        std::size_t acceptedHits{};
        std::size_t acceptedTicks{};
        bool finalized{};

        [[nodiscard]] double ScoreRate() const noexcept;
#if defined(_DEBUG)
        [[nodiscard]] std::wstring DebugText() const;
#endif
    };

    struct NoteProcessResult
    {
        std::vector<NoteEvent> events;
        std::vector<AudioCueRequest> audioCues;
        std::vector<NoteAccuracy> finalizedAccuracies;

        void Append(NoteProcessResult other);
    };

    class INoteSoundPolicy
    {
    public:
        virtual ~INoteSoundPolicy() = default;
        virtual void AppendAudioCues(
            const NoteEvent& event,
            std::vector<AudioCueRequest>& output) const = 0;
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
        MappedNoteSoundPolicy& Bind(SoundBinding binding);
        void AppendAudioCues(
            const NoteEvent& event,
            std::vector<AudioCueRequest>& output) const override;

    private:
        [[nodiscard]] static bool Matches(
            const SoundBinding& binding,
            const NoteEvent& event) noexcept;

        std::vector<SoundBinding> bindings_;
    };

    struct NoteRuleContext
    {
        NoteId noteId{};
        RhythmTime noteTime{};
        const JudgementProfile& judgementProfile;
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

        [[nodiscard]] bool operator==(
            const NoteProgress&) const noexcept = default;
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
        [[nodiscard]] virtual bool CanAccept(
            const NoteRuleContext& context,
            const RhythmInputEvent& input,
            const JudgementResult& judgement) const noexcept = 0;
        virtual void ProcessInput(
            const NoteRuleContext& context,
            const RhythmInputEvent& input,
            const JudgementResult& judgement,
            NoteProcessResult& output) = 0;
        virtual void Update(
            const NoteRuleContext& context,
            const NoteUpdateContext& update,
            NoteProcessResult& output) = 0;
        virtual void MarkMissed(
            const NoteRuleContext& context,
            RhythmTime time,
            NoteProcessResult& output) = 0;
        [[nodiscard]] virtual RhythmTime ExpireTime(
            const NoteRuleContext& context) const noexcept = 0;
        [[nodiscard]] virtual std::optional<NoteProgress>
            Progress() const noexcept
        {
            return std::nullopt;
        }
    };

    class INote
    {
    public:
        virtual ~INote() = default;
        [[nodiscard]] virtual const NoteAccuracy& Accuracy() const noexcept = 0;
        [[nodiscard]] virtual NoteId Id() const noexcept = 0;
        [[nodiscard]] virtual RhythmTime Timing() const noexcept = 0;
        [[nodiscard]] virtual RhythmTime ExpireTime() const noexcept = 0;
        [[nodiscard]] virtual NoteState State() const noexcept = 0;
        [[nodiscard]] virtual std::optional<NoteProgress>
            Progress() const noexcept = 0;
#if defined(_DEBUG)
        [[nodiscard]] virtual std::wstring DebugText() const = 0;
#endif
        [[nodiscard]] virtual const JudgementProfile& Profile() const noexcept = 0;
        [[nodiscard]] virtual JudgementResult Preview(
            const RhythmInputEvent& input) const noexcept = 0;
        [[nodiscard]] virtual bool CanAccept(
            const RhythmInputEvent& input) const noexcept = 0;
        [[nodiscard]] virtual NoteProcessResult ProcessInput(
            const RhythmInputEvent& input) = 0;
        [[nodiscard]] virtual NoteProcessResult Update(
            const NoteUpdateContext& update) = 0;
        [[nodiscard]] virtual NoteProcessResult MarkMissed(
            RhythmTime time) = 0;
        virtual void Reset() noexcept = 0;
    };

    class RuleBasedNote final : public INote
    {
    public:
        RuleBasedNote(
            NoteId id,
            RhythmTime timing,
            std::shared_ptr<const JudgementProfile> profile,
            std::unique_ptr<INoteRule> rule,
            std::shared_ptr<const INoteSoundPolicy> soundPolicy = {});
        ~RuleBasedNote() override;
        [[nodiscard]] const NoteAccuracy& Accuracy() const noexcept override;

        [[nodiscard]] NoteId Id() const noexcept override;
        [[nodiscard]] RhythmTime Timing() const noexcept override;
        [[nodiscard]] RhythmTime ExpireTime() const noexcept override;
        [[nodiscard]] NoteState State() const noexcept override;
        [[nodiscard]] std::optional<NoteProgress>
            Progress() const noexcept override;
#if defined(_DEBUG)
        [[nodiscard]] std::wstring DebugText() const override;
#endif
        [[nodiscard]] const JudgementProfile& Profile() const noexcept override;
        [[nodiscard]] JudgementResult Preview(
            const RhythmInputEvent& input) const noexcept override;
        [[nodiscard]] bool CanAccept(
            const RhythmInputEvent& input) const noexcept override;
        [[nodiscard]] NoteProcessResult ProcessInput(
            const RhythmInputEvent& input) override;
        [[nodiscard]] NoteProcessResult Update(
            const NoteUpdateContext& update) override;
        [[nodiscard]] NoteProcessResult MarkMissed(
            RhythmTime time) override;
        void Reset() noexcept override;

    private:
        [[nodiscard]] NoteRuleContext Context() const noexcept;
        void AppendAudioCues(NoteProcessResult& result) const;
        void AccumulateAccuracy(NoteProcessResult& result);

        NoteId id_{};
        RhythmTime timing_{};
        std::shared_ptr<const JudgementProfile> profile_;
        std::unique_ptr<INoteRule> rule_;
        std::shared_ptr<const INoteSoundPolicy> soundPolicy_;
        NoteAccuracy accuracy_;
    };

    class TapInputRule final : public INoteRule
    {
    public:
        explicit TapInputRule(
            NoteAction requiredAction,
            JudgementGrade maximumGrade = JudgementGrade::Bad) noexcept;

        void Reset() noexcept override;
        [[nodiscard]] NoteState State() const noexcept override;
        [[nodiscard]] bool CanAccept(
            const NoteRuleContext& context,
            const RhythmInputEvent& input,
            const JudgementResult& judgement) const noexcept override;
        void ProcessInput(
            const NoteRuleContext& context,
            const RhythmInputEvent& input,
            const JudgementResult& judgement,
            NoteProcessResult& output) override;
        void Update(
            const NoteRuleContext& context,
            const NoteUpdateContext& update,
            NoteProcessResult& output) override;
        void MarkMissed(
            const NoteRuleContext& context,
            RhythmTime time,
            NoteProcessResult& output) override;
        [[nodiscard]] RhythmTime ExpireTime(
            const NoteRuleContext& context) const noexcept override;

    private:
        NoteAction requiredAction_{};
        JudgementGrade maximumGrade_{JudgementGrade::Bad};
        NoteState state_{NoteState::Pending};
    };

    class CountedHitInputRule final : public INoteRule
    {
    public:
        [[nodiscard]] NoteAccuracyTarget AccuracyTarget() const noexcept override
        {
            return {NoteAccuracyKind::TimingHits, requiredHitCount_};
        }
        CountedHitInputRule(
            NoteAction requiredAction,
            std::size_t requiredHitCount,
            JudgementGrade maximumGrade = JudgementGrade::Good,
            bool requireDistinctPhysicalKeys = false,
            RhythmDuration maximumInterHitDuration =
                RhythmDuration::max());

        void Reset() noexcept override;
        [[nodiscard]] NoteState State() const noexcept override;
        [[nodiscard]] bool CanAccept(
            const NoteRuleContext& context,
            const RhythmInputEvent& input,
            const JudgementResult& judgement) const noexcept override;
        void ProcessInput(
            const NoteRuleContext& context,
            const RhythmInputEvent& input,
            const JudgementResult& judgement,
            NoteProcessResult& output) override;
        void Update(
            const NoteRuleContext& context,
            const NoteUpdateContext& update,
            NoteProcessResult& output) override;
        void MarkMissed(
            const NoteRuleContext& context,
            RhythmTime time,
            NoteProcessResult& output) override;
        [[nodiscard]] RhythmTime ExpireTime(
            const NoteRuleContext& context) const noexcept override;

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

    class SequenceInputRule final : public INoteRule
    {
    public:
        [[nodiscard]] NoteAccuracyTarget AccuracyTarget() const noexcept override
        {
            return {NoteAccuracyKind::TimingHits, sequence_.size()};
        }
        SequenceInputRule(
            std::vector<NoteAction> sequence,
            JudgementGrade maximumGrade = JudgementGrade::Good,
            bool allowAnyOrder = false);

        void Reset() noexcept override;
        [[nodiscard]] NoteState State() const noexcept override;
        [[nodiscard]] bool CanAccept(
            const NoteRuleContext& context,
            const RhythmInputEvent& input,
            const JudgementResult& judgement) const noexcept override;
        void ProcessInput(
            const NoteRuleContext& context,
            const RhythmInputEvent& input,
            const JudgementResult& judgement,
            NoteProcessResult& output) override;
        void Update(
            const NoteRuleContext& context,
            const NoteUpdateContext& update,
            NoteProcessResult& output) override;
        void MarkMissed(
            const NoteRuleContext& context,
            RhythmTime time,
            NoteProcessResult& output) override;
        [[nodiscard]] RhythmTime ExpireTime(
            const NoteRuleContext& context) const noexcept override;

    private:
        std::vector<NoteAction> sequence_;
        bool allowAnyOrder_{};
        JudgementGrade maximumGrade_{JudgementGrade::Good};
        NoteState state_{NoteState::Pending};
        std::size_t nextActionIndex_{};
    };

    class HoldInputRule final : public INoteRule
    {
    public:
        [[nodiscard]] NoteAccuracyTarget AccuracyTarget() const noexcept override
        {
            return {NoteAccuracyKind::Hold, 1, tickTimes_.size()};
        }
        HoldInputRule(
            NoteAction requiredAction,
            RhythmTime endTime,
            std::vector<RhythmTime> tickTimes,
            JudgementGrade startMaximumGrade = JudgementGrade::Good);

        void Reset() noexcept override;
        [[nodiscard]] NoteState State() const noexcept override;
        [[nodiscard]] bool CanAccept(
            const NoteRuleContext& context,
            const RhythmInputEvent& input,
            const JudgementResult& judgement) const noexcept override;
        void ProcessInput(
            const NoteRuleContext& context,
            const RhythmInputEvent& input,
            const JudgementResult& judgement,
            NoteProcessResult& output) override;
        void Update(
            const NoteRuleContext& context,
            const NoteUpdateContext& update,
            NoteProcessResult& output) override;
        void MarkMissed(
            const NoteRuleContext& context,
            RhythmTime time,
            NoteProcessResult& output) override;
        [[nodiscard]] RhythmTime ExpireTime(
            const NoteRuleContext& context) const noexcept override;

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
        void AdvanceTicks(const NoteRuleContext& context, RhythmTime time,
            NoteProcessResult& output);
    };

    class DrumRollInputRule final : public INoteRule
    {
    public:
        [[nodiscard]] NoteAccuracyTarget AccuracyTarget() const noexcept override
        {
            return {NoteAccuracyKind::HitCount, requiredHitCount_};
        }
        DrumRollInputRule(
            std::vector<NoteAction> acceptedActions,
            RhythmTime endTime,
            std::size_t requiredHitCount = 1);

        void Reset() noexcept override;
        [[nodiscard]] NoteState State() const noexcept override;
        [[nodiscard]] bool CanAccept(
            const NoteRuleContext& context,
            const RhythmInputEvent& input,
            const JudgementResult& judgement) const noexcept override;
        void ProcessInput(
            const NoteRuleContext& context,
            const RhythmInputEvent& input,
            const JudgementResult& judgement,
            NoteProcessResult& output) override;
        void Update(
            const NoteRuleContext& context,
            const NoteUpdateContext& update,
            NoteProcessResult& output) override;
        void MarkMissed(
            const NoteRuleContext& context,
            RhythmTime time,
            NoteProcessResult& output) override;
        [[nodiscard]] RhythmTime ExpireTime(
            const NoteRuleContext& context) const noexcept override;

    private:
        std::vector<NoteAction> acceptedActions_;
        RhythmTime endTime_{};
        NoteState state_{NoteState::Active};
        std::size_t tickCount_{};
        std::size_t requiredHitCount_{1};
    };

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
        TimedSequenceInputRule(
            std::vector<NoteAction> repeatingSequence,
            std::size_t requiredHitCount,
            RhythmTime endTime);

        void Reset() noexcept override;
        [[nodiscard]] NoteState State() const noexcept override;
        [[nodiscard]] bool CanAccept(
            const NoteRuleContext& context,
            const RhythmInputEvent& input,
            const JudgementResult& judgement) const noexcept override;
        void ProcessInput(
            const NoteRuleContext& context,
            const RhythmInputEvent& input,
            const JudgementResult& judgement,
            NoteProcessResult& output) override;
        void Update(
            const NoteRuleContext& context,
            const NoteUpdateContext& update,
            NoteProcessResult& output) override;
        void MarkMissed(
            const NoteRuleContext& context,
            RhythmTime time,
            NoteProcessResult& output) override;
        [[nodiscard]] RhythmTime ExpireTime(
            const NoteRuleContext& context) const noexcept override;
        [[nodiscard]] std::optional<NoteProgress>
            Progress() const noexcept override;

    private:
        std::vector<NoteAction> repeatingSequence_;
        std::size_t requiredHitCount_{1};
        RhythmTime endTime_{};
        NoteState state_{NoteState::Active};
        std::size_t acceptedHitCount_{};
    };

    // Caps a roll to one accepted press per authored tick. Each press is
    // evaluated against its tick with the supplied judgement window.
    class TickRollInputRule final : public INoteRule
    {
    public:
        [[nodiscard]] NoteAccuracyTarget AccuracyTarget() const noexcept override
        {
            return {NoteAccuracyKind::Ticks, 0, tickTimes_.size()};
        }
        TickRollInputRule(
            std::vector<NoteAction> acceptedActions,
            RhythmTime endTime,
            std::vector<RhythmTime> tickTimes,
            JudgementGrade maximumGrade = JudgementGrade::Good);

        void Reset() noexcept override;
        [[nodiscard]] NoteState State() const noexcept override;
        [[nodiscard]] bool CanAccept(
            const NoteRuleContext& context,
            const RhythmInputEvent& input,
            const JudgementResult& judgement) const noexcept override;
        void ProcessInput(
            const NoteRuleContext& context,
            const RhythmInputEvent& input,
            const JudgementResult& judgement,
            NoteProcessResult& output) override;
        void Update(
            const NoteRuleContext& context,
            const NoteUpdateContext& update,
            NoteProcessResult& output) override;
        void MarkMissed(
            const NoteRuleContext& context,
            RhythmTime time,
            NoteProcessResult& output) override;
        [[nodiscard]] RhythmTime ExpireTime(
            const NoteRuleContext& context) const noexcept override;

    private:
        void AppendExpiredTicks(
            const NoteRuleContext& context,
            RhythmTime time,
            NoteProcessResult& output);

        std::vector<NoteAction> acceptedActions_;
        RhythmTime endTime_{};
        std::vector<RhythmTime> tickTimes_;
        JudgementGrade maximumGrade_{JudgementGrade::Good};
        NoteState state_{NoteState::Active};
        std::size_t nextTickIndex_{};
    };
}
