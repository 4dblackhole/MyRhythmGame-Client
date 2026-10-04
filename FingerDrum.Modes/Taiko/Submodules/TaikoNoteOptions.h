#pragma once
#include "Model/ChartDocument.h"
#include "TaikoInputBinding.h"
#include "TaikoSoundIds.h"
#include <algorithm>
#include <cctype>
#include <charconv>

namespace finger_drum::mode::taiko_options
{
    [[nodiscard]] inline std::string_view Trim(std::string_view value) noexcept
    {
        while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())) != 0)
        {
            value.remove_prefix(1);
        }
        while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0)
        {
            value.remove_suffix(1);
        }
        return value;
    }

    [[nodiscard]] inline bool EqualsInsensitive(const std::string_view left, const std::string_view right) noexcept
    {
        return left.size() == right.size() && std::ranges::equal(left, right, [](const char a, const char b) {
                   return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
               });
    }

    [[nodiscard]] inline std::optional<std::string_view> FindOption(const chart::PatternNote &note,
                                                                    const std::string_view name) noexcept
    {
        for (const std::string &field : note.extraData)
        {
            const std::string_view view = field;
            const std::size_t equals = view.find('=');
            if (equals != std::string_view::npos && EqualsInsensitive(Trim(view.substr(0, equals)), name))
            {
                return Trim(view.substr(equals + 1));
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] inline bool ParsePositiveSize(const std::string_view value, std::size_t &output) noexcept
    {
        const char *const begin = value.data();
        const char *const end = begin + value.size();
        const auto parsed = std::from_chars(begin, end, output);
        return parsed.ec == std::errc{} && parsed.ptr == end && output > 0;
    }

    [[nodiscard]] inline std::optional<std::size_t> ReadPositiveOption(const chart::PatternNote &note,
                                                                       const std::string_view name,
                                                                       const std::optional<std::size_t> fallback,
                                                                       std::vector<chart::Diagnostic> &diagnostics)
    {
        const std::optional<std::string_view> value = FindOption(note, name);
        if (!value.has_value())
        {
            if (fallback.has_value())
            {
                return fallback;
            }
            diagnostics.push_back({chart::DiagnosticSeverity::Error, note.source,
                                   std::string(name) + " is required for this long note."});
            return std::nullopt;
        }

        std::size_t parsed{};
        if (!ParsePositiveSize(*value, parsed) || parsed > 1024)
        {
            diagnostics.push_back({chart::DiagnosticSeverity::Error, note.source,
                                   std::string(name) + " must be an integer from 1 through 1024."});
            return std::nullopt;
        }
        return parsed;
    }

    [[nodiscard]] inline std::optional<std::size_t> ReadHitCount(const chart::PatternNote &note,
                                                                 const std::size_t fallback,
                                                                 std::vector<chart::Diagnostic> &diagnostics)
    {
        std::optional<std::string_view> value;
        for (const std::string &field : note.extraData)
        {
            const std::string_view trimmed = Trim(field);
            if (!trimmed.empty() && trimmed.find('=') == std::string_view::npos)
            {
                value = trimmed;
                break;
            }
        }
        if (!value.has_value())
        {
            value = FindOption(note, finger_drum::mode::taiko_option::HitCount);
        }

        std::size_t parsed = fallback;
        if ((value.has_value() && !ParsePositiveSize(*value, parsed)) || parsed == 0 || parsed > 1024)
        {
            diagnostics.push_back(
                {chart::DiagnosticSeverity::Error, note.source, "Hit count must be an integer from 1 through 1024."});
            return std::nullopt;
        }
        return parsed;
    }

    [[nodiscard]] inline std::optional<TaikoAction> ReadBuzzAction(const chart::PatternNote &note,
                                                                   std::vector<chart::Diagnostic> &diagnostics)
    {
        const std::optional<std::string_view> value = FindOption(note, finger_drum::mode::taiko_option::Action);
        if (value.has_value() && EqualsInsensitive(*value, "Don"))
        {
            return TaikoAction::Don;
        }
        if (value.has_value() && EqualsInsensitive(*value, "Kat"))
        {
            return TaikoAction::Kat;
        }
        diagnostics.push_back(
            {chart::DiagnosticSeverity::Error, note.source, "Buzz requires Action=Don or Action=Kat."});
        return std::nullopt;
    }

} // namespace finger_drum::mode::taiko_options
