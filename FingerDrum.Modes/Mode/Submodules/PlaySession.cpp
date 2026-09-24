#include "Mode/PlayGameMode.h"

#include <algorithm>
#include <cmath>
#include <set>
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
        inputMapping_ = std::move(mapping);
    }

    void PlaySession::SetFreeInputCue(const rhythm::NoteAction action, rhythm::AudioCueRequest cue)
    {
        freeInputCues_.insert_or_assign(action, std::move(cue));
    }

    void PlaySession::SetEffects(std::vector<chart::CompiledEffectCommand> effects)
    {
        effects_ = std::move(effects);
    }

    void PlaySession::SetMeasureLines(std::vector<rhythm::RhythmTime> measureLines)
    {
        measureLines_ = std::move(measureLines);
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

    const std::vector<rhythm::RhythmTime> &PlaySession::MeasureLines() const noexcept
    {
        return measureLines_;
    }

    void PlaySession::SetNotePresentation(const rhythm::NoteId noteId,
                                          NotePresentationInfo presentation)
    {
        notePresentation_.insert_or_assign(noteId, std::move(presentation));
    }

    const NotePresentationInfo *PlaySession::FindNotePresentation(
        const rhythm::NoteId noteId) const noexcept
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
        result = gear_.Lanes().front()->ProcessInput(input);
        AccumulateAccuracy(result);

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
        std::set<rhythm::NoteAction> uniqueActions;
        for (const rhythm::PhysicalKey key : heldPhysicalKeys)
        {
            const auto mapping = inputMapping_.find(key);
            if (mapping != inputMapping_.end())
            {
                uniqueActions.insert(mapping->second);
            }
        }
        const std::vector<rhythm::NoteAction> heldActions(uniqueActions.begin(),
                                                          uniqueActions.end());
        rhythm::NoteProcessResult result = gear_.Update(time, heldActions);
        AccumulateAccuracy(result);
        ResolveSoundOverrides(result);
        return result;
    }

    std::vector<AutomationValue> PlaySession::EvaluateAutomation(
        const rhythm::RhythmTime time) const
    {
        std::vector<AutomationValue> values;
        for (const chart::CompiledEffectCommand &command : effects_)
        {
            if (time < command.timing)
            {
                continue;
            }
            values.push_back(AutomationValue{command.command.type, command.command.target,
                                             Interpolate(command, time)});
        }
        return values;
    }

    void PlaySession::Reset() noexcept
    {
        gear_.Reset();
        accuracySum_ = 0.0;
        finalizedNoteCount_ = 0;
        lastNoteAccuracy_.reset();
    }

    void PlaySession::AccumulateAccuracy(const rhythm::NoteProcessResult &result)
    {
        for (const rhythm::NoteAccuracy &accuracy : result.finalizedAccuracies)
        {
            accuracySum_ += accuracy.ScoreRate();
            ++finalizedNoteCount_;
            lastNoteAccuracy_ = accuracy;
        }
    }

    std::optional<double> PlaySession::AccuracyRate() const noexcept
    {
        return finalizedNoteCount_ == 0 ? std::nullopt
                                        : std::optional<double>{accuracySum_ / finalizedNoteCount_};
    }

    std::size_t PlaySession::FinalizedNoteCount() const noexcept
    {
        return finalizedNoteCount_;
    }

    const std::optional<rhythm::NoteAccuracy> &PlaySession::LastNoteAccuracy() const noexcept
    {
        return lastNoteAccuracy_;
    }

    double PlaySession::Interpolate(const chart::CompiledEffectCommand &command,
                                    const rhythm::RhythmTime time) noexcept
    {
        if (command.duration <= rhythm::RhythmDuration::zero())
        {
            return command.command.endValue;
        }
        double amount = std::clamp(static_cast<double>((time - command.timing).count()) /
                                       static_cast<double>(command.duration.count()),
                                   0.0, 1.0);
        switch (command.command.curve)
        {
        case chart::AutomationCurve::Step:
            amount = amount >= 1.0 ? 1.0 : 0.0;
            break;
        case chart::AutomationCurve::Smoothstep:
            amount = amount * amount * (3.0 - 2.0 * amount);
            break;
        case chart::AutomationCurve::Exponential:
            amount *= amount;
            break;
        case chart::AutomationCurve::Linear:
            break;
        }
        return std::lerp(command.command.beginValue, command.command.endValue, amount);
    }

} // namespace finger_drum::mode
