#include "Note/Note.h"

#include <algorithm>
#include <format>
#include <iterator>
#include <numeric>
#include <stdexcept>
#include <utility>

namespace finger_drum::rhythm
{
    namespace
    {
        [[nodiscard]] NoteEvent MakeEvent(
            const NoteRuleContext& context,
            const NoteEventType type,
            const NoteState before,
            const NoteState after,
            const JudgementResult judgement,
            const RhythmTime eventTime,
            const std::size_t hitIndex = 0,
            const std::size_t tickIndex = 0) noexcept
        {
            return NoteEvent{
                context.noteId,
                type,
                before,
                after,
                judgement,
                eventTime,
                hitIndex,
                tickIndex};
        }

        [[nodiscard]] JudgementResult MissJudgement(
            const NoteRuleContext& context,
            const RhythmTime time) noexcept
        {
            return {
                JudgementGrade::Miss,
                time - context.noteTime,
                0.0};
        }

        [[nodiscard]] bool IsTerminal(const NoteState state) noexcept
        {
            return state == NoteState::Completed ||
                state == NoteState::Missed;
        }

#if defined(_DEBUG)
        [[nodiscard]] std::wstring_view StateName(
            const NoteState state) noexcept
        {
            switch (state)
            {
            case NoteState::Pending: return L"Pending";
            case NoteState::Active: return L"Active";
            case NoteState::AwaitingAdditionalInput: return L"Awaiting input";
            case NoteState::Holding: return L"Holding";
            case NoteState::Completed: return L"Completed";
            case NoteState::Missed: return L"Missed";
            default: return L"Unknown";
            }
        }
#endif
    }

    void NoteProcessResult::Append(NoteProcessResult other)
    {
        events.insert(
            events.end(),
            std::make_move_iterator(other.events.begin()),
            std::make_move_iterator(other.events.end()));
        audioCues.insert(
            audioCues.end(),
            std::make_move_iterator(other.audioCues.begin()),
            std::make_move_iterator(other.audioCues.end()));
        finalizedAccuracies.insert(
            finalizedAccuracies.end(),
            std::make_move_iterator(other.finalizedAccuracies.begin()),
            std::make_move_iterator(other.finalizedAccuracies.end()));
    }

    double NoteAccuracy::ScoreRate() const noexcept
    {
        const auto ratio = [](std::size_t accepted, std::size_t required)
        {
            return required == 0 ? 0.0 : std::min(1.0,
                static_cast<double>(accepted) / static_cast<double>(required));
        };
        const double timing = hitScoreRates.empty() ? 0.0 :
            std::accumulate(hitScoreRates.begin(), hitScoreRates.end(), 0.0) /
                static_cast<double>(hitScoreRates.size());
        switch (target.kind)
        {
        case NoteAccuracyKind::TimingHits: return timing;
        case NoteAccuracyKind::HitCount: return ratio(acceptedHits, target.hits);
        case NoteAccuracyKind::Ticks: return ratio(acceptedTicks, target.ticks);
        case NoteAccuracyKind::Hold:
            return target.ticks == 0 ? timing :
                0.5 * timing + 0.5 * ratio(acceptedTicks, target.ticks);
        }
        return 0.0;
    }

#if defined(_DEBUG)
    std::wstring NoteAccuracy::DebugText() const
    {
        std::wstring result;
        for (std::size_t index = 0; index < hitScoreRates.size(); ++index)
        {
            result += std::format(L"Hit{}={:.2f}%{}  ", index + 1,
                hitScoreRates[index] * 100.0,
                index < acceptedHits ? L"" : L" (unfilled)");
        }
        if (target.kind == NoteAccuracyKind::HitCount)
        {
            result += std::format(L"Hits={}/{}  ", acceptedHits, target.hits);
        }
        if (target.kind == NoteAccuracyKind::Hold ||
            target.kind == NoteAccuracyKind::Ticks)
        {
            const double ratio = target.ticks == 0 ? 0.0 :
                100.0 * static_cast<double>(acceptedTicks) / target.ticks;
            result += std::format(L"Ticks={}/{} ({:.2f}%){}  ",
                acceptedTicks, target.ticks, ratio,
                target.kind == NoteAccuracyKind::Hold ? L" [head/ticks 50:50]" : L"");
        }
        return result + std::format(L"{}={:.2f}%",
            finalized ? L"Final" : L"Current", ScoreRate() * 100.0);
    }
#endif

    MappedNoteSoundPolicy& MappedNoteSoundPolicy::Bind(
        SoundBinding binding)
    {
        bindings_.push_back(std::move(binding));
        return *this;
    }

