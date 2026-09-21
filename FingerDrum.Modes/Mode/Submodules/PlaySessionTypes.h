#pragma once

#include "Model/ChartDocument.h"
#include "Mode/Submodules/NoteVisualKind.h"
#include "Parsing/ChartParser.h"
#include "Scroll/ScrollGear.h"
#include "Timing/MusicalTimeline.h"

#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace finger_drum::mode
{
    struct NotePresentationInfo
    {
        NoteVisualKind visualKind{NoteVisualKind::Unknown};
        rhythm::RhythmTime endTime{};
        bool hasEndTime{};
        std::vector<rhythm::RhythmTime> tickTimes;
        double scrollMultiplier{1};
    };

    struct AutomationValue
    {
        chart::EffectCommandType type{chart::EffectCommandType::Custom};
        std::string target;
        double value{};
    };

    struct TimedSoundOverride
    {
        rhythm::RhythmTime time{};
        rhythm::SoundId sound;
    };
} // namespace finger_drum::mode
