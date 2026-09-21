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
    };

    struct CompiledEffectCommand
    {
        EffectCommand command;
        rhythm::RhythmTime timing{};
        rhythm::RhythmDuration duration{};
    };
} // namespace finger_drum::chart
