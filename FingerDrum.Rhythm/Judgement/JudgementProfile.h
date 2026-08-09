#pragma once

#include "Common/RhythmTypes.h"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>

namespace finger_drum::rhythm
{
    enum class JudgementGrade : std::uint8_t
    {
        Max,
        Perfect,
        Great,
        Good,
        Bad,
        Miss,
        Unjudged,
    };

    struct JudgementBand
    {
        JudgementGrade grade{JudgementGrade::Miss};
        RhythmDuration halfWindow{};
        double boundaryScoreRate{};
    };

    struct JudgementResult
    {
        JudgementGrade grade{JudgementGrade::Miss};
        RhythmDuration signedError{};
        double scoreRate{};
    };

    class JudgementProfile final
    {
    public:
        static constexpr std::size_t BandCount = 5;
        static constexpr std::size_t DefaultLevel = 50;

        explicit JudgementProfile(
            std::string id = "default",
            std::size_t level = DefaultLevel);
        JudgementProfile(
            std::string id,
            std::size_t level,
            std::array<JudgementBand, BandCount> level50Bands);

        [[nodiscard]] std::string_view Id() const noexcept;
        [[nodiscard]] std::size_t Level() const noexcept;
        void SetLevel(std::size_t level);
        [[nodiscard]] const std::array<JudgementBand, BandCount>&
            Bands() const noexcept;
        [[nodiscard]] RhythmDuration HalfWindow(
            JudgementGrade grade) const noexcept;
        [[nodiscard]] JudgementResult Evaluate(
            RhythmTime noteTime,
            RhythmTime inputTime) const noexcept;
        [[nodiscard]] bool IsWithin(
            JudgementGrade outerGrade,
            RhythmTime noteTime,
            RhythmTime inputTime) const noexcept;

        [[nodiscard]] static std::array<JudgementBand, BandCount>
            DefaultLevel50Bands() noexcept;

    private:
        void RebuildScaledBands();

        std::string id_;
        std::size_t level_{DefaultLevel};
        std::array<JudgementBand, BandCount> level50Bands_{};
        std::array<JudgementBand, BandCount> scaledBands_{};
    };

    [[nodiscard]] bool IsAtLeastAsAccurateAs(
        JudgementGrade grade,
        JudgementGrade outerGrade) noexcept;
}
