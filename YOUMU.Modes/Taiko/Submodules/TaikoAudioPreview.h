#pragma once
#include "Timing/MusicalTimeline.h"

struct TaikoPreviewCue
{
    RhythmTime time;
    SoundId sound;
};
// Ideal head/tick sounds only; free input and balloon completion times
// depend on the player and cannot be predicted from chart data.
std::vector<TaikoPreviewCue> BuildTaikoAudioPreview(const PatternDocument &, const EffectDocument &,
                                                    const MusicalTimeline &,
                                                    const std::vector<CompiledPatternNote> &);