    void MappedNoteSoundPolicy::AppendAudioCues(
        const NoteEvent& event,
        std::vector<AudioCueRequest>& output) const
    {
        for (const SoundBinding& binding : bindings_)
        {
            if (!Matches(binding, event))
            {
                continue;
            }
            AudioCueRequest cue = binding.cue;
            cue.timelineTime = event.eventTime;
            output.push_back(std::move(cue));
            if (binding.stopAfterMatch)
            {
                break;
            }
        }
    }

    bool MappedNoteSoundPolicy::Matches(
        const SoundBinding& binding,
        const NoteEvent& event) noexcept
    {
        if (binding.eventType != event.type ||
            (binding.inputAction.has_value() && binding.inputAction != event.inputAction) ||
            (binding.stateBefore.has_value() &&
                *binding.stateBefore != event.stateBefore) ||
            (binding.stateAfter.has_value() &&
                *binding.stateAfter != event.stateAfter) ||
            (binding.hitIndex.has_value() &&
                *binding.hitIndex != event.hitIndex) ||
            (binding.tickIndex.has_value() &&
                *binding.tickIndex != event.tickIndex))
        {
            return false;
        }
        return !binding.maximumGrade.has_value() ||
            IsAtLeastAsAccurateAs(
                event.judgement.grade,
                *binding.maximumGrade);
    }

    bool NoteUpdateContext::IsHeld(const NoteAction action) const noexcept
    {
        return std::ranges::find(heldActions, action) != heldActions.end();
    }

    RuleBasedNote::RuleBasedNote(
        const NoteId id,
        const RhythmTime timing,
        std::shared_ptr<const JudgementProfile> profile,
        std::unique_ptr<INoteRule> rule,
        std::shared_ptr<const INoteSoundPolicy> soundPolicy)
        : id_(id),
          timing_(timing),
          profile_(std::move(profile)),
          rule_(std::move(rule)),
          soundPolicy_(std::move(soundPolicy))
    {
        if (profile_ == nullptr || rule_ == nullptr)
        {
            throw std::invalid_argument(
                "A rule-based note requires a profile and input rule.");
        }
        accuracy_.noteId = id_;
        accuracy_.target = rule_->AccuracyTarget();
        if (accuracy_.target.kind == NoteAccuracyKind::TimingHits ||
            accuracy_.target.kind == NoteAccuracyKind::Hold)
        {
            accuracy_.hitScoreRates.resize(accuracy_.target.hits);
        }
    }

    RuleBasedNote::~RuleBasedNote() = default;

    const NoteAccuracy& RuleBasedNote::Accuracy() const noexcept
    {
        return accuracy_;
    }

    NoteId RuleBasedNote::Id() const noexcept
    {
        return id_;
    }

    RhythmTime RuleBasedNote::Timing() const noexcept
    {
        return timing_;
    }

    RhythmTime RuleBasedNote::ExpireTime() const noexcept
    {
        return rule_->ExpireTime(Context());
    }

    NoteState RuleBasedNote::State() const noexcept
    {
        return rule_->State();
    }

    std::optional<NoteProgress> RuleBasedNote::Progress() const noexcept
    {
        return rule_->Progress();
    }

#if defined(_DEBUG)
    std::wstring RuleBasedNote::DebugText() const
    {
        std::wstring result = std::format(
            L"Note #{}  {}  timing={} us  expire={} us",
            id_,
            StateName(State()),
            timing_.count(),
            ExpireTime().count());
        if (const std::optional<NoteProgress> progress = Progress())
        {
            result += std::format(
                L"  progress={}/{}",
                progress->accepted,
                progress->required);
        }
        return result + L"\n" + accuracy_.DebugText();
    }
#endif

    const JudgementProfile& RuleBasedNote::Profile() const noexcept
    {
        return *profile_;
    }

    JudgementResult RuleBasedNote::Preview(
        const RhythmInputEvent& input) const noexcept
    {
        return profile_->Evaluate(timing_, input.time);
    }

    bool RuleBasedNote::CanAccept(
        const RhythmInputEvent& input) const noexcept
    {
        const JudgementResult judgement = Preview(input);
        return rule_->CanAccept(Context(), input, judgement);
    }

    NoteProcessResult RuleBasedNote::ProcessInput(
        const RhythmInputEvent& input)
    {
        NoteProcessResult result;
        const JudgementResult judgement = Preview(input);
        rule_->ProcessInput(Context(), input, judgement, result);
        for (NoteEvent& event : result.events)
        {
            if (event.type == NoteEventType::HitAccepted)
            {
                event.inputAction = input.action;
            }
        }
        AccumulateAccuracy(result);
        AppendAudioCues(result);
        return result;
    }

