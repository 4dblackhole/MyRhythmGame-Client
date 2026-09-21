#pragma once
#include "Parsing/ChartParser.h"
#include <algorithm>
#include <charconv>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace finger_drum::chart::parsing
{
    [[nodiscard]] inline std::string ReadUtf8File(const std::filesystem::path &path)
    {
        std::ifstream stream(path, std::ios::binary);
        if (!stream)
        {
            throw std::runtime_error("Unable to open chart file: " + path.string());
        }
        return std::string(std::istreambuf_iterator<char>(stream),
                           std::istreambuf_iterator<char>());
    }

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

    [[nodiscard]] inline std::vector<std::string_view> Split(std::string_view value,
                                                             const char delimiter)
    {
        std::vector<std::string_view> result;
        while (true)
        {
            const std::size_t position = value.find(delimiter);
            result.push_back(Trim(value.substr(0, position)));
            if (position == std::string_view::npos)
            {
                break;
            }
            value.remove_prefix(position + 1);
        }
        return result;
    }

    [[nodiscard]] inline bool StartsWithInsensitive(const std::string_view value,
                                                    const std::string_view prefix) noexcept
    {
        if (value.size() < prefix.size())
        {
            return false;
        }
        for (std::size_t index = 0; index < prefix.size(); ++index)
        {
            if (std::tolower(static_cast<unsigned char>(value[index])) !=
                std::tolower(static_cast<unsigned char>(prefix[index])))
            {
                return false;
            }
        }
        return true;
    }

    template <typename IntegerType>
    [[nodiscard]] inline bool ParseInteger(const std::string_view value,
                                           IntegerType &output) noexcept
    {
        const std::string_view trimmed = Trim(value);
        const auto [end, error] =
            std::from_chars(trimmed.data(), trimmed.data() + trimmed.size(), output);
        return error == std::errc{} && end == trimmed.data() + trimmed.size();
    }

    [[nodiscard]] inline bool ParseDouble(const std::string_view value, double &output) noexcept
    {
        const std::string owned(Trim(value));
        char *end = nullptr;
        output = std::strtod(owned.c_str(), &end);
        return end != owned.c_str() && *end == '\0';
    }

    [[nodiscard]] inline std::pair<std::string_view, std::string_view> SplitKeyValue(
        const std::string_view line) noexcept
    {
        const std::size_t colon = line.find(':');
        if (colon == std::string_view::npos)
        {
            return {Trim(line), {}};
        }
        return {Trim(line.substr(0, colon)), Trim(line.substr(colon + 1))};
    }

    inline void AddDiagnostic(std::vector<Diagnostic> &diagnostics,
                              const std::filesystem::path &source, const std::size_t line,
                              std::string message,
                              const DiagnosticSeverity severity = DiagnosticSeverity::Error)
    {
        diagnostics.push_back(
            Diagnostic{severity, SourceLocation{source, line, 1}, std::move(message)});
    }

    [[nodiscard]] inline std::string NormalizeCommand(std::string_view command)
    {
        command = Trim(command);
        if (!command.empty() && command.front() == '#')
        {
            command.remove_prefix(1);
        }
        std::string result;
        for (const char character : command)
        {
            if (std::isspace(static_cast<unsigned char>(character)) == 0)
            {
                result.push_back(
                    static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
            }
        }
        return result;
    }

    [[nodiscard]] inline std::filesystem::path PathFromUtf8(const std::string_view value)
    {
        return std::filesystem::path(
            std::u8string(reinterpret_cast<const char8_t *>(value.data()), value.size()));
    }

    [[nodiscard]] inline std::vector<std::pair<std::size_t, std::string_view>> EnumerateLines(
        const std::string_view text)
    {
        std::vector<std::pair<std::size_t, std::string_view>> lines;
        std::size_t lineNumber = 1;
        std::size_t begin = 0;
        while (begin <= text.size())
        {
            const std::size_t end = text.find('\n', begin);
            std::string_view line = text.substr(begin, end - begin);
            if (!line.empty() && line.back() == '\r')
            {
                line.remove_suffix(1);
            }
            lines.emplace_back(lineNumber++, line);
            if (end == std::string_view::npos)
            {
                break;
            }
            begin = end + 1;
        }
        return lines;
    }

    [[nodiscard]] inline AutomationCurve ParseCurve(const std::string_view value) noexcept
    {
        const std::string normalized = NormalizeCommand(value);
        if (normalized == "linear")
            return AutomationCurve::Linear;
        if (normalized == "smoothstep")
            return AutomationCurve::Smoothstep;
        if (normalized == "exponential")
            return AutomationCurve::Exponential;
        return AutomationCurve::Step;
    }

    [[nodiscard]] inline EffectCommandType ParseEffectType(const std::string_view value) noexcept
    {
        const std::string normalized = NormalizeCommand(value);
        if (normalized == "scrollspeed")
            return EffectCommandType::ScrollSpeed;
        if (normalized == "notespeed")
            return EffectCommandType::NoteSpeed;
        if (normalized == "busvolume" || normalized == "volume")
            return EffectCommandType::BusVolume;
        if (normalized == "reverbsend")
            return EffectCommandType::ReverbSend;
        if (normalized == "lowpasscutoff")
            return EffectCommandType::LowPassCutoff;
        if (normalized == "highpasscutoff")
            return EffectCommandType::HighPassCutoff;
        if (normalized == "syncopationzone" || normalized == "area")
            return EffectCommandType::SyncopationZone;
        if (normalized == "measurelinevisible")
            return EffectCommandType::MeasureLineVisible;
        return EffectCommandType::Custom;
    }

    struct ParsedCommand
    {
        std::string name;
        std::string_view argument;
    };

    [[nodiscard]] inline bool TryParseCommand(std::string_view value, ParsedCommand &output)
    {
        value = Trim(value);
        const std::size_t separator = value.find_first_of(" \t");
        if (separator == std::string_view::npos)
        {
            return false;
        }

        const std::string_view token = value.substr(0, separator);
        const std::string_view argument = Trim(value.substr(separator));
        if (token.size() <= 1 || token.front() != '#' || argument.empty())
        {
            return false;
        }

        output.name = NormalizeCommand(token);
        output.argument = argument;
        return !output.name.empty();
    }

    [[nodiscard]] inline bool IsFinite(const double value) noexcept
    {
        return std::isfinite(value);
    }
} // namespace finger_drum::chart::parsing
