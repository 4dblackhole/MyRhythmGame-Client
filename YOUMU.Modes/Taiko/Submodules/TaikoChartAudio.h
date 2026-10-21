#pragma once
#include "TaikoNoteDefinition.h"
#include "TaikoSoundIds.h"
#include "Timing/MusicalTimeline.h"

inline std::string ChartSoundId(const std::string &index)
{
    return "Chart.HitSound." + index;
}
inline std::string InputSound(TaikoAction action)
{
    return action == TaikoAction::Don ? DonHit : KatHit;
}
inline std::string NoteSound(const PatternNote &note, TaikoAction action, bool big = false)
{
    if (!note.hitSound.empty())
        return note.hitSound;
    if (big)
        return action == TaikoAction::Don ? BigDonFirstHit : BigKatFirstHit;
    return InputSound(action);
}
struct TimedSoundBinding
{
    const char *sound;
    int keyType;
};
inline constexpr TimedSoundBinding TimedSoundBindings[]{
    {DonHit, 1},       {BigDonFirstHit, 1}, {DonFreeInput, 1},
    {LongNoteTick, 1}, {KatHit, 2},         {BigKatFirstHit, 2},
    {KatFreeInput, 2}};

// Preview uses play's semantic IDs and compiled-time overrides. Namespaced
// explicit chart assignments take precedence over timed defaults.
inline std::string ResolveSound(std::string sound, RhythmTime time, const EffectDocument &effects,
                                const MusicalTimeline &timeline)
{
    int key = 0;
    for (const auto &binding : TimedSoundBindings)
        if (sound == binding.sound)
            key = binding.keyType;
    if (key == 0)
        return sound;
    std::optional<RhythmTime> lastTime;
    for (const auto &change : effects.hitSoundChanges)
    {
        if (change.keyType != key || !timeline.IsPositionActive(change.position))
            continue;
        const auto changeTime = timeline.Compile(change.position);
        if (changeTime <= time && (!lastTime || changeTime >= *lastTime))
        {
            lastTime = changeTime;
            sound = ChartSoundId(change.soundIndex);
        }
    }
    return sound;
}
