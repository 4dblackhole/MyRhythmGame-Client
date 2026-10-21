#pragma once

#include "Model/Submodules/CompiledChart.h"
#include "Mode/Submodules/NoteVisualKind.h"
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

struct NotePresentationInfo
{
    NoteVisualKind visualKind{NoteVisualKind::Unknown};
    RhythmTime endTime{};
    bool hasEndTime{};
    std::vector<RhythmTime> tickTimes;
    double scrollMultiplier{1};
};

struct AutomationValue
{
    EffectCommandType type{EffectCommandType::Custom};
    std::string target;
    double value{};
};

struct MeasureLinePresentationInfo
{
    RhythmTime timing{};
    double scrollMultiplier{1};
};

struct TimedSoundOverride
{
    RhythmTime time{};
    SoundId sound;
};
