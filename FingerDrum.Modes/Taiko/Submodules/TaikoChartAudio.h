#pragma once
#include "TaikoNoteDefinition.h"
#include "TaikoSoundIds.h"
#include "Timing/MusicalTimeline.h"

namespace finger_drum::mode::taiko_audio
{
    inline std::string ChartSoundId(const std::string &index)
    {
        return "Chart.HitSound." + index;
    }
    inline std::string InputSound(TaikoAction action)
    {
        return action == TaikoAction::Don ? taiko_sound::DonHit : taiko_sound::KatHit;
    }
    inline std::string NoteSound(const chart::PatternNote &note, TaikoAction action, bool big = false)
    {
        if (!note.hitSound.empty())
            return note.hitSound;
        if (big)
            return action == TaikoAction::Don ? taiko_sound::BigDonFirstHit : taiko_sound::BigKatFirstHit;
        return InputSound(action);
    }
    struct TimedSoundBinding
    {
        const char *sound;
        int keyType;
    };
    inline constexpr TimedSoundBinding TimedSoundBindings[]{
        {taiko_sound::DonHit, 1},       {taiko_sound::BigDonFirstHit, 1}, {taiko_sound::DonFreeInput, 1},
        {taiko_sound::LongNoteTick, 1}, {taiko_sound::KatHit, 2},         {taiko_sound::BigKatFirstHit, 2},
        {taiko_sound::KatFreeInput, 2}};

    // Preview uses play's semantic IDs and compiled-time overrides. Namespaced
    // explicit chart assignments take precedence over timed defaults.
    inline std::string ResolveSound(std::string sound, rhythm::RhythmTime time, const chart::EffectDocument &effects,
                                    const chart::MusicalTimeline &timeline)
    {
        int key = 0;
        for (const auto &binding : TimedSoundBindings)
            if (sound == binding.sound)
                key = binding.keyType;
        if (key == 0)
            return sound;
        std::optional<rhythm::RhythmTime> lastTime;
        for (const auto &change : effects.hitSoundChanges)
        {
            if (change.keyType != key)
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
} // namespace finger_drum::mode::taiko_audio