    NoteProcessResult RuleBasedNote::Update(
        const NoteUpdateContext& update)
    {
        NoteProcessResult result;
        rule_->Update(Context(), update, result);
        AccumulateAccuracy(result);
        AppendAudioCues(result);
        return result;
    }

    NoteProcessResult RuleBasedNote::MarkMissed(const RhythmTime time)
    {
        NoteProcessResult result;
        rule_->MarkMissed(Context(), time, result);
        AccumulateAccuracy(result);
        AppendAudioCues(result);
        return result;
    }

    void RuleBasedNote::Reset() noexcept
    {
        rule_->Reset();
        std::ranges::fill(accuracy_.hitScoreRates, 0.0);
        accuracy_.acceptedHits = 0;
        accuracy_.acceptedTicks = 0;
        accuracy_.finalized = false;
    }

    void RuleBasedNote::AccumulateAccuracy(NoteProcessResult& result)
    {
        if (accuracy_.finalized)
        {
            return;
        }
        for (const NoteEvent& event : result.events)
        {
            if (event.type == NoteEventType::HitAccepted)
            {
                if (event.hitIndex < accuracy_.hitScoreRates.size())
                {
                    accuracy_.hitScoreRates[event.hitIndex] = event.judgement.scoreRate;
                }
                ++accuracy_.acceptedHits;
            }
            else if (event.type == NoteEventType::TickAccepted)
            {
                if (accuracy_.target.kind == NoteAccuracyKind::HitCount)
                {
                    ++accuracy_.acceptedHits;
                }
                else
                {
                    ++accuracy_.acceptedTicks;
                }
            }
            else if (event.type == NoteEventType::Completed ||
                event.type == NoteEventType::Missed)
            {
                accuracy_.finalized = true;
                result.finalizedAccuracies.push_back(accuracy_);
                break;
            }
        }
    }

    NoteRuleContext RuleBasedNote::Context() const noexcept
    {
        return {id_, timing_, *profile_};
    }

    void RuleBasedNote::AppendAudioCues(NoteProcessResult& result) const
    {
        if (soundPolicy_ == nullptr)
        {
            return;
        }
        for (const NoteEvent& event : result.events)
        {
            soundPolicy_->AppendAudioCues(event, result.audioCues);
        }
    }

    TapInputRule::TapInputRule(
        const NoteAction requiredAction,
        const JudgementGrade maximumGrade) noexcept
        : requiredAction_(requiredAction),
          maximumGrade_(maximumGrade)
    {
    }

    void TapInputRule::Reset() noexcept
    {
        state_ = NoteState::Pending;
    }

    NoteState TapInputRule::State() const noexcept
    {
        return state_;
    }

    bool TapInputRule::CanAccept(
        const NoteRuleContext&,
        const RhythmInputEvent& input,
        const JudgementResult& judgement) const noexcept
    {
        return !IsTerminal(state_) &&
            input.edge == InputEdge::Pressed &&
            input.action == requiredAction_ &&
            IsAtLeastAsAccurateAs(judgement.grade, maximumGrade_);
    }

    void TapInputRule::ProcessInput(
        const NoteRuleContext& context,
        const RhythmInputEvent& input,
        const JudgementResult& judgement,
        NoteProcessResult& output)
    {
        if (!CanAccept(context, input, judgement))
        {
            output.events.push_back(MakeEvent(
                context,
                NoteEventType::InputRejected,
                state_,
                state_,
                judgement,
                input.time));
            return;
        }

        const NoteState before = state_;
        state_ = NoteState::Completed;
        output.events.push_back(MakeEvent(
            context,
            NoteEventType::HitAccepted,
            before,
            state_,
            judgement,
            input.time));
        output.events.push_back(MakeEvent(
            context,
            NoteEventType::Completed,
            before,
            state_,
            judgement,
            input.time));
    }

    void TapInputRule::Update(
        const NoteRuleContext&,
        const NoteUpdateContext&,
        NoteProcessResult&)
    {
    }

    void TapInputRule::MarkMissed(
        const NoteRuleContext& context,
        const RhythmTime time,
        NoteProcessResult& output)
    {
        if (IsTerminal(state_))
        {
            return;
        }
        const NoteState before = state_;
        state_ = NoteState::Missed;
        output.events.push_back(MakeEvent(
            context,
            NoteEventType::Missed,
            before,
            state_,
            MissJudgement(context, time),
            time));
    }

    RhythmTime TapInputRule::ExpireTime(
        const NoteRuleContext& context) const noexcept
    {
        return context.noteTime +
            context.judgementProfile.HalfWindow(JudgementGrade::Bad);
    }

