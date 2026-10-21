#pragma once
#include "Model/Submodules/ChartTypes.h"
#include "Model/Submodules/PatternDocument.h"
#include "Model/Submodules/EffectDocument.h"

struct CompiledPatternNote
{
    PatternNote note;
    RhythmTime timing{};
    double scrollMultiplier{1};
    // Source endpoints are retained so each mode can decide how to pair longs.
    bool positionActive{true};
};

struct CompiledEffectCommand
{
    EffectCommand command;
    RhythmTime timing{};
    RhythmDuration duration{};
};
