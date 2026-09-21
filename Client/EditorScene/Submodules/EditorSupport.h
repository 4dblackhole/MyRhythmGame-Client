#pragma once
#include "MRG_Core.h"
#include "Editing/ChartEditor.h"
#include "Parsing/ChartParser.h"
#include <Windows.h>
#include <cmath>
#include <stdexcept>

namespace editor_ui
{
    namespace chart = finger_drum::chart;
    namespace v = mrg::visual2d;
    constexpr v::Color Background{.914F, .961F, 1, 1}, Paper{.973F, .988F, 1, 1};
    constexpr v::Color Ink{.17F, .34F, .47F, 1}, Blue{.31F, .62F, .88F, 1},
        Pale{.82F, .91F, .96F, 1}, White{1, 1, 1, 1};
    constexpr v::Color Don{1, .33F, .43F, 1}, Kat{.10F, .76F, .82F, 1}, Gold{.96F, .80F, .29F, 1};
    inline std::wstring Wide(const std::string &s)
    {
        const int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(),
                                          static_cast<int>(s.size()), nullptr, 0);
        std::wstring w(static_cast<std::size_t>(n), L'\0');
        if (n)
            MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
        return w;
    }
    inline std::string Utf8(const std::wstring &w)
    {
        const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr,
                                          0, nullptr, nullptr);
        std::string s(static_cast<std::size_t>(n), '\0');
        if (n)
            WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), s.data(), n,
                                nullptr, nullptr);
        return s;
    }
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
        if (!chart::TryParseMusicalPosition(fraction, Integer(measure) - 1, result))
            throw std::invalid_argument("Use a one-based measure and N/D without internal spaces.");
        return result;
    }
    inline std::int64_t MeasureNearTime(const chart::MusicalTimeline &timeline, double milliseconds)
    {
        // Seek through cached prefix sums/tempo anchors, not from measure zero.
        std::int64_t low = 0, high = 1;
        const auto at = [&](std::int64_t measure) {
            return timeline.Compile({measure, {}}).count() / 1000.0;
        };
        while (at(high) <= milliseconds)
        {
            if (high > 1'000'000)
                throw std::invalid_argument("Editor time is beyond the supported measure range.");
            high *= 2;
        }
        while (low + 1 < high)
        {
            const auto middle = low + (high - low) / 2;
            if (at(middle) <= milliseconds)
                low = middle;
            else
                high = middle;
        }
        return low;
    }
} // namespace editor_ui

namespace editor_ui
{
    std::optional<std::string> EditText(const std::wstring &, const std::string &);
}