    CountedHitInputRule::CountedHitInputRule(
        const NoteAction requiredAction,
        const std::size_t requiredHitCount,
        const JudgementGrade maximumGrade,
        const bool requireDistinctPhysicalKeys,
        const RhythmDuration maximumInterHitDuration)
        : requiredAction_(requiredAction),
          requiredHitCount_(std::max<std::size_t>(requiredHitCount, 1)),
          maximumGrade_(maximumGrade),
          requireDistinctPhysicalKeys_(requireDistinctPhysicalKeys),
          maximumInterHitDuration_(maximumInterHitDuration)
    {
    }

    void CountedHitInputRule::Reset() noexcept
    {
        state_ = NoteState::Pending;
        acceptedHitCount_ = 0;
        firstHitTime_ = RhythmTime::zero();
        acceptedPhysicalKeys_.clear();
    }

    NoteState CountedHitInputRule::State() const noexcept
    {
        return state_;
    }

    bool CountedHitInputRule::CanAccept(
        const NoteRuleContext&,
        const RhythmInputEvent& input,
        const JudgementResult& judgement) const noexcept
    {
        if (IsTerminal(state_) ||
            input.edge != InputEdge::Pressed ||
            input.action != requiredAction_ ||
            !IsAtLeastAsAccurateAs(judgement.grade, maximumGrade_))
        {
            return false;
        }
        if (acceptedHitCount_ > 0 &&
            input.time - firstHitTime_ > maximumInterHitDuration_)
        {
            return false;
        }
        return !requireDistinctPhysicalKeys_ ||
            !acceptedPhysicalKeys_.contains(input.physicalKey);
    }

    void CountedHitInputRule::ProcessInput(
        const NoteRuleContext& context,
        const RhythmInputEvent& input,
        const JudgementResult& judgement,
        NoteProcessResult& output)
    {
        if (!CanAccept(context, input, judgement))
        {
            output.events.push_back(MakeEvent(
                context,
                NoteEventType::InputRejected,
                state_,
                state_,
                judgement,
                input.time,
                acceptedHitCount_));
            return;
        }

        const NoteState before = state_;
        if (acceptedHitCount_ == 0)
        {
            firstHitTime_ = input.time;
        }
        acceptedPhysicalKeys_.insert(input.physicalKey);
        const std::size_t acceptedIndex = acceptedHitCount_++;
        state_ = acceptedHitCount_ >= requiredHitCount_
            ? NoteState::Completed
            : NoteState::AwaitingAdditionalInput;
        output.events.push_back(MakeEvent(
            context,
            NoteEventType::HitAccepted,
            before,
            state_,
            judgement,
            input.time,
            acceptedIndex));
        output.events.push_back(MakeEvent(
            context,
            state_ == NoteState::Completed
                ? NoteEventType::Completed
                : NoteEventType::StageAdvanced,
            before,
            state_,
            judgement,
            input.time,
            acceptedIndex));
    }

    void CountedHitInputRule::Update(
        const NoteRuleContext&,
        const NoteUpdateContext&,
        NoteProcessResult&)
    {
    }

    void CountedHitInputRule::MarkMissed(
        const NoteRuleContext& context,
        const RhythmTime time,
        NoteProcessResult& output)
    {
        if (IsTerminal(state_))
        {
            return;
        }
        const NoteState before = state_;
        state_ = NoteState::Missed;
        output.events.push_back(MakeEvent(
            context,
            NoteEventType::Missed,
            before,
            state_,
            MissJudgement(context, time),
            time,
            acceptedHitCount_));
    }

    RhythmTime CountedHitInputRule::ExpireTime(
        const NoteRuleContext& context) const noexcept
    {
        return context.noteTime +
            context.judgementProfile.HalfWindow(maximumGrade_);
    }

    SequenceInputRule::SequenceInputRule(
        std::vector<NoteAction> sequence,
        const JudgementGrade maximumGrade,
        const bool allowAnyOrder)
        : sequence_(std::move(sequence)),
          allowAnyOrder_(allowAnyOrder),
          maximumGrade_(maximumGrade)
    {
        if (sequence_.empty())
        {
            throw std::invalid_argument("An input sequence cannot be empty.");
        }
    }

    void SequenceInputRule::Reset() noexcept
    {
        state_ = NoteState::Pending;
        nextActionIndex_ = 0;
    }

    NoteState SequenceInputRule::State() const noexcept
    {
        return state_;
    }

    bool SequenceInputRule::CanAccept(
        const NoteRuleContext&,
        const RhythmInputEvent& input,
        const JudgementResult& judgement) const noexcept
    {
        return !IsTerminal(state_) &&
            input.edge == InputEdge::Pressed &&
            (allowAnyOrder_
                ? std::find(sequence_.begin() + nextActionIndex_, sequence_.end(),
                    input.action) != sequence_.end()
                : input.action == sequence_[nextActionIndex_]) &&
            IsAtLeastAsAccurateAs(judgement.grade, maximumGrade_);
    }

