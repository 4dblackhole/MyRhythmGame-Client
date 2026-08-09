#include "Model/ChartDocument.h"

#include <numeric>
#include <stdexcept>

namespace finger_drum::chart
{
    Rational::Rational(
        std::int64_t numeratorValue,
        std::int64_t denominatorValue)
    {
        if (denominatorValue == 0)
        {
            throw std::invalid_argument(
                "A rational denominator cannot be zero.");
        }
        if (denominatorValue < 0)
        {
            numeratorValue = -numeratorValue;
            denominatorValue = -denominatorValue;
        }
        const std::int64_t divisor = std::gcd(
            numeratorValue,
            denominatorValue);
        numerator = numeratorValue / divisor;
        denominator = denominatorValue / divisor;
    }

    long double Rational::Value() const noexcept
    {
        return static_cast<long double>(numerator) /
            static_cast<long double>(denominator);
    }
}
