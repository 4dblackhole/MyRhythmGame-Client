#include "Parsing/Submodules/ParserSupport.h"

namespace finger_drum::chart
{
    using namespace parsing;

    ParseResult<EffectDocument> ChartParser::ParseEffect(const std::string_view utf8,
                                                         std::filesystem::path source) const
    {
        ParseResult<EffectDocument> result;
        result.document.sourcePath = source;
        std::string section;
        std::int64_t measure = 0;
        for (const auto &[lineNumber, rawLine] : EnumerateLines(utf8))
        {
            const std::string_view line = Trim(rawLine);
            if (line.empty() || StartsWithInsensitive(line, "//"))
                continue;
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
                    AddDiagnostic(result.diagnostics, source, lineNumber,
                                  "Version must be an integer.");
                }
                continue;
            }

            const std::vector<std::string_view> fields = Split(line, ',');
            if (section == "HitSound Changes")
            {
                HitSoundChange change;
                std::int64_t measureNumber{};
                if (fields.size() != 4 || !ParseInteger(fields[0], measureNumber) ||
                    measureNumber < 1 ||
                    !TryParseMusicalPosition(fields[1], measureNumber - 1, change.position) ||
                    fields[2].empty() || !ParseInteger(fields[3], change.keyType) ||
                    (change.keyType != 1 && change.keyType != 2))
                {
                    AddDiagnostic(result.diagnostics, source, lineNumber,
                                  "A hit sound change requires: measure (1-based), N/D, sound "
                                  "index, key ID (1=Don, 2=Kat).");
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
            if (fields.size() > 2)
                command.target = std::string(fields[2]);
            bool valid = true;
            if (fields.size() > 3 &&
                (!ParseDouble(fields[3], command.beginValue) || !IsFinite(command.beginValue)))
            {
                AddDiagnostic(result.diagnostics, source, lineNumber,
                              "An effect begin value must be a finite number.");
                valid = false;
            }
            command.endValue = command.beginValue;
            if (fields.size() > 4 &&
                (!ParseDouble(fields[4], command.endValue) || !IsFinite(command.endValue)))
            {
                AddDiagnostic(result.diagnostics, source, lineNumber,
                              "An effect end value must be a finite number.");
                valid = false;
            }
            if (fields.size() > 5 &&
                (!ParseDouble(fields[5], command.durationMilliseconds) ||
                 !IsFinite(command.durationMilliseconds) || command.durationMilliseconds < 0.0))
            {
                AddDiagnostic(
                    result.diagnostics, source, lineNumber,
                    "An effect duration must be a finite non-negative millisecond value.");
                valid = false;
            }
            if (fields.size() > 6)
                command.curve = ParseCurve(fields[6]);
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
                        !TryParseMusicalPosition(end.substr(colon + 1), endMeasure - 1,
                                                 endPosition) ||
                        endPosition <= command.position || command.endPosition)
                    {
                        AddDiagnostic(
                            result.diagnostics, source, lineNumber,
                            "Effect End requires a later one-based measure:N/D position.");
                        valid = false;
                    }
                    else
                        command.endPosition = endPosition;
                }
                else
                    command.arguments.emplace_back(fields[index]);
            }
            if (valid)
            {
                result.document.commands.push_back(std::move(command));
            }
        }
        return result;
    }
} // namespace finger_drum::chart