    void SequenceInputRule::ProcessInput(
        const NoteRuleContext& context,
        const RhythmInputEvent& input,
        const JudgementResult& judgement,
        NoteProcessResult& output)
    {
        if (!CanAccept(context, input, judgement))
        {
            output.events.push_back(MakeEvent(
                context,
                NoteEventType::InputRejected,
                state_,
                state_,
                judgement,
                input.time,
                nextActionIndex_));
            return;
        }

        const NoteState before = state_;
        if (allowAnyOrder_)
        {
            const auto next = sequence_.begin() + nextActionIndex_;
            std::iter_swap(next, std::find(next, sequence_.end(), input.action));
        }
        const std::size_t acceptedIndex = nextActionIndex_++;
        state_ = nextActionIndex_ == sequence_.size()
            ? NoteState::Completed
            : NoteState::AwaitingAdditionalInput;
        output.events.push_back(MakeEvent(
            context,
            NoteEventType::HitAccepted,
            before,
            state_,
            judgement,
            input.time,
            acceptedIndex));
        output.events.push_back(MakeEvent(
            context,
            state_ == NoteState::Completed
                ? NoteEventType::Completed
                : NoteEventType::StageAdvanced,
            before,
            state_,
            judgement,
            input.time,
            acceptedIndex));
    }

    void SequenceInputRule::Update(
        const NoteRuleContext&,
        const NoteUpdateContext&,
        NoteProcessResult&)
    {
    }

    void SequenceInputRule::MarkMissed(
        const NoteRuleContext& context,
        const RhythmTime time,
        NoteProcessResult& output)
    {
        if (IsTerminal(state_))
        {
            return;
        }
        const NoteState before = state_;
        state_ = NoteState::Missed;
        output.events.push_back(MakeEvent(
            context,
            NoteEventType::Missed,
            before,
            state_,
            MissJudgement(context, time),
            time,
            nextActionIndex_));
    }

    RhythmTime SequenceInputRule::ExpireTime(
        const NoteRuleContext& context) const noexcept
    {
        return context.noteTime +
            context.judgementProfile.HalfWindow(maximumGrade_);
    }

    HoldInputRule::HoldInputRule(
        const NoteAction requiredAction,
        const RhythmTime endTime,
        std::vector<RhythmTime> tickTimes,
        const JudgementGrade startMaximumGrade)
        : requiredAction_(requiredAction),
          endTime_(endTime),
          tickTimes_(std::move(tickTimes)),
          startMaximumGrade_(startMaximumGrade)
    {
        std::ranges::sort(tickTimes_);
    }

    void HoldInputRule::Reset() noexcept
    {
        state_ = NoteState::Pending;
        nextTickIndex_ = 0;
        started_ = false;
        held_ = false;
        heldPhysicalKeys_.clear();
    }

    NoteState HoldInputRule::State() const noexcept
    {
        return state_;
    }

    bool HoldInputRule::CanAccept(
        const NoteRuleContext&,
        const RhythmInputEvent& input,
        const JudgementResult& judgement) const noexcept
    {
        if (IsTerminal(state_) || input.action != requiredAction_ ||
            input.time > endTime_)
        {
            return false;
        }
        if (!started_)
        {
            return input.edge == InputEdge::Pressed &&
                IsAtLeastAsAccurateAs(
                    judgement.grade,
                    startMaximumGrade_);
        }
        return true;
    }

    void HoldInputRule::ProcessInput(
        const NoteRuleContext& context,
        const RhythmInputEvent& input,
        const JudgementResult& judgement,
        NoteProcessResult& output)
    {
        if (!IsTerminal(state_))
        {
            // Preserve the held state before this timestamp. A press at a tick
            // accepts it; a release at that timestamp does not.
            AdvanceTicks(context, input.time - RhythmDuration{1}, output);
        }
        if (!CanAccept(context, input, judgement))
        {
            output.events.push_back(MakeEvent(
                context,
                NoteEventType::InputRejected,
                state_,
                state_,
                judgement,
                input.time));
            return;
        }

        const NoteState before = state_;
        if (input.edge == InputEdge::Pressed)
        {
            heldPhysicalKeys_.insert(input.physicalKey);
            held_ = true;
            state_ = NoteState::Holding;
            if (!started_)
            {
                started_ = true;
                output.events.push_back(MakeEvent(
                    context,
                    NoteEventType::HitAccepted,
                    before,
                    state_,
                    judgement,
                    input.time));
                output.events.push_back(MakeEvent(
                    context,
                    NoteEventType::HoldStarted,
                    before,
                    state_,
                    judgement,
                    input.time));
            }
            AdvanceTicks(context, input.time, output);
            return;
        }

        heldPhysicalKeys_.erase(input.physicalKey);
        held_ = !heldPhysicalKeys_.empty();
        state_ = held_ ? NoteState::Holding : NoteState::Active;
        output.events.push_back(MakeEvent(
            context,
            NoteEventType::HoldReleased,
            before,
            state_,
            judgement,
            input.time));
        AdvanceTicks(context, input.time, output);
    }

