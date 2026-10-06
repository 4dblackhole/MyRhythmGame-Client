#include "Judgement/AccuracyRange.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace finger_drum::rhythm
{
namespace
{
[[nodiscard]] RhythmDuration Milliseconds(const double value) noexcept
{
    return RhythmDuration{static_cast<RhythmDuration::rep>(std::llround(value * 1000.0))};
}

[[nodiscard]] std::size_t GradeIndex(const JudgementGrade grade) noexcept
{
    return static_cast<std::size_t>(grade);
}
} // namespace

AccuracyRange::AccuracyRange(std::string id, const std::size_t level)
    : AccuracyRange(std::move(id), level, DefaultLevel50Bands())
{
}

AccuracyRange::AccuracyRange(std::string id, const std::size_t level, std::array<JudgementBand, BandCount> level50Bands,
                             const RhythmDuration extraHalfWindow)
    : extraHalfWindow_(extraHalfWindow), id_(std::move(id)), level_(std::max<std::size_t>(level, 1)),
      level50Bands_(std::move(level50Bands))
{
    if (id_.empty())
    {
        throw std::invalid_argument("A judgement profile ID cannot be empty.");
    }
    if (extraHalfWindow_ < RhythmDuration::zero())
        throw std::invalid_argument("AccuracyRange padding cannot be negative.");
    for (std::size_t index = 1; index < level50Bands_.size(); ++index)
    {
        if (level50Bands_[index].halfWindow <= level50Bands_[index - 1].halfWindow)
        {
            throw std::invalid_argument("Judgement windows must be strictly increasing.");
        }
    }
    RebuildScaledBands();
}

std::string_view AccuracyRange::Id() const noexcept
{
    return id_;
}

std::size_t AccuracyRange::Level() const noexcept
{
    return level_;
}

void AccuracyRange::SetLevel(const std::size_t level)
{
    level_ = std::max<std::size_t>(level, 1);
    RebuildScaledBands();
}

const std::array<JudgementBand, AccuracyRange::BandCount> &AccuracyRange::Bands() const noexcept
{
    return scaledBands_;
}

RhythmDuration AccuracyRange::HalfWindow(const JudgementGrade grade) const noexcept
{
    const std::size_t index = GradeIndex(grade);
    return index < scaledBands_.size() ? scaledBands_[index].halfWindow : RhythmDuration::zero();
}

JudgementResult AccuracyRange::Evaluate(const RhythmTime noteTime, const RhythmTime inputTime) const noexcept
{
    const RhythmDuration error = inputTime - noteTime;
    const RhythmDuration absoluteError = error >= RhythmDuration::zero() ? error : -error;

    for (std::size_t index = 0; index < scaledBands_.size(); ++index)
    {
        const JudgementBand &band = scaledBands_[index];
        if (absoluteError > band.halfWindow)
        {
            continue;
        }

        if (index == 0)
        {
            return {band.grade, error, band.boundaryScoreRate};
        }

        const JudgementBand &previous = scaledBands_[index - 1];
        const double range = static_cast<double>((band.halfWindow - previous.halfWindow).count());
        const double position = static_cast<double>((absoluteError - previous.halfWindow).count());
        const double amount = range > 0.0 ? std::clamp(position / range, 0.0, 1.0) : 1.0;
        return {band.grade, error, std::lerp(previous.boundaryScoreRate, band.boundaryScoreRate, amount)};
    }
    return {JudgementGrade::Miss, error, 0.0};
}

bool AccuracyRange::IsWithin(const JudgementGrade outerGrade, const RhythmTime noteTime,
                             const RhythmTime inputTime) const noexcept
{
    const JudgementResult result = Evaluate(noteTime, inputTime);
    return IsAtLeastAsAccurateAs(result.grade, outerGrade);
}

std::array<JudgementBand, AccuracyRange::BandCount> AccuracyRange::DefaultLevel50Bands() noexcept
{
    return {{
        {JudgementGrade::Max, Milliseconds(4.5), 1.0},
        {JudgementGrade::Perfect, Milliseconds(9.5), 0.99},
        {JudgementGrade::Great, Milliseconds(23.5), 0.90},
        {JudgementGrade::Good, Milliseconds(54.5), 0.0},
        {JudgementGrade::Bad, Milliseconds(90.0), 0.0},
    }};
}

void AccuracyRange::RebuildScaledBands()
{
    const long double scale = static_cast<long double>(DefaultLevel) / static_cast<long double>(level_);
    for (std::size_t index = 0; index < scaledBands_.size(); ++index)
    {
        scaledBands_[index] = level50Bands_[index];
        scaledBands_[index].halfWindow =
            RhythmDuration{static_cast<RhythmDuration::rep>(
                std::llround(static_cast<long double>(level50Bands_[index].halfWindow.count()) * scale))} +
            extraHalfWindow_;
    }
}

bool IsAtLeastAsAccurateAs(const JudgementGrade grade, const JudgementGrade outerGrade) noexcept
{
    if (grade == JudgementGrade::Unjudged || outerGrade == JudgementGrade::Unjudged)
    {
        return false;
    }
    return GradeIndex(grade) <= GradeIndex(outerGrade);
}
} // namespace finger_drum::rhythm
