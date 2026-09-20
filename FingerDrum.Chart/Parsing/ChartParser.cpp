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
            if (normalized == "measurelinevisible") return EffectCommandType::MeasureLineVisible;
            return EffectCommandType::Custom;
        }

        struct ParsedCommand
        {
            std::string name;
            std::string_view argument;
        };

        [[nodiscard]] bool TryParseCommand(
            std::string_view value,
            ParsedCommand& output)
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

        [[nodiscard]] bool IsFinite(const double value) noexcept
        {
            return std::isfinite(value);
        }

        void ParseStandaloneTimingCommand(
            const std::string_view line,
            const std::int64_t measure,
            const std::size_t lineNumber,
            const std::filesystem::path& source,
            ParseResult<PatternDocument>& result)
        {
            ParsedCommand command;
            if (!TryParseCommand(line, command))
            {
                AddDiagnostic(
                    result.diagnostics,
                    source,
                    lineNumber,
                    "A timing command requires '#command value' with whitespace before the value.");
                return;
            }

            if (command.name != "measure")
            {
                AddDiagnostic(
                    result.diagnostics,
                    source,
                    lineNumber,
                    "Legacy effect directive should be moved to a YME file: " +
                        std::string(line.substr(0, line.find_first_of(" \t"))),
                    DiagnosticSeverity::Warning);
                return;
            }

            TimingDirective directive;
            directive.position = {measure, Rational{0, 1}};
            directive.type = TimingDirectiveType::MeasureLength;
            directive.source = {source, lineNumber, 1};
            if (NormalizeCommand(command.argument) == "c")
            {
                directive.ratio = Rational{1, 1};
            }
            else
            {
                MusicalPosition ratioPosition;
                if (!TryParseMusicalPosition(
                        command.argument,
                        0,
                        ratioPosition) ||
                    ratioPosition.fraction <= Rational{0, 1})
                {
                    AddDiagnostic(
                        result.diagnostics,
                        source,
                        lineNumber,
                        "#measure requires a positive N/D ratio or C without whitespace inside the ratio.");
                    return;
                }
                directive.ratio = ratioPosition.fraction;
            }
            result.document.timing.push_back(std::move(directive));
        }

        void ParsePositionedTimingCommand(
            const std::string_view line,
            const std::int64_t measure,
            const std::size_t lineNumber,
            const std::filesystem::path& source,
            ParseResult<PatternDocument>& result)
        {
            const std::vector<std::string_view> fields = Split(line, ',');
            if (fields.size() != 2)
            {
                AddDiagnostic(
                    result.diagnostics,
                    source,
                    lineNumber,
                    "A timing directive requires exactly 'N/D, #command value'.");
                return;
            }

            MusicalPosition position;
            if (!TryParseMusicalPosition(fields[0], measure, position))
            {
                AddDiagnostic(
                    result.diagnostics,
                    source,
                    lineNumber,
                    "The timing directive requires N/D without whitespace inside the ratio.");
                return;
            }

            ParsedCommand command;
            if (!TryParseCommand(fields[1], command))
            {
                AddDiagnostic(
                    result.diagnostics,
                    source,
                    lineNumber,
                    "A positioned timing command requires '#command value' with whitespace before the value.");
                return;
            }

            TimingDirective directive;
            directive.position = position;
            directive.source = {source, lineNumber, 1};
            if (command.name == "bpm")
            {
                directive.type = TimingDirectiveType::Bpm;
                if (!ParseDouble(command.argument, directive.value) ||
                    !IsFinite(directive.value) || directive.value <= 0.0)
                {
                    AddDiagnostic(
                        result.diagnostics,
                        source,
                        lineNumber,
                        "#bpm requires a finite value greater than zero.");
                    return;
                }
            }
            else if (command.name == "delay")
            {
                directive.type = TimingDirectiveType::DelayMilliseconds;
                if (!ParseDouble(command.argument, directive.value) ||
                    !IsFinite(directive.value))
                {
                    AddDiagnostic(
                        result.diagnostics,
                        source,
                        lineNumber,
                        "#delay requires a finite millisecond value.");
                    return;
                }
            }
            else if (command.name == "measure")
            {
                AddDiagnostic(
                    result.diagnostics,
                    source,
                    lineNumber,
                    "#measure must be a standalone command for the current measure.");
                return;
            }
            else
            {
                AddDiagnostic(
                    result.diagnostics,
                    source,
                    lineNumber,
                    "Legacy effect directive should be moved to a YME file: #" +
                        command.name,
                    DiagnosticSeverity::Warning);
                return;
            }
            result.document.timing.push_back(std::move(directive));
        }

        [[nodiscard]] std::vector<Rational> BuildMeasureLengths(
            const PatternDocument& document)
        {
            std::int64_t maximumMeasure = 0;
            std::map<std::int64_t, Rational> changes;
            for (const TimingDirective& directive : document.timing)
            {
                maximumMeasure = std::max(
                    maximumMeasure,
                    directive.position.measure);
                if (directive.type == TimingDirectiveType::MeasureLength)
                {
                    changes[directive.position.measure] = directive.ratio;
                }
            }
            for (const PatternNote& note : document.notes)
            {
                maximumMeasure = std::max(maximumMeasure, note.position.measure);
            }

            std::vector<Rational> lengths(
                static_cast<std::size_t>(maximumMeasure + 1),
                Rational{1, 1});
            Rational currentLength{1, 1};
            for (std::int64_t measure = 0;
                 measure <= maximumMeasure;
                 ++measure)
            {
                if (const auto change = changes.find(measure);
                    change != changes.end())
                {
                    currentLength = change->second;
                }
                lengths[static_cast<std::size_t>(measure)] = currentLength;
            }
            return lengths;
        }

        void RemoveOutOfMeasureEntries(ParseResult<PatternDocument>& result)
        {
            const std::vector<Rational> measureLengths =
                BuildMeasureLengths(result.document);
            const auto isOutsideMeasure = [&measureLengths](
                const MusicalPosition& position)
            {
                return position.measure < 0 ||
                    position.fraction < Rational{0, 1} ||
                    position.fraction >= measureLengths.at(
                        static_cast<std::size_t>(position.measure));
            };

            std::erase_if(
                result.document.notes,
                [&result, &isOutsideMeasure](const PatternNote& note)
                {
                    if (!isOutsideMeasure(note.position))
                    {
                        return false;
                    }
                    AddDiagnostic(
                        result.diagnostics,
                        note.source.file,
                        note.source.line,
                        "The note lies outside the current measure and was ignored.",
                        DiagnosticSeverity::Warning);
                    return true;
                });

            std::erase_if(
                result.document.timing,
                [&result, &isOutsideMeasure](const TimingDirective& directive)
                {
                    if (directive.type == TimingDirectiveType::MeasureLength ||
                        !isOutsideMeasure(directive.position))
                    {
                        return false;
                    }
                    AddDiagnostic(
                        result.diagnostics,
                        directive.source.file,
                        directive.source.line,
                        "The timing directive lies outside the current measure and was ignored.",
                        DiagnosticSeverity::Warning);
                    return true;
                });
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
            if (line == "--" || line == "---")
            {
                std::int64_t* nextMeasure = nullptr;
                if (section == "Pattern")
                {
                    nextMeasure = &patternMeasure;
                }
                else if (section == "Time Signature")
                {
                    nextMeasure = &timingMeasure;
                }
                if (nextMeasure != nullptr)
                {
                    ++*nextMeasure;
                    if (line == "---")
                    {
                        result.document.systemBreakMeasures.push_back(
                            *nextMeasure);
                    }
                }
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
                if (line.front() == '#')
                {
                    ParseStandaloneTimingCommand(
                        line,
                        timingMeasure,
                        lineNumber,
                        source,
                        result);
                    continue;
                }
                ParsePositionedTimingCommand(
                    line,
                    timingMeasure,
                    lineNumber,
                    source,
                    result);
                continue;
            }

            const auto [key, value] = SplitKeyValue(line);
            if (key == "Version")
            {
                if (!ParseInteger(value, result.document.version))
                {
                    AddDiagnostic(
                        result.diagnostics,
                        source,
                        lineNumber,
                        "Version must be an integer.");
                }
            }
            else if (key == "Music metadata") result.document.musicMetadataFile = PathFromUtf8(value);
            else if (StartsWithInsensitive(key, "Pattern Maker") && !StartsWithInsensitive(key, "Pattern Maker Count")) result.document.makers.emplace_back(value);
            else if (key == "Pattern Name") result.document.name = std::string(value);
            else if (key == "Mode") result.document.mode = std::string(value);
            else if (key == "Pattern Offset")
            {
                if (!ParseDouble(
                        value,
                        result.document.patternOffsetMilliseconds) ||
                    !IsFinite(result.document.patternOffsetMilliseconds))
                {
                    AddDiagnostic(
                        result.diagnostics,
                        source,
                        lineNumber,
                        "Pattern Offset requires a finite millisecond value.");
                }
            }
            else if (key == "Base BPM")
            {
                if (!ParseDouble(value, result.document.baseBpm) ||
                    !IsFinite(result.document.baseBpm))
                {
                    AddDiagnostic(
                        result.diagnostics,
                        source,
                        lineNumber,
                        "Base BPM requires a finite numeric value.");
                }
            }
            else if (key == "JudgeLevel")
            {
                if (!ParseInteger(value, result.document.judgementLevel) ||
                    result.document.judgementLevel == 0)
                {
                    AddDiagnostic(
                        result.diagnostics,
                        source,
                        lineNumber,
                        "JudgeLevel must be a positive integer.");
                }
            }
            else if (key == "Tags")
            {
                for (const std::string_view tag : Split(value, ',')) result.document.tags.emplace_back(tag);
            }
            else if (section == "HitSounds")
            {
                if (key.empty() || value.empty() ||
                    !result.document.hitSounds.emplace(
                        std::string(key), PathFromUtf8(value)).second)
                {
                    AddDiagnostic(result.diagnostics, source, lineNumber,
                        "A hit sound requires a unique index and a non-empty path.");
                }
            }
        }

        if (result.document.baseBpm <= 0.0)
        {
            AddDiagnostic(result.diagnostics, source, 1,
                "Base BPM must be greater than zero.");
        }
        RemoveOutOfMeasureEntries(result);
        std::ranges::sort(result.document.systemBreakMeasures);
        const auto duplicateBreaks = std::ranges::unique(
            result.document.systemBreakMeasures);
        result.document.systemBreakMeasures.erase(
            duplicateBreaks.begin(),
            duplicateBreaks.end());
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
                if (!ParseInteger(value, result.document.version))
                {
                    AddDiagnostic(
                        result.diagnostics,
                        source,
                        lineNumber,
                        "Version must be an integer.");
                }
                continue;
            }

            const std::vector<std::string_view> fields = Split(line, ',');
            if (section == "HitSound Changes")
            {
                HitSoundChange change;
                std::int64_t measureNumber{};
                if (fields.size() != 4 ||
                    !ParseInteger(fields[0], measureNumber) || measureNumber < 1 ||
                    !TryParseMusicalPosition(fields[1], measureNumber - 1,
                        change.position) || fields[2].empty() ||
                    !ParseInteger(fields[3], change.keyType) ||
                    (change.keyType != 1 && change.keyType != 2))
                {
                    AddDiagnostic(result.diagnostics, source, lineNumber,
                        "A hit sound change requires: measure (1-based), N/D, sound index, key ID (1=Don, 2=Kat).");
                    continue;
                }
                change.soundIndex = std::string(fields[2]);
                change.source = {source, lineNumber, 1};
                result.document.hitSoundChanges.push_back(std::move(change));
                continue;
            }
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
            if (command.type == EffectCommandType::Custom)
                command.customCommand = std::string(fields[1]);
            command.source = {source, lineNumber, 1};
            if (fields.size() > 2) command.target = std::string(fields[2]);
            bool valid = true;
            if (fields.size() > 3 &&
                (!ParseDouble(fields[3], command.beginValue) ||
                    !IsFinite(command.beginValue)))
            {
                AddDiagnostic(
                    result.diagnostics,
                    source,
                    lineNumber,
                    "An effect begin value must be a finite number.");
                valid = false;
            }
            command.endValue = command.beginValue;
            if (fields.size() > 4 &&
                (!ParseDouble(fields[4], command.endValue) ||
                    !IsFinite(command.endValue)))
            {
                AddDiagnostic(
                    result.diagnostics,
                    source,
                    lineNumber,
                    "An effect end value must be a finite number.");
                valid = false;
            }
            if (fields.size() > 5 &&
                (!ParseDouble(fields[5], command.durationMilliseconds) ||
                    !IsFinite(command.durationMilliseconds) ||
                    command.durationMilliseconds < 0.0))
            {
                AddDiagnostic(
                    result.diagnostics,
                    source,
                    lineNumber,
                    "An effect duration must be a finite non-negative millisecond value.");
                valid = false;
            }
            if (fields.size() > 6) command.curve = ParseCurve(fields[6]);
            for (std::size_t index = 7; index < fields.size(); ++index)
            {
                if (fields[index].starts_with("End="))
                {
                    const auto end = fields[index].substr(4);
                    const auto colon = end.find(':');
                    std::int64_t endMeasure{};
                    MusicalPosition endPosition;
                    if (colon == std::string_view::npos ||
                        !ParseInteger(end.substr(0, colon), endMeasure) || endMeasure < 1 ||
                        !TryParseMusicalPosition(end.substr(colon + 1), endMeasure - 1, endPosition) ||
                        endPosition <= command.position || command.endPosition)
                    {
                        AddDiagnostic(result.diagnostics, source, lineNumber,
                            "Effect End requires a later one-based measure:N/D position.");
                        valid = false;
                    }
                    else command.endPosition = endPosition;
                }
                else command.arguments.emplace_back(fields[index]);
            }
            if (valid)
            {
                result.document.commands.push_back(std::move(command));
            }
        }
        return result;
    }

    bool TryParseMusicalPosition(
        std::string_view value,
        const std::int64_t implicitMeasure,
        MusicalPosition& output) noexcept
    {
        value = Trim(value);
        if (value.empty() || std::ranges::any_of(
                value,
                [](const char character)
                {
                    return std::isspace(
                        static_cast<unsigned char>(character)) != 0;
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
        if (slash == std::string_view::npos ||
            value.find('/', slash + 1) != std::string_view::npos)
        {
            return false;
        }
        std::int64_t numerator = 0;
        std::int64_t denominator = 0;
        if (!ParseInteger(value.substr(0, slash), numerator) ||
            !ParseInteger(value.substr(slash + 1), denominator) ||
            denominator <= 0 || numerator < 0 || measure < 0)
        {
            return false;
        }
        output = {measure, Rational{numerator, denominator}};
        return true;
    }
}