    void HoldInputRule::Update(
        const NoteRuleContext& context,
        const NoteUpdateContext& update,
        NoteProcessResult& output)
    {
        if (IsTerminal(state_))
        {
            return;
        }
        if (started_)
        {
            held_ = update.IsHeld(requiredAction_);
            state_ = held_ ? NoteState::Holding : NoteState::Active;
        }

        AdvanceTicks(context, update.time, output);

        if (update.time < endTime_)
        {
            return;
        }
        const NoteState before = state_;
        state_ = started_ ? NoteState::Completed : NoteState::Missed;
        output.events.push_back(MakeEvent(
            context,
            state_ == NoteState::Completed
                ? NoteEventType::Completed
                : NoteEventType::Missed,
            before,
            state_,
            state_ == NoteState::Completed
                ? JudgementResult{JudgementGrade::Unjudged, {}, 1.0}
                : MissJudgement(context, update.time),
            endTime_));
    }

    void HoldInputRule::MarkMissed(
        const NoteRuleContext& context,
        const RhythmTime time,
        NoteProcessResult& output)
    {
        if (IsTerminal(state_))
        {
            return;
        }
        const NoteState before = state_;
        state_ = NoteState::Missed;
        output.events.push_back(MakeEvent(
            context,
            NoteEventType::Missed,
            before,
            state_,
            MissJudgement(context, time),
            time));
    }

    void HoldInputRule::AdvanceTicks(const NoteRuleContext& context,
        const RhythmTime time, NoteProcessResult& output)
    {
        while (nextTickIndex_ < tickTimes_.size() &&
            tickTimes_[nextTickIndex_] <= time)
        {
            const std::size_t index = nextTickIndex_++;
            output.events.push_back(MakeEvent(context,
                held_ ? NoteEventType::TickAccepted : NoteEventType::TickMissed,
                state_, state_,
                {JudgementGrade::Unjudged, {}, held_ ? 1.0 : 0.0},
                tickTimes_[index], 0, index));
        }
    }

    RhythmTime HoldInputRule::ExpireTime(
        const NoteRuleContext&) const noexcept
    {
        return endTime_;
    }

    DrumRollInputRule::DrumRollInputRule(
        std::vector<NoteAction> acceptedActions,
        const RhythmTime endTime,
        const std::size_t requiredHitCount)
        : acceptedActions_(std::move(acceptedActions)),
          endTime_(endTime),
          requiredHitCount_(std::max<std::size_t>(requiredHitCount, 1))
    {
        if (acceptedActions_.empty())
        {
            throw std::invalid_argument(
                "A drum-roll rule needs at least one input action.");
        }
    }

    void DrumRollInputRule::Reset() noexcept
    {
        state_ = NoteState::Active;
        tickCount_ = 0;
    }

    NoteState DrumRollInputRule::State() const noexcept
    {
        return state_;
    }

    bool DrumRollInputRule::CanAccept(
        const NoteRuleContext& context,
        const RhythmInputEvent& input,
        const JudgementResult&) const noexcept
    {
        return !IsTerminal(state_) &&
            input.edge == InputEdge::Pressed &&
            input.time >= context.noteTime &&
            input.time <= endTime_ &&
            std::ranges::find(acceptedActions_, input.action) !=
                acceptedActions_.end();
    }

    void DrumRollInputRule::ProcessInput(
        const NoteRuleContext& context,
        const RhythmInputEvent& input,
        const JudgementResult& judgement,
        NoteProcessResult& output)
    {
        if (!CanAccept(context, input, judgement))
        {
            output.events.push_back(MakeEvent(
                context,
                NoteEventType::InputRejected,
                state_,
                state_,
                judgement,
                input.time));
            return;
        }
        output.events.push_back(MakeEvent(
            context,
            NoteEventType::TickAccepted,
            state_,
            state_,
            {JudgementGrade::Unjudged, {}, 1.0},
            input.time,
            0,
            tickCount_++));
    }

