#pragma once
#include "Parsing/ChartParser.h"
#include <cmath>
#include <stdexcept>

namespace editor_values
{
    namespace chart = finger_drum::chart;
    inline std::string Fraction(chart::Rational beat)
    {
        return std::to_string(beat.Numerator()) + "/" + std::to_string(beat.Denominator());
    }
    inline double Number(const std::string &value)
    {
        std::size_t used{};
        const double result = std::stod(value, &used);
        if (used != value.size() || !std::isfinite(result))
            throw std::invalid_argument("Invalid number.");
        return result;
    }
    inline std::int64_t Integer(const std::string &value)
    {
        std::size_t used{};
        const auto result = std::stoll(value, &used);
        if (used != value.size())
            throw std::invalid_argument("Invalid integer.");
        return result;
    }
    inline chart::MusicalPosition Position(const std::string &measure, const std::string &fraction)
    {
        chart::MusicalPosition result;
        const auto oneBased = Integer(measure);
        if (oneBased < 1 || !chart::TryParseMusicalPosition(fraction, oneBased - 1, result))
            throw std::invalid_argument("Use a one-based measure and N/D without internal spaces.");
        return result;
    }
} // namespace editor_values
