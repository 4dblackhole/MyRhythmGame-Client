#pragma once
#include "Timing/MusicalTimeline.h"

namespace finger_drum::mode
{
    struct TaikoPreviewCue
    {
        rhythm::RhythmTime time;
        rhythm::SoundId sound;
    };
    // Ideal head/tick sounds only; free input and balloon completion times
    // depend on the player and cannot be predicted from chart data.
    std::vector<TaikoPreviewCue> BuildTaikoAudioPreview(const chart::PatternDocument &, const chart::EffectDocument &,
                                                        const chart::MusicalTimeline &,
                                                        const std::vector<chart::CompiledPatternNote> &);
} // namespace finger_drum::mode