    void DrumRollInputRule::Update(
        const NoteRuleContext& context,
        const NoteUpdateContext& update,
        NoteProcessResult& output)
    {
        if (IsTerminal(state_) || update.time < endTime_)
        {
            return;
        }
        const NoteState before = state_;
        state_ = tickCount_ >= requiredHitCount_ ? NoteState::Completed : NoteState::Missed;
        output.events.push_back(MakeEvent(
            context,
            state_ == NoteState::Completed ? NoteEventType::Completed : NoteEventType::Missed,
            before,
            state_,
            {JudgementGrade::Unjudged, {}, 1.0},
            endTime_,
            tickCount_));
    }

    void DrumRollInputRule::MarkMissed(
        const NoteRuleContext& context,
        const RhythmTime time,
        NoteProcessResult& output)
    {
        if (IsTerminal(state_))
        {
            return;
        }
        const NoteState before = state_;
        state_ = NoteState::Missed;
        output.events.push_back(MakeEvent(
            context,
            NoteEventType::Missed,
            before,
            state_,
            MissJudgement(context, time),
            time,
            tickCount_));
    }

    RhythmTime DrumRollInputRule::ExpireTime(
        const NoteRuleContext&) const noexcept
    {
        return endTime_;
    }

    TimedSequenceInputRule::TimedSequenceInputRule(
        std::vector<NoteAction> repeatingSequence,
        const std::size_t requiredHitCount,
        const RhythmTime endTime)
        : repeatingSequence_(std::move(repeatingSequence)),
          requiredHitCount_(std::max<std::size_t>(requiredHitCount, 1)),
          endTime_(endTime)
    {
        if (repeatingSequence_.empty())
        {
            throw std::invalid_argument(
                "A timed sequence requires at least one input action.");
        }
    }

    void TimedSequenceInputRule::Reset() noexcept
    {
        state_ = NoteState::Active;
        acceptedHitCount_ = 0;
    }

    NoteState TimedSequenceInputRule::State() const noexcept
    {
        return state_;
    }

    bool TimedSequenceInputRule::CanAccept(
        const NoteRuleContext& context,
        const RhythmInputEvent& input,
        const JudgementResult&) const noexcept
    {
        return !IsTerminal(state_) &&
            input.edge == InputEdge::Pressed &&
            input.time >= context.noteTime &&
            input.time <= endTime_ &&
            input.action == repeatingSequence_[
                acceptedHitCount_ % repeatingSequence_.size()];
    }

    void TimedSequenceInputRule::ProcessInput(
        const NoteRuleContext& context,
        const RhythmInputEvent& input,
        const JudgementResult& judgement,
        NoteProcessResult& output)
    {
        if (!CanAccept(context, input, judgement))
        {
            output.events.push_back(MakeEvent(
                context,
                NoteEventType::InputRejected,
                state_,
                state_,
                judgement,
                input.time,
                acceptedHitCount_));
            return;
        }

        const NoteState before = state_;
        const std::size_t acceptedIndex = acceptedHitCount_++;
        state_ = acceptedHitCount_ >= requiredHitCount_
            ? NoteState::Completed
            : NoteState::AwaitingAdditionalInput;
        const JudgementResult accepted{
            JudgementGrade::Unjudged,
            {},
            1.0};
        output.events.push_back(MakeEvent(
            context,
            NoteEventType::HitAccepted,
            before,
            state_,
            accepted,
            input.time,
            acceptedIndex));
        output.events.push_back(MakeEvent(
            context,
            state_ == NoteState::Completed
                ? NoteEventType::Completed
                : NoteEventType::StageAdvanced,
            before,
            state_,
            accepted,
            input.time,
            acceptedIndex));
    }

    void TimedSequenceInputRule::Update(
        const NoteRuleContext& context,
        const NoteUpdateContext& update,
        NoteProcessResult& output)
    {
        if (IsTerminal(state_) || update.time < endTime_)
        {
            return;
        }
        MarkMissed(context, endTime_, output);
    }

    void TimedSequenceInputRule::MarkMissed(
        const NoteRuleContext& context,
        const RhythmTime time,
        NoteProcessResult& output)
    {
        if (IsTerminal(state_))
        {
            return;
        }
        const NoteState before = state_;
        state_ = NoteState::Missed;
        output.events.push_back(MakeEvent(
            context,
            NoteEventType::Missed,
            before,
            state_,
            MissJudgement(context, time),
            time,
            acceptedHitCount_));
    }

    RhythmTime TimedSequenceInputRule::ExpireTime(
        const NoteRuleContext&) const noexcept
    {
        return endTime_;
    }

    std::optional<NoteProgress>
    TimedSequenceInputRule::Progress() const noexcept
    {
        return NoteProgress{acceptedHitCount_, requiredHitCount_};
    }

