#include "Parsing/Submodules/PatternTiming.h"

namespace finger_drum::chart
{
    using namespace parsing;

    ParseResult<PatternDocument> ChartParser::ParsePattern(const std::string_view utf8,
                                                           std::filesystem::path source) const
    {
        ParseResult<PatternDocument> result;
        result.document.sourcePath = source;
        std::string section;
        std::int64_t timingMeasure = 0;
        std::int64_t patternMeasure = 0;
        std::size_t sourceOrder = 0;

        for (const auto &[lineNumber, rawLine] : EnumerateLines(utf8))
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
                std::int64_t *nextMeasure = nullptr;
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
                        result.document.systemBreakMeasures.push_back(*nextMeasure);
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
                if (!TryParseMusicalPosition(fields[0], patternMeasure, note.position) ||
                    !ParseInteger(fields[1], note.keyType) ||
                    !ParseInteger(fields[2], note.actionType))
                {
                    AddDiagnostic(
                        result.diagnostics, source, lineNumber,
                        "The pattern note contains an invalid position or integer field.");
                    continue;
                }
                if (fields.size() > 3)
                    note.hitSound = std::string(fields[3]);
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
                    ParseStandaloneTimingCommand(line, timingMeasure, lineNumber, source, result);
                    continue;
                }
                ParsePositionedTimingCommand(line, timingMeasure, lineNumber, source, result);
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
            else if (key == "Music metadata")
                result.document.musicMetadataFile = PathFromUtf8(value);
            else if (StartsWithInsensitive(key, "Pattern Maker") &&
                     !StartsWithInsensitive(key, "Pattern Maker Count"))
                result.document.makers.emplace_back(value);
            else if (key == "Pattern Name")
                result.document.name = std::string(value);
            else if (key == "Mode")
                result.document.mode = std::string(value);
            else if (key == "Pattern Offset")
            {
                if (!ParseDouble(value, result.document.patternOffsetMilliseconds) ||
                    !IsFinite(result.document.patternOffsetMilliseconds))
                {
                    AddDiagnostic(result.diagnostics, source, lineNumber,
                                  "Pattern Offset requires a finite millisecond value.");
                }
            }
            else if (key == "Base BPM")
            {
                if (!ParseDouble(value, result.document.baseBpm) ||
                    !IsFinite(result.document.baseBpm))
                {
                    AddDiagnostic(result.diagnostics, source, lineNumber,
                                  "Base BPM requires a finite numeric value.");
                }
            }
            else if (key == "JudgeLevel")
            {
                if (!ParseInteger(value, result.document.judgementLevel) ||
                    result.document.judgementLevel == 0)
                {
                    AddDiagnostic(result.diagnostics, source, lineNumber,
                                  "JudgeLevel must be a positive integer.");
                }
            }
            else if (key == "Tags")
            {
                for (const std::string_view tag : Split(value, ','))
                    result.document.tags.emplace_back(tag);
            }
            else if (section == "HitSounds")
            {
                if (key.empty() || value.empty() ||
                    !result.document.hitSounds.emplace(std::string(key), PathFromUtf8(value))
                         .second)
                {
                    AddDiagnostic(result.diagnostics, source, lineNumber,
                                  "A hit sound requires a unique index and a non-empty path.");
                }
            }
        }

        if (result.document.baseBpm <= 0.0)
        {
            AddDiagnostic(result.diagnostics, source, 1, "Base BPM must be greater than zero.");
        }
        RemoveOutOfMeasureEntries(result);
        std::ranges::sort(result.document.systemBreakMeasures);
        const auto duplicateBreaks = std::ranges::unique(result.document.systemBreakMeasures);
        result.document.systemBreakMeasures.erase(duplicateBreaks.begin(), duplicateBreaks.end());
        std::ranges::stable_sort(result.document.notes,
                                 [](const PatternNote &left, const PatternNote &right) {
                                     if (left.position != right.position)
                                     {
                                         return left.position < right.position;
                                     }
                                     return left.sourceOrder < right.sourceOrder;
                                 });
        return result;
    }
} // namespace finger_drum::chart
