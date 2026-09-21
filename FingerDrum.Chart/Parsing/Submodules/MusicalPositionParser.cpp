#include "Parsing/Submodules/ParserSupport.h"

namespace finger_drum::chart
{
    using namespace parsing;

    bool TryParseMusicalPosition(std::string_view value, const std::int64_t implicitMeasure,
                                 MusicalPosition &output) noexcept
    {
        value = Trim(value);
        if (value.empty() || std::ranges::any_of(value, [](const char character) {
                return std::isspace(static_cast<unsigned char>(character)) != 0;
            }))
        {
            return false;
        }
        std::int64_t measure = implicitMeasure;
        const std::size_t measureSeparator = value.find(',');
        if (measureSeparator != std::string_view::npos)
        {
            if (!ParseInteger(value.substr(0, measureSeparator), measure))
            {
                return false;
            }
            value = Trim(value.substr(measureSeparator + 1));
        }

        const std::size_t slash = value.find('/');
        if (slash == std::string_view::npos || value.find('/', slash + 1) != std::string_view::npos)
        {
            return false;
        }
        std::int64_t numerator = 0;
        std::int64_t denominator = 0;
        if (!ParseInteger(value.substr(0, slash), numerator) ||
            !ParseInteger(value.substr(slash + 1), denominator) || denominator <= 0 ||
            numerator < 0 || measure < 0)
        {
            return false;
        }
        output = {measure, Rational{numerator, denominator}};
        return true;
    }
} // namespace finger_drum::chart
