#pragma once

#include <compare>
#include <cstdint>

namespace finger_drum::chart
{
    class RationalNumber final
    {
    public:
        using Representation = std::int64_t;

        RationalNumber() = default;
        explicit RationalNumber(Representation numerator) noexcept;
        RationalNumber(
            Representation numerator,
            Representation denominator);

        [[nodiscard]] Representation Numerator() const noexcept;
        [[nodiscard]] Representation Denominator() const noexcept;
        [[nodiscard]] long double Value() const noexcept;

        [[nodiscard]] RationalNumber operator+() const noexcept;
        [[nodiscard]] RationalNumber operator-() const;
        [[nodiscard]] RationalNumber operator+(
            const RationalNumber& right) const;
        [[nodiscard]] RationalNumber operator-(
            const RationalNumber& right) const;
        [[nodiscard]] RationalNumber operator*(Representation scalar) const;

        RationalNumber& operator+=(const RationalNumber& right);
        RationalNumber& operator-=(const RationalNumber& right);
        RationalNumber& operator*=(Representation scalar);

        [[nodiscard]] std::strong_ordering operator<=> (
            const RationalNumber& right) const noexcept;
        [[nodiscard]] bool operator==(
            const RationalNumber& right) const noexcept = default;

    private:
        Representation numerator_{};
        Representation denominator_{1};
    };

    // Keep the existing chart-facing spelling while the implementation uses
    // the corrected RationalNumber value type.
    using Rational = RationalNumber;
}
