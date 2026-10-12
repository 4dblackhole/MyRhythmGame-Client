#include "Automation/EffectValidation.h"
#include "Automation/InterpolationExpression.h"
#include "Mode/PlayGameMode.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace finger_drum::mode
{
    rhythm::ScrollGear &PlaySession::Gear() noexcept
    {
        return gear_;
    }

    const rhythm::ScrollGear &PlaySession::Gear() const noexcept
    {
        return gear_;
    }

    void PlaySession::SetTimeline(chart::MusicalTimeline timeline)
    {
        timeline_.emplace(std::move(timeline));
    }

    const chart::MusicalTimeline &PlaySession::Timeline() const
    {
        if (!timeline_)
            throw std::logic_error("A play session has no musical timeline.");
        return *timeline_;
    }

    void PlaySession::SetInputMapping(std::map<rhythm::PhysicalKey, rhythm::NoteAction> mapping)
    {
        heldActions_.reserve(mapping.size());
        inputMapping_ = std::move(mapping);
    }

    void PlaySession::SetFreeInputCue(const rhythm::NoteAction action, rhythm::AudioCueRequest cue)
    {
        freeInputCues_.insert_or_assign(action, std::move(cue));
    }

    void PlaySession::SetEffects(std::vector<chart::CompiledEffectCommand> effects)
    {
        chart::ScrollAutomation scroll(effects);
        decltype(effectTracks_) tracks;
        for (std::size_t i = 0; i < effects.size(); ++i)
            tracks[{effects[i].command.type, effects[i].command.target}].push_back(i);
        for (auto &[key, indices] : tracks)
            std::ranges::stable_sort(indices, {}, [&effects](std::size_t i) { return effects[i].timing; });
        decltype(automationLastValues_) lastValues(tracks.size());
        automationValues_.reserve(tracks.size());
        automationIndices_.reserve(tracks.size());
        effects_ = std::move(effects);
        scroll_ = std::move(scroll);
        effectTracks_ = std::move(tracks);
        automationTime_.reset();
        automationValues_.clear();
        automationLastValues_.swap(lastValues);
        automationError_.clear();
    }

    void PlaySession::SetMeasureLines(std::vector<MeasureLinePresentationInfo> measureLines)
    {
        double minimum = 1;
        for (const auto &line : measureLines)
        {
            if (!std::isfinite(line.scrollMultiplier) || line.scrollMultiplier <= 0)
                throw std::invalid_argument("Measure-line scroll multiplier must be positive and finite.");
            minimum = std::min(minimum, line.scrollMultiplier);
        }
        std::ranges::stable_sort(measureLines, {}, &MeasureLinePresentationInfo::timing);
        measureLines_ = std::move(measureLines);
        minimumMeasureScrollMultiplier_ = minimum;
    }

    void PlaySession::SetHitSoundFiles(std::map<rhythm::SoundId, std::filesystem::path> files)
    {
        hitSoundFiles_ = std::move(files);
    }

    const std::map<rhythm::SoundId, std::filesystem::path> &PlaySession::HitSoundFiles()
        const noexcept
    {
        return hitSoundFiles_;
    }

    void PlaySession::SetSoundOverrides(rhythm::SoundId source,
                                        std::vector<TimedSoundOverride> changes)
    {
        std::ranges::stable_sort(changes, {}, &TimedSoundOverride::time);
        soundOverrides_.insert_or_assign(std::move(source), std::move(changes));
    }

    void PlaySession::ResolveSoundOverrides(rhythm::NoteProcessResult &result) const
    {
        // Query by each cue's timestamp, not render/update time. This also
        // handles held ticks emitted together and backward debug seeking.
        for (auto &cue : result.audioCues)
        {
            const auto found = soundOverrides_.find(cue.sound);
            if (found == soundOverrides_.end())
                continue;
            const auto &changes = found->second;
            const auto next =
                std::ranges::upper_bound(changes, cue.timelineTime, {}, &TimedSoundOverride::time);
            if (next != changes.begin())
                cue.sound = std::prev(next)->sound;
        }
    }

    const std::vector<MeasureLinePresentationInfo> &PlaySession::MeasureLines() const noexcept
    {
        return measureLines_;
    }

    void PlaySession::SetNotePresentation(const rhythm::NoteId noteId, NotePresentationInfo presentation)
    {
        // Tick rules process chronological indices, including duplicate times
        // and negative-delay maps. Rendering uses the same processed prefix.
        std::ranges::sort(presentation.tickTimes);
        notePresentation_.insert_or_assign(noteId, std::move(presentation));
    }

    const NotePresentationInfo *PlaySession::FindNotePresentation(const rhythm::NoteId noteId) const noexcept
    {
        const auto found = notePresentation_.find(noteId);
        return found == notePresentation_.end() ? nullptr : &found->second;
    }

    void PlaySession::SetNoteScrollMultiplier(rhythm::NoteId noteId, double multiplier)
    {
        if (!std::isfinite(multiplier) || multiplier <= 0)
            throw std::invalid_argument("Note scroll multiplier must be positive and finite.");
        minimumScrollMultiplier_ = std::min(minimumScrollMultiplier_, multiplier);
        if (auto found = notePresentation_.find(noteId); found != notePresentation_.end())
            found->second.scrollMultiplier = multiplier;
    }

    rhythm::NoteProcessResult PlaySession::ProcessInput(const rhythm::PhysicalKey physicalKey,
                                                        const rhythm::InputEdge edge,
                                                        const rhythm::RhythmTime time)
    {
        rhythm::NoteProcessResult result;
        const auto mapping = inputMapping_.find(physicalKey);
        if (mapping == inputMapping_.end() || gear_.LaneCount() == 0)
        {
            return result;
        }

        const rhythm::RhythmInputEvent input{time, mapping->second, physicalKey, edge};
        if (resultCollector_) resultCollector_->RecordInput(input);
        result = gear_.Lanes().front()->ProcessInput(input);
        AccumulateAccuracy(result, edge == rhythm::InputEdge::Pressed);

        // Free-input feedback is deliberately separate from note-owned cues.
        // Therefore an early/out-of-range big note can play a normal Don/Kat
        // sound but never leaks the large-note sound reserved for its first
        // accepted Good-or-better transition.
        const bool noteAccepted =
            std::ranges::any_of(result.events, [](const rhythm::NoteEvent &event) {
                return event.type == rhythm::NoteEventType::HitAccepted ||
                       event.type == rhythm::NoteEventType::TickAccepted;
            });
        if (edge == rhythm::InputEdge::Pressed && result.audioCues.empty() && !noteAccepted)
        {
            const auto cue = freeInputCues_.find(mapping->second);
            if (cue != freeInputCues_.end())
            {
                rhythm::AudioCueRequest request = cue->second;
                request.timelineTime = time;
                result.audioCues.push_back(std::move(request));
            }
        }
        ResolveSoundOverrides(result);
        return result;
    }

    rhythm::NoteProcessResult PlaySession::Update(
        const rhythm::RhythmTime time, const std::span<const rhythm::PhysicalKey> heldPhysicalKeys)
    {
        heldActions_.clear();
        // Iterate bindings so repeated physical keys cannot exceed the reserved buffer.
        for (const auto &[key, action] : inputMapping_)
        {
            if (std::ranges::find(heldPhysicalKeys, key) != heldPhysicalKeys.end())
                heldActions_.push_back(action);
        }
        std::ranges::sort(heldActions_);
        const auto duplicates = std::ranges::unique(heldActions_);
        heldActions_.erase(duplicates.begin(), duplicates.end());
        rhythm::NoteProcessResult result = gear_.Update(time, heldActions_);
        AccumulateAccuracy(result);
        ResolveSoundOverrides(result);
        return result;
    }

    const std::vector<AutomationValue> &PlaySession::EvaluateAutomation(
        const rhythm::RhythmTime time) const
    {
        if (automationTime_ == time)
            return automationValues_;
        automationTime_.reset();
        auto &selected = automationIndices_;
        selected.clear();
        std::size_t trackIndex = 0;
        for (const auto &[key, indices] : effectTracks_)
        {
            const auto next = std::ranges::upper_bound(indices, time, {},
                [this](std::size_t i) { return effects_[i].timing; });
            if (next != indices.begin())
                selected.emplace_back(*std::prev(next), trackIndex);
            ++trackIndex;
        }
        // Preserve authored order across targets that share the same output bus.
        std::ranges::sort(selected, {}, [](const auto &entry) { return entry.first; });
        automationValues_.resize(selected.size());
        for (std::size_t i = 0; i < selected.size(); ++i)
        {
            const auto &[effectIndex, track] = selected[i];
            const auto &command = effects_[effectIndex];
            auto &value = automationValues_[i];
            value.type = command.command.type;
            value.target = command.command.target;
            value.value = EvaluateTrack(effectIndex, track, time);
        }
        automationTime_ = time;
        return automationValues_;
    }

    double PlaySession::EvaluateTrack(std::size_t effectIndex, std::size_t trackIndex, rhythm::RhythmTime time) const
    {
        const auto &effect = effects_[effectIndex];
        auto &lastValue = automationLastValues_[trackIndex];
        // The validated From value anchors a track until its first successful
        // sample, including a first query that seeks directly into a failure.
        if (!lastValue)
            lastValue = effect.command.beginValue;
        try
        {
            const auto value = Interpolate(effect, time);
            chart::ValidateEffectValue(effect.command.type, value);
            lastValue = value;
            return value;
        }
        catch (const std::exception &error)
        {
            if (automationError_.empty())
            {
                const auto path = effect.command.source.file.generic_u8string();
                automationError_ = std::string(reinterpret_cast<const char *>(path.data()), path.size()) + ":" +
                                   std::to_string(effect.command.source.line) + " [" + effect.command.target + "] " +
                                   error.what();
            }
            return *lastValue;
        }
    }

    void PlaySession::Reset() noexcept
    {
        gear_.Reset();
        accuracySum_ = 0;
        if (resultCollector_) resultCollector_->Reset();
        finalizedNoteCount_ = 0;
        lastNoteAccuracy_.reset();
        automationTime_.reset();
        std::ranges::fill(automationLastValues_, std::nullopt);
        automationError_.clear();
    }

    bool PlaySession::IsKiaiActive(rhythm::RhythmTime time) const
    {
        for (const auto &value : EvaluateAutomation(time))
            if (value.type == chart::EffectCommandType::Kiai) return value.value >= .5;
        return false;
    }

    void PlaySession::AccumulateAccuracy(const rhythm::NoteProcessResult &result, bool keyPressed)
    {
        if (resultCollector_) resultCollector_->Consume(result, keyPressed);
        for (const rhythm::NoteAccuracy &accuracy : result.finalizedAccuracies)
        {
            accuracySum_ += accuracy.Score().Units();
            ++finalizedNoteCount_;
            lastNoteAccuracy_ = accuracy;
        }
    }

    std::optional<double> PlaySession::AccuracyRate() const noexcept
    {
        return finalizedNoteCount_ == 0 ? std::nullopt
            : std::optional<double>{rhythm::AccuracyScore::Average(accuracySum_, finalizedNoteCount_).Rate()};
    }

    std::size_t PlaySession::FinalizedNoteCount() const noexcept
    {
        return finalizedNoteCount_;
    }

    void PlaySession::SetResultCollector(std::unique_ptr<IPlayResultCollector> collector)
    {
        resultCollector_ = std::move(collector);
    }
    void PlaySession::SetResultInitialTime(rhythm::RhythmTime time) noexcept
    {
        if (resultCollector_) resultCollector_->SetInitialTime(time);
    }
    std::unique_ptr<IPlayResult> PlaySession::TakeResult(PlayEndReason reason, rhythm::RhythmTime time)
    {
        if (!resultCollector_) throw std::logic_error("The mode has no result collector.");
        return resultCollector_->TakeResult(reason, time);
    }

    const std::optional<rhythm::NoteAccuracy> &PlaySession::LastNoteAccuracy() const noexcept
    {
        return lastNoteAccuracy_;
    }

    double PlaySession::Interpolate(const chart::CompiledEffectCommand &command, rhythm::RhythmTime time)
    {
        if (command.duration <= rhythm::RhythmDuration::zero()) return command.command.endValue;
        return chart::EvaluateInterpolation(command.command,
            static_cast<double>((time - command.timing).count()) / static_cast<double>(command.duration.count()));
    }
} // namespace finger_drum::mode
