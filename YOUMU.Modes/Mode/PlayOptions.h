#pragma once
#include "Judgement/JudgeLevel.h"
#include <cmath>
#include <cstdint>
#include <optional>
#include <stdexcept>

// Value snapshot for one attempt. No option mutates a source chart.
struct PlayOptions
{
    enum class ScrollUnit
    {
        Multiplier,
        Bpm
    };
    ScrollUnit scrollUnit{ScrollUnit::Multiplier};
    double scrollSpeed{1.0};
    bool ignoreScrollEffects{};
    bool metronome{};
    bool mirror{};
    bool random{};
    std::uint64_t randomSeed{};
    double randomAttemptProbability{1.0};
    double accelerate{1.0};
    bool preserveMusicPitch{true};
    bool preserveHitSoundPitch{true};
    std::optional<JudgeLevel> judgeLevel;

    void Validate() const
    {
        if ((scrollUnit != ScrollUnit::Multiplier && scrollUnit != ScrollUnit::Bpm) || !std::isfinite(scrollSpeed) ||
            scrollSpeed <= 0 || !std::isfinite(randomAttemptProbability) || randomAttemptProbability < 0 ||
            randomAttemptProbability > 1 || !std::isfinite(accelerate) || accelerate < 0.5 || accelerate > 2.0)
            throw std::invalid_argument(
                "Invalid play options: positive scroll speed, probability 0..1, Accelerate 0.5..2 required.");
    }
    double ScrollMultiplier(double baseBpm) const
    {
        Validate();
        const double value = scrollUnit == ScrollUnit::Bpm ? scrollSpeed / baseBpm : scrollSpeed;
        if (!std::isfinite(value) || value <= 0)
            throw std::invalid_argument("Scroll speed cannot be represented for this chart.");
        return value;
    }
};
