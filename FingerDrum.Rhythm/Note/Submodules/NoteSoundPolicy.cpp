#include "Note/Submodules/NoteSoundPolicy.h"
#include "Note/Submodules/RuleHelpers.h"
#include <algorithm>
#include <format>
#include <iterator>
#include <numeric>
#include <stdexcept>
#include <utility>

namespace finger_drum::rhythm
{
    using namespace detail;

    MappedNoteSoundPolicy &MappedNoteSoundPolicy::Bind(SoundBinding binding)
    {
        bindings_.push_back(std::move(binding));
        return *this;
    }

    void MappedNoteSoundPolicy::AppendAudioCues(const NoteEvent &event,
                                                std::vector<AudioCueRequest> &output) const
    {
        for (const SoundBinding &binding : bindings_)
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

    bool MappedNoteSoundPolicy::Matches(const SoundBinding &binding,
                                        const NoteEvent &event) noexcept
    {
        if (binding.eventType != event.type ||
            (binding.inputAction.has_value() && binding.inputAction != event.inputAction) ||
            (binding.stateBefore.has_value() && *binding.stateBefore != event.stateBefore) ||
            (binding.stateAfter.has_value() && *binding.stateAfter != event.stateAfter) ||
            (binding.hitIndex.has_value() && *binding.hitIndex != event.hitIndex) ||
            (binding.tickIndex.has_value() && *binding.tickIndex != event.tickIndex))
        {
            return false;
        }
        return !binding.maximumGrade.has_value() ||
               IsAtLeastAsAccurateAs(event.judgement.grade, *binding.maximumGrade);
    }
} // namespace finger_drum::rhythm
