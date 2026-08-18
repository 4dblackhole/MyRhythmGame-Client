#include "Utility/RationalNumber.h"

#include <limits>
#include <numeric>
#include <stdexcept>

namespace finger_drum::chart
{
    namespace
    {
        using Representation = RationalNumber::Representation;

        [[nodiscard]] std::uint64_t Magnitude(
            const Representation value) noexcept
        {
            if (value >= 0)
            {
                return static_cast<std::uint64_t>(value);
            }
            return std::uint64_t{} - static_cast<std::uint64_t>(value);
        }

        [[nodiscard]] Representation CheckedAdd(
            const Representation left,
            const Representation right)
        {
            constexpr Representation Minimum =
                std::numeric_limits<Representation>::min();
            constexpr Representation Maximum =
                std::numeric_limits<Representation>::max();
            if ((right > 0 && left > Maximum - right) ||
                (right < 0 && left < Minimum - right))
            {
                throw std::overflow_error("Rational addition overflowed.");
            }
            return left + right;
        }

        [[nodiscard]] Representation CheckedMultiply(
            const Representation left,
            const Representation right)
        {
            constexpr Representation Minimum =
                std::numeric_limits<Representation>::min();
            constexpr Representation Maximum =
                std::numeric_limits<Representation>::max();
            if (left > 0)
            {
                if ((right > 0 && left > Maximum / right) ||
                    (right < 0 && right < Minimum / left))
                {
                    throw std::overflow_error(
                        "Rational multiplication overflowed.");
                }
            }
            else if (left < 0)
            {
                if ((right > 0 && left < Minimum / right) ||
                    (right < 0 && right < Maximum / left))
                {
                    throw std::overflow_error(
                        "Rational multiplication overflowed.");
                }
            }
            return left * right;
        }

        [[nodiscard]] std::strong_ordering CompareUnsignedFractions(
            std::uint64_t leftNumerator,
            std::uint64_t leftDenominator,
            std::uint64_t rightNumerator,
            std::uint64_t rightDenominator) noexcept
        {
            const auto reverseOrdering = [](const std::strong_ordering value)
            {
                if (value == std::strong_ordering::less)
                {
                    return std::strong_ordering::greater;
                }
                if (value == std::strong_ordering::greater)
                {
                    return std::strong_ordering::less;
                }
                return std::strong_ordering::equal;
            };
            bool reversed = false;
            while (true)
            {
                const std::uint64_t leftQuotient =
                    leftNumerator / leftDenominator;
                const std::uint64_t rightQuotient =
                    rightNumerator / rightDenominator;
                if (leftQuotient != rightQuotient)
                {
                    const std::strong_ordering result = leftQuotient <
                        rightQuotient ? std::strong_ordering::less :
                        std::strong_ordering::greater;
                    return reversed ? reverseOrdering(result) : result;
                }

                const std::uint64_t leftRemainder =
                    leftNumerator % leftDenominator;
                const std::uint64_t rightRemainder =
                    rightNumerator % rightDenominator;
                if (leftRemainder == 0 || rightRemainder == 0)
                {
                    std::strong_ordering result = std::strong_ordering::equal;
                    if (leftRemainder == 0 && rightRemainder != 0)
                    {
                        result = std::strong_ordering::less;
                    }
                    else if (leftRemainder != 0 && rightRemainder == 0)
                    {
                        result = std::strong_ordering::greater;
                    }
                    return reversed ? reverseOrdering(result) : result;
                }

                leftNumerator = leftDenominator;
                leftDenominator = leftRemainder;
                rightNumerator = rightDenominator;
                rightDenominator = rightRemainder;
                reversed = !reversed;
            }
        }
    }

    RationalNumber::RationalNumber(const Representation numerator) noexcept
        : numerator_(numerator)
    {
    }

    RationalNumber::RationalNumber(
        const Representation numerator,
        const Representation denominator)
    {
        if (denominator <= 0)
        {
            throw std::invalid_argument(
                "A rational denominator must be greater than zero.");
        }
        const std::uint64_t divisor = std::gcd(
            Magnitude(numerator),
            static_cast<std::uint64_t>(denominator));
        numerator_ = numerator / static_cast<Representation>(divisor);
        denominator_ = denominator / static_cast<Representation>(divisor);
    }

    RationalNumber::Representation RationalNumber::Numerator() const noexcept
    {
        return numerator_;
    }

    RationalNumber::Representation RationalNumber::Denominator() const noexcept
    {
        return denominator_;
    }

    long double RationalNumber::Value() const noexcept
    {
        return static_cast<long double>(numerator_) /
            static_cast<long double>(denominator_);
    }

    RationalNumber RationalNumber::operator+() const noexcept
    {
        return *this;
    }

    RationalNumber RationalNumber::operator-() const
    {
        if (numerator_ == std::numeric_limits<Representation>::min())
        {
            throw std::overflow_error("Rational negation overflowed.");
        }
        return RationalNumber{-numerator_, denominator_};
    }

    RationalNumber RationalNumber::operator+(
        const RationalNumber& right) const
    {
        const Representation divisor = std::gcd(
            denominator_,
            right.denominator_);
        const Representation leftFactor = right.denominator_ / divisor;
        const Representation rightFactor = denominator_ / divisor;
        const Representation numerator = CheckedAdd(
            CheckedMultiply(numerator_, leftFactor),
            CheckedMultiply(right.numerator_, rightFactor));
        const Representation denominator = CheckedMultiply(
            denominator_,
            leftFactor);
        return RationalNumber{numerator, denominator};
    }

    RationalNumber RationalNumber::operator-(
        const RationalNumber& right) const
    {
        return *this + (-right);
    }

    RationalNumber RationalNumber::operator*(
        const Representation scalar) const
    {
        return RationalNumber{
            CheckedMultiply(numerator_, scalar),
            denominator_};
    }

    RationalNumber& RationalNumber::operator+=(const RationalNumber& right)
    {
        *this = *this + right;
        return *this;
    }

    RationalNumber& RationalNumber::operator-=(const RationalNumber& right)
    {
        *this = *this - right;
        return *this;
    }

    RationalNumber& RationalNumber::operator*=(const Representation scalar)
    {
        *this = *this * scalar;
        return *this;
    }

    std::strong_ordering RationalNumber::operator<=> (
        const RationalNumber& right) const noexcept
    {
        if (numerator_ < 0 && right.numerator_ >= 0)
        {
            return std::strong_ordering::less;
        }
        if (numerator_ >= 0 && right.numerator_ < 0)
        {
            return std::strong_ordering::greater;
        }

        const std::strong_ordering magnitudeComparison =
            CompareUnsignedFractions(
                Magnitude(numerator_),
                static_cast<std::uint64_t>(denominator_),
                Magnitude(right.numerator_),
                static_cast<std::uint64_t>(right.denominator_));
        if (numerator_ < 0)
        {
            if (magnitudeComparison == std::strong_ordering::less)
            {
                return std::strong_ordering::greater;
            }
            if (magnitudeComparison == std::strong_ordering::greater)
            {
                return std::strong_ordering::less;
            }
        }
        return magnitudeComparison;
    }
}
