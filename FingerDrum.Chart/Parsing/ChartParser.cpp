#include "Parsing/ChartParser.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace finger_drum::chart
{
    namespace
    {
        [[nodiscard]] std::string ReadUtf8File(
            const std::filesystem::path& path)
        {
            std::ifstream stream(path, std::ios::binary);
            if (!stream)
            {
                throw std::runtime_error(
                    "Unable to open chart file: " + path.string());
            }
            return std::string(
                std::istreambuf_iterator<char>(stream),
                std::istreambuf_iterator<char>());
        }

        [[nodiscard]] std::string_view Trim(std::string_view value) noexcept
        {
            while (!value.empty() &&
                std::isspace(static_cast<unsigned char>(value.front())) != 0)
            {
                value.remove_prefix(1);
            }
            while (!value.empty() &&
                std::isspace(static_cast<unsigned char>(value.back())) != 0)
            {
                value.remove_suffix(1);
            }
            return value;
        }

        [[nodiscard]] std::vector<std::string_view> Split(
            std::string_view value,
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

        [[nodiscard]] bool StartsWithInsensitive(
            const std::string_view value,
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
        [[nodiscard]] bool ParseInteger(
            const std::string_view value,
            IntegerType& output) noexcept
        {
            const std::string_view trimmed = Trim(value);
            const auto [end, error] = std::from_chars(
                trimmed.data(),
                trimmed.data() + trimmed.size(),
                output);
            return error == std::errc{} &&
                end == trimmed.data() + trimmed.size();
        }

        [[nodiscard]] bool ParseDouble(
            const std::string_view value,
            double& output) noexcept
        {
            const std::string owned(Trim(value));
            char* end = nullptr;
            output = std::strtod(owned.c_str(), &end);
            return end != owned.c_str() && *end == '\0';
        }

        [[nodiscard]] std::pair<std::string_view, std::string_view>
        SplitKeyValue(const std::string_view line) noexcept
        {
            const std::size_t colon = line.find(':');
            if (colon == std::string_view::npos)
            {
                return {Trim(line), {}};
            }
            return {
                Trim(line.substr(0, colon)),
                Trim(line.substr(colon + 1))};
        }

        void AddDiagnostic(
            std::vector<Diagnostic>& diagnostics,
            const std::filesystem::path& source,
            const std::size_t line,
            std::string message,
            const DiagnosticSeverity severity = DiagnosticSeverity::Error)
        {
            diagnostics.push_back(Diagnostic{
                severity,
                SourceLocation{source, line, 1},
                std::move(message)});
        }

        [[nodiscard]] std::string NormalizeCommand(
            std::string_view command)
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
                    result.push_back(static_cast<char>(std::tolower(
                        static_cast<unsigned char>(character))));
                }
            }
            return result;
        }

        [[nodiscard]] std::filesystem::path PathFromUtf8(
            const std::string_view value)
        {
            return std::filesystem::path(std::u8string(
                reinterpret_cast<const char8_t*>(value.data()),
                value.size()));
        }

        [[nodiscard]] std::vector<std::pair<std::size_t, std::string_view>>
        EnumerateLines(const std::string_view text)
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

        [[nodiscard]] AutomationCurve ParseCurve(
            const std::string_view value) noexcept
        {
            const std::string normalized = NormalizeCommand(value);
            if (normalized == "linear") return AutomationCurve::Linear;
            if (normalized == "smoothstep") return AutomationCurve::Smoothstep;
            if (normalized == "exponential") return AutomationCurve::Exponential;
            return AutomationCurve::Step;
        }

        [[nodiscard]] EffectCommandType ParseEffectType(
            const std::string_view value) noexcept
        {
            const std::string normalized = NormalizeCommand(value);
            if (normalized == "scrollspeed") return EffectCommandType::ScrollSpeed;
            if (normalized == "notespeed") return EffectCommandType::NoteSpeed;
            if (normalized == "busvolume" || normalized == "volume") return EffectCommandType::BusVolume;
            if (normalized == "reverbsend") return EffectCommandType::ReverbSend;
            if (normalized == "lowpasscutoff") return EffectCommandType::LowPassCutoff;
            if (normalized == "highpasscutoff") return EffectCommandType::HighPassCutoff;
            if (normalized == "syncopationzone" || normalized == "area") return EffectCommandType::SyncopationZone;
            return EffectCommandType::Custom;
        }
    }

    ParseResult<MusicDocument> ChartParser::ParseMusicFile(
        const std::filesystem::path& path) const
    {
        return ParseMusic(ReadUtf8File(path), path);
    }

    ParseResult<PatternDocument> ChartParser::ParsePatternFile(
        const std::filesystem::path& path) const
    {
        return ParsePattern(ReadUtf8File(path), path);
    }

    ParseResult<EffectDocument> ChartParser::ParseEffectFile(
        const std::filesystem::path& path) const
    {
        return ParseEffect(ReadUtf8File(path), path);
    }

    ParseResult<MusicDocument> ChartParser::ParseMusic(
        const std::string_view utf8,
        std::filesystem::path source) const
    {
        ParseResult<MusicDocument> result;
        result.document.sourcePath = source;
        for (const auto& [lineNumber, rawLine] : EnumerateLines(utf8))
        {
            const std::string_view line = Trim(rawLine);
            if (line.empty() || StartsWithInsensitive(line, "//"))
            {
                continue;
            }
            const auto [key, value] = SplitKeyValue(line);
            if (key == "Version")
            {
                if (!ParseInteger(value, result.document.version))
                {
                    AddDiagnostic(result.diagnostics, source, lineNumber,
                        "Version must be an integer.");
                }
            }
            else if (key == "File")
            {
                result.document.audioFile = PathFromUtf8(value);
            }
            else if (StartsWithInsensitive(key, "Music Name") &&
                !StartsWithInsensitive(key, "Music Name Count"))
            {
                result.document.names.emplace_back(value);
            }
            else if (StartsWithInsensitive(key, "Artist") &&
                !StartsWithInsensitive(key, "Artist Count"))
            {
                result.document.artists.emplace_back(value);
            }
            else if (key == "Tags")
            {
                for (const std::string_view tag : Split(value, ','))
                {
                    result.document.tags.emplace_back(tag);
                }
            }
        }
        if (result.document.audioFile.empty())
        {
            AddDiagnostic(result.diagnostics, source, 1,
                "Music metadata is missing the audio File field.");
        }
        return result;
    }

    ParseResult<PatternDocument> ChartParser::ParsePattern(
        const std::string_view utf8,
        std::filesystem::path source) const
    {
        ParseResult<PatternDocument> result;
        result.document.sourcePath = source;
        std::string section;
        std::int64_t timingMeasure = 0;
        std::int64_t patternMeasure = 0;
        std::size_t sourceOrder = 0;

        for (const auto& [lineNumber, rawLine] : EnumerateLines(utf8))
        {
            const std::string_view line = Trim(rawLine);
            if (line.empty() || StartsWithInsensitive(line, "//"))
            {
                continue;
            }
            if (line.front() == '[' && line.back() == ']')
            {
                section = std::string(Trim(line.substr(1, line.size() - 2)));
                continue;
            }
            if (line == "--")
            {
                if (section == "Pattern") ++patternMeasure;
                else if (section == "Time Signature") ++timingMeasure;
                continue;
            }

            if (section == "Pattern")
            {
                const std::vector<std::string_view> fields = Split(line, ',');
                if (fields.size() < 3)
                {
                    AddDiagnostic(result.diagnostics, source, lineNumber,
                        "A pattern note requires position, key type and action type.");
                    continue;
                }
                PatternNote note;
                if (!TryParseMusicalPosition(
                        fields[0], patternMeasure, note.position) ||
                    !ParseInteger(fields[1], note.keyType) ||
                    !ParseInteger(fields[2], note.actionType))
                {
                    AddDiagnostic(result.diagnostics, source, lineNumber,
                        "The pattern note contains an invalid position or integer field.");
                    continue;
                }
                if (fields.size() > 3) note.hitSound = std::string(fields[3]);
                for (std::size_t index = 4; index < fields.size(); ++index)
                {
                    note.extraData.emplace_back(fields[index]);
                }
                note.source = {source, lineNumber, 1};
                note.sourceOrder = sourceOrder++;
                result.document.notes.push_back(std::move(note));
                continue;
            }

            if (section == "Time Signature")
            {
                // The original RPG YMP grammar permits measure directives
                // without an N/D prefix. They affect the current implicit
                // measure and persist until the next #measure declaration.
                if (line.front() == '#')
                {
                    const std::vector<std::string_view> commandParts =
                        Split(line, ' ');
                    const std::string command = NormalizeCommand(
                        commandParts.front());
                    if (command == "measure")
                    {
                        if (commandParts.size() < 2)
                        {
                            AddDiagnostic(result.diagnostics, source, lineNumber,
                                "#measure requires N/D or C.");
                            continue;
                        }
                        TimingDirective directive;
                        directive.position = {
                            timingMeasure,
                            Rational{0, 1}};
                        directive.type = TimingDirectiveType::MeasureLength;
                        directive.source = {source, lineNumber, 1};
                        if (NormalizeCommand(commandParts[1]) == "c")
                        {
                            directive.ratio = Rational{1, 1};
                        }
                        else
                        {
                            MusicalPosition ratioPosition;
                            if (!TryParseMusicalPosition(
                                    commandParts[1],
                                    0,
                                    ratioPosition))
                            {
                                AddDiagnostic(result.diagnostics, source, lineNumber,
                                    "#measure requires a valid N/D ratio or C.");
                                continue;
                            }
                            directive.ratio = ratioPosition.fraction;
                        }
                        result.document.timing.push_back(std::move(directive));
                    }
                    else
                    {
                        AddDiagnostic(result.diagnostics, source, lineNumber,
                            "Legacy effect directive should be moved to a YME file: " +
                                std::string(commandParts.front()),
                            DiagnosticSeverity::Warning);
                    }
                    continue;
                }

                const std::vector<std::string_view> fields = Split(line, ',');
                if (fields.size() < 2)
                {
                    continue;
                }
                MusicalPosition position;
                if (!TryParseMusicalPosition(
                        fields[0], timingMeasure, position))
                {
                    AddDiagnostic(result.diagnostics, source, lineNumber,
                        "The timing directive has an invalid musical position.");
                    continue;
                }
                std::vector<std::string_view> commandParts = Split(fields[1], ' ');
                if (commandParts.empty()) continue;
                const std::string command = NormalizeCommand(commandParts[0]);
                TimingDirective directive;
                directive.position = position;
                directive.source = {source, lineNumber, 1};
                if (command == "bpm")
                {
                    directive.type = TimingDirectiveType::Bpm;
                    if (commandParts.size() < 2 ||
                        !ParseDouble(commandParts[1], directive.value))
                    {
                        AddDiagnostic(result.diagnostics, source, lineNumber,
                            "#bpm requires a numeric value.");
                        continue;
                    }
                }
                else if (command == "delay")
                {
                    directive.type = TimingDirectiveType::DelayMilliseconds;
                    if (commandParts.size() < 2 ||
                        !ParseDouble(commandParts[1], directive.value))
                    {
                        AddDiagnostic(result.diagnostics, source, lineNumber,
                            "#Delay requires milliseconds.");
                        continue;
                    }
                }
                else if (command == "measure")
                {
                    directive.type = TimingDirectiveType::MeasureLength;
                    MusicalPosition ratioPosition;
                    if (commandParts.size() < 2 ||
                        !TryParseMusicalPosition(
                            commandParts[1], 0, ratioPosition))
                    {
                        AddDiagnostic(result.diagnostics, source, lineNumber,
                            "#measure requires an N/D ratio.");
                        continue;
                    }
                    directive.ratio = ratioPosition.fraction;
                }
                else
                {
                    // Legacy visual/audio commands are accepted but belong in
                    // YME. Keep a warning so conversion tools can migrate them.
                    AddDiagnostic(result.diagnostics, source, lineNumber,
                        "Legacy effect directive should be moved to a YME file: " +
                            std::string(commandParts[0]),
                        DiagnosticSeverity::Warning);
                    continue;
                }
                result.document.timing.push_back(std::move(directive));
                continue;
            }

            const auto [key, value] = SplitKeyValue(line);
            if (key == "Version") static_cast<void>(ParseInteger(value, result.document.version));
            else if (key == "Music metadata") result.document.musicMetadataFile = PathFromUtf8(value);
            else if (StartsWithInsensitive(key, "Pattern Maker") && !StartsWithInsensitive(key, "Pattern Maker Count")) result.document.makers.emplace_back(value);
            else if (key == "Pattern Name") result.document.name = std::string(value);
            else if (key == "Mode") result.document.mode = std::string(value);
            else if (key == "Pattern Offset") static_cast<void>(ParseDouble(value, result.document.patternOffsetMilliseconds));
            else if (key == "Base BPM") static_cast<void>(ParseDouble(value, result.document.baseBpm));
            else if (key == "JudgeLevel") static_cast<void>(ParseInteger(value, result.document.judgementLevel));
            else if (key == "Tags")
            {
                for (const std::string_view tag : Split(value, ',')) result.document.tags.emplace_back(tag);
            }
            else if (section == "HitSounds")
            {
                result.document.hitSounds.emplace(
                    std::string(key),
                    PathFromUtf8(value));
            }
        }

        if (result.document.baseBpm <= 0.0)
        {
            AddDiagnostic(result.diagnostics, source, 1,
                "Base BPM must be greater than zero.");
        }
        std::ranges::stable_sort(
            result.document.notes,
            [](const PatternNote& left, const PatternNote& right)
            {
                if (left.position != right.position)
                {
                    return left.position < right.position;
                }
                return left.sourceOrder < right.sourceOrder;
            });
        return result;
    }

    ParseResult<EffectDocument> ChartParser::ParseEffect(
        const std::string_view utf8,
        std::filesystem::path source) const
    {
        ParseResult<EffectDocument> result;
        result.document.sourcePath = source;
        std::string section;
        std::int64_t measure = 0;
        for (const auto& [lineNumber, rawLine] : EnumerateLines(utf8))
        {
            const std::string_view line = Trim(rawLine);
            if (line.empty() || StartsWithInsensitive(line, "//")) continue;
            if (line.front() == '[' && line.back() == ']')
            {
                section = std::string(Trim(line.substr(1, line.size() - 2)));
                continue;
            }
            if (line == "--")
            {
                ++measure;
                continue;
            }
            const auto [key, value] = SplitKeyValue(line);
            if (section.empty() && key == "Version")
            {
                static_cast<void>(ParseInteger(value, result.document.version));
                continue;
            }

            const std::vector<std::string_view> fields = Split(line, ',');
            if (fields.size() < 2)
            {
                AddDiagnostic(result.diagnostics, source, lineNumber,
                    "An effect command requires a position and command.");
                continue;
            }
            EffectCommand command;
            if (!TryParseMusicalPosition(fields[0], measure, command.position))
            {
                AddDiagnostic(result.diagnostics, source, lineNumber,
                    "The effect command has an invalid musical position.");
                continue;
            }
            command.type = ParseEffectType(fields[1]);
            command.source = {source, lineNumber, 1};
            if (fields.size() > 2) command.target = std::string(fields[2]);
            if (fields.size() > 3) static_cast<void>(ParseDouble(fields[3], command.beginValue));
            command.endValue = command.beginValue;
            if (fields.size() > 4) static_cast<void>(ParseDouble(fields[4], command.endValue));
            if (fields.size() > 5) static_cast<void>(ParseDouble(fields[5], command.durationMilliseconds));
            if (fields.size() > 6) command.curve = ParseCurve(fields[6]);
            for (std::size_t index = 7; index < fields.size(); ++index)
            {
                command.arguments.emplace_back(fields[index]);
            }
            result.document.commands.push_back(std::move(command));
        }
        return result;
    }

    bool TryParseMusicalPosition(
        std::string_view value,
        const std::int64_t implicitMeasure,
        MusicalPosition& output) noexcept
    {
        value = Trim(value);
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
        if (slash == std::string_view::npos)
        {
            return false;
        }
        std::int64_t numerator = 0;
        std::int64_t denominator = 0;
        if (!ParseInteger(value.substr(0, slash), numerator) ||
            !ParseInteger(value.substr(slash + 1), denominator) ||
            denominator <= 0 || numerator < 0)
        {
            return false;
        }
        output = {measure, Rational{numerator, denominator}};
        return true;
    }
}
