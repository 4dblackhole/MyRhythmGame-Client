#pragma once

#include "Common/RhythmTypes.h"
#include "Utility/RationalNumber.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace finger_drum::chart
{
    // File positions are limited to 100,000 measures (internal indices 0..99,999).
    inline constexpr std::int64_t MaximumMeasureCount = 100'000;

    struct MusicalPosition
    {
        std::int64_t measure{};
        Rational fraction{};

        auto operator<=>(const MusicalPosition &) const = default;
    };

    struct SourceLocation
    {
        std::filesystem::path file;
        std::size_t line{};
        std::size_t column{};
    };

    enum class DiagnosticSeverity : std::uint8_t
    {
        Warning,
        Error,
    };

    struct Diagnostic
    {
        DiagnosticSeverity severity{DiagnosticSeverity::Error};
        SourceLocation location;
        std::string message;
    };

    template <typename DocumentType> struct ParseResult
    {
        DocumentType document;
        std::vector<Diagnostic> diagnostics;

        [[nodiscard]] bool Succeeded() const noexcept
        {
            for (const Diagnostic &diagnostic : diagnostics)
            {
                if (diagnostic.severity == DiagnosticSeverity::Error)
                {
                    return false;
                }
            }
            return true;
        }
    };
} // namespace finger_drum::chart
