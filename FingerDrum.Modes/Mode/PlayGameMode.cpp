#include "Mode/PlayGameMode.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <utility>

namespace finger_drum::mode
{
    rhythm::ScrollGear& PlaySession::Gear() noexcept
    {
        return gear_;
    }

    const rhythm::ScrollGear& PlaySession::Gear() const noexcept
    {
        return gear_;
    }

    void PlaySession::SetInputMapping(
        std::map<rhythm::PhysicalKey, rhythm::NoteAction> mapping)
    {
        inputMapping_ = std::move(mapping);
    }

    void PlaySession::SetFreeInputCue(
        const rhythm::NoteAction action,
        rhythm::AudioCueRequest cue)
    {
        freeInputCues_.insert_or_assign(action, std::move(cue));
    }

    void PlaySession::SetEffects(
        std::vector<chart::CompiledEffectCommand> effects)
    {
        effects_ = std::move(effects);
    }

    rhythm::NoteProcessResult PlaySession::ProcessInput(
        const rhythm::PhysicalKey physicalKey,
        const rhythm::InputEdge edge,
        const rhythm::RhythmTime time)
    {
        rhythm::NoteProcessResult result;
        const auto mapping = inputMapping_.find(physicalKey);
        if (mapping == inputMapping_.end() || gear_.LaneCount() == 0)
        {
            return result;
        }

        const rhythm::RhythmInputEvent input{
            time,
            mapping->second,
            physicalKey,
            edge};
        result = gear_.Lanes().front()->ProcessInput(input);

        // Free-input feedback is deliberately separate from note-owned cues.
        // Therefore an early/out-of-range big note can play a normal Don/Kat
        // sound but never leaks the large-note sound reserved for its first
        // accepted Good-or-better transition.
        const bool noteAccepted = std::ranges::any_of(
            result.events,
            [](const rhythm::NoteEvent& event)
            {
                return event.type == rhythm::NoteEventType::HitAccepted ||
                    event.type == rhythm::NoteEventType::TickAccepted;
            });
        if (edge == rhythm::InputEdge::Pressed &&
            result.audioCues.empty() &&
            !noteAccepted)
        {
            const auto cue = freeInputCues_.find(mapping->second);
            if (cue != freeInputCues_.end())
            {
                rhythm::AudioCueRequest request = cue->second;
                request.timelineTime = time;
                result.audioCues.push_back(std::move(request));
            }
        }
        return result;
    }

    rhythm::NoteProcessResult PlaySession::Update(
        const rhythm::RhythmTime time,
        const std::span<const rhythm::PhysicalKey> heldPhysicalKeys)
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
        const std::vector<rhythm::NoteAction> heldActions(
            uniqueActions.begin(),
            uniqueActions.end());
        return gear_.Update(time, heldActions);
    }

    std::vector<AutomationValue> PlaySession::EvaluateAutomation(
        const rhythm::RhythmTime time) const
    {
        std::vector<AutomationValue> values;
        for (const chart::CompiledEffectCommand& command : effects_)
        {
            if (time < command.timing)
            {
                continue;
            }
            values.push_back(AutomationValue{
                command.command.type,
                command.command.target,
                Interpolate(command, time)});
        }
        return values;
    }

    void PlaySession::Reset() noexcept
    {
        gear_.Reset();
    }

    double PlaySession::Interpolate(
        const chart::CompiledEffectCommand& command,
        const rhythm::RhythmTime time) noexcept
    {
        if (command.duration <= rhythm::RhythmDuration::zero())
        {
            return command.command.endValue;
        }
        double amount = std::clamp(
            static_cast<double>((time - command.timing).count()) /
                static_cast<double>(command.duration.count()),
            0.0,
            1.0);
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
        return std::lerp(
            command.command.beginValue,
            command.command.endValue,
            amount);
    }

    bool ModeLoadResult::Succeeded() const noexcept
    {
        if (session == nullptr)
        {
            return false;
        }
        return std::ranges::none_of(
            diagnostics,
            [](const chart::Diagnostic& diagnostic)
            {
                return diagnostic.severity ==
                    chart::DiagnosticSeverity::Error;
            });
    }
}
