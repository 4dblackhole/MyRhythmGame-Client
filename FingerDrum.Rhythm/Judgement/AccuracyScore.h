#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace finger_drum::rhythm
{
    // One unit is 0.000001 percentage points. Integer sums never drift while
    // accumulating a chart; conversion to a rate exists only at API/UI boundaries.
    class AccuracyScore final
    {
      public:
        static constexpr std::uint32_t MaximumUnits = 100'000'000;
        static AccuracyScore FromRate(double rate) noexcept
        {
            if (!std::isfinite(rate) || rate <= 0)
                return {};
            const double scaled = std::min(rate, 1.0) * MaximumUnits;
            // Recover a representable integer boundary lost by one floating-point
            // ULP (e.g. converting a previously quantized rate back to units).
            return FromUnits(static_cast<std::uint32_t>(
                std::floor(std::nextafter(scaled, std::numeric_limits<double>::infinity()))));
        }
        static constexpr AccuracyScore FromUnits(std::uint32_t units) noexcept
        {
            AccuracyScore score;
            score.units_ = std::min(units, MaximumUnits);
            return score;
        }
        static constexpr AccuracyScore Average(std::uint64_t sum, std::size_t count) noexcept
        {
            return count == 0 ? AccuracyScore{} : FromUnits(static_cast<std::uint32_t>(sum / count));
        }
        constexpr std::uint32_t Units() const noexcept
        {
            return units_;
        }
        constexpr double Rate() const noexcept
        {
            return static_cast<double>(units_) / MaximumUnits;
        }
        constexpr double Percent() const noexcept
        {
            return static_cast<double>(units_) / 1'000'000;
        }
        bool operator==(const AccuracyScore &) const = default;

      private:
        std::uint32_t units_{};
    };
} // namespace finger_drum::rhythm