    TickRollInputRule::TickRollInputRule(
        std::vector<NoteAction> acceptedActions,
        const RhythmTime endTime,
        std::vector<RhythmTime> tickTimes,
        const JudgementGrade maximumGrade)
        : acceptedActions_(std::move(acceptedActions)),
          endTime_(endTime),
          tickTimes_(std::move(tickTimes)),
          maximumGrade_(maximumGrade)
    {
        if (acceptedActions_.empty())
        {
            throw std::invalid_argument(
                "A tick roll requires at least one input action.");
        }
        std::ranges::sort(tickTimes_);
    }

    void TickRollInputRule::Reset() noexcept
    {
        state_ = NoteState::Active;
        nextTickIndex_ = 0;
    }

    NoteState TickRollInputRule::State() const noexcept
    {
        return state_;
    }

    bool TickRollInputRule::CanAccept(
        const NoteRuleContext& context,
        const RhythmInputEvent& input,
        const JudgementResult&) const noexcept
    {
        if (IsTerminal(state_) || input.edge != InputEdge::Pressed ||
            input.time > endTime_ ||
            std::ranges::find(acceptedActions_, input.action) ==
                acceptedActions_.end())
        {
            return false;
        }

        const RhythmDuration halfWindow =
            context.judgementProfile.HalfWindow(maximumGrade_);
        std::size_t candidate = nextTickIndex_;
        while (candidate < tickTimes_.size() &&
            input.time > tickTimes_[candidate] + halfWindow)
        {
            ++candidate;
        }
        return candidate < tickTimes_.size() &&
            context.judgementProfile.IsWithin(
                maximumGrade_,
                tickTimes_[candidate],
                input.time);
    }

    void TickRollInputRule::ProcessInput(
        const NoteRuleContext& context,
        const RhythmInputEvent& input,
        const JudgementResult& judgement,
        NoteProcessResult& output)
    {
        AppendExpiredTicks(context, input.time, output);
        if (!CanAccept(context, input, judgement))
        {
            output.events.push_back(MakeEvent(
                context,
                NoteEventType::InputRejected,
                state_,
                state_,
                {JudgementGrade::Unjudged, {}, 0.0},
                input.time,
                0,
                nextTickIndex_));
            return;
        }

        const std::size_t acceptedIndex = nextTickIndex_++;
        output.events.push_back(MakeEvent(
            context,
            NoteEventType::TickAccepted,
            state_,
            state_,
            {JudgementGrade::Unjudged, input.time - tickTimes_[acceptedIndex], 1.0},
            input.time,
            0,
            acceptedIndex));
    }

    void TickRollInputRule::Update(
        const NoteRuleContext& context,
        const NoteUpdateContext& update,
        NoteProcessResult& output)
    {
        if (IsTerminal(state_))
        {
            return;
        }
        AppendExpiredTicks(context, update.time, output);
        if (update.time < endTime_)
        {
            return;
        }

        while (nextTickIndex_ < tickTimes_.size())
        {
            const std::size_t missedIndex = nextTickIndex_++;
            output.events.push_back(MakeEvent(
                context,
                NoteEventType::TickMissed,
                state_,
                state_,
                MissJudgement(context, tickTimes_[missedIndex]),
                tickTimes_[missedIndex],
                0,
                missedIndex));
        }
        const NoteState before = state_;
        state_ = NoteState::Completed;
        output.events.push_back(MakeEvent(
            context,
            NoteEventType::Completed,
            before,
            state_,
            {JudgementGrade::Unjudged, {}, 1.0},
            endTime_,
            0,
            nextTickIndex_));
    }

    void TickRollInputRule::MarkMissed(
        const NoteRuleContext& context,
        const RhythmTime time,
        NoteProcessResult& output)
    {
        if (IsTerminal(state_))
        {
            return;
        }
        const NoteState before = state_;
        state_ = NoteState::Missed;
        output.events.push_back(MakeEvent(
            context,
            NoteEventType::Missed,
            before,
            state_,
            MissJudgement(context, time),
            time,
            0,
            nextTickIndex_));
    }

    RhythmTime TickRollInputRule::ExpireTime(
        const NoteRuleContext&) const noexcept
    {
        return endTime_;
    }

    void TickRollInputRule::AppendExpiredTicks(
        const NoteRuleContext& context,
        const RhythmTime time,
        NoteProcessResult& output)
    {
        const RhythmDuration halfWindow =
            context.judgementProfile.HalfWindow(maximumGrade_);
        while (nextTickIndex_ < tickTimes_.size() &&
            time > tickTimes_[nextTickIndex_] + halfWindow)
        {
            const std::size_t missedIndex = nextTickIndex_++;
            output.events.push_back(MakeEvent(
                context,
                NoteEventType::TickMissed,
                state_,
                state_,
                MissJudgement(context, tickTimes_[missedIndex]),
                tickTimes_[missedIndex],
                0,
                missedIndex));
        }
    }
}
