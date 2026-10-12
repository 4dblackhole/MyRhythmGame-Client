#pragma once
#include "Model/Submodules/ChartTypes.h"
#include "Model/Submodules/PatternDocument.h"
#include "Model/Submodules/EffectDocument.h"

namespace finger_drum::chart
{
    struct CompiledPatternNote
    {
        PatternNote note;
        rhythm::RhythmTime timing{};
        double scrollMultiplier{1};
        // Source endpoints are retained so each mode can decide how to pair longs.
        bool positionActive{true};
    };

    struct CompiledEffectCommand
    {
        EffectCommand command;
        rhythm::RhythmTime timing{};
        rhythm::RhythmDuration duration{};
    };
} // namespace finger_drum::chart
