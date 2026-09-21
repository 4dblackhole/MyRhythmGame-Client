#pragma once
#include "ParserSupport.h"

namespace finger_drum::chart::parsing
{
    inline void ParseStandaloneTimingCommand(const std::string_view line,
                                             const std::int64_t measure,
                                             const std::size_t lineNumber,
                                             const std::filesystem::path &source,
                                             ParseResult<PatternDocument> &result)
    {
        ParsedCommand command;
        if (!TryParseCommand(line, command))
        {
            AddDiagnostic(
                result.diagnostics, source, lineNumber,
                "A timing command requires '#command value' with whitespace before the value.");
            return;
        }

        if (command.name != "measure")
        {
            AddDiagnostic(result.diagnostics, source, lineNumber,
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
            if (!TryParseMusicalPosition(command.argument, 0, ratioPosition) ||
                ratioPosition.fraction <= Rational{0, 1})
            {
                AddDiagnostic(result.diagnostics, source, lineNumber,
                              "#measure requires a positive N/D ratio or C without whitespace "
                              "inside the ratio.");
                return;
            }
            directive.ratio = ratioPosition.fraction;
        }
        result.document.timing.push_back(std::move(directive));
    }

    inline void ParsePositionedTimingCommand(const std::string_view line,
                                             const std::int64_t measure,
                                             const std::size_t lineNumber,
                                             const std::filesystem::path &source,
                                             ParseResult<PatternDocument> &result)
    {
        const std::vector<std::string_view> fields = Split(line, ',');
        if (fields.size() != 2)
        {
            AddDiagnostic(result.diagnostics, source, lineNumber,
                          "A timing directive requires exactly 'N/D, #command value'.");
            return;
        }

        MusicalPosition position;
        if (!TryParseMusicalPosition(fields[0], measure, position))
        {
            AddDiagnostic(result.diagnostics, source, lineNumber,
                          "The timing directive requires N/D without whitespace inside the ratio.");
            return;
        }

        ParsedCommand command;
        if (!TryParseCommand(fields[1], command))
        {
            AddDiagnostic(result.diagnostics, source, lineNumber,
                          "A positioned timing command requires '#command value' with whitespace "
                          "before the value.");
            return;
        }

        TimingDirective directive;
        directive.position = position;
        directive.source = {source, lineNumber, 1};
        if (command.name == "bpm")
        {
            directive.type = TimingDirectiveType::Bpm;
            if (!ParseDouble(command.argument, directive.value) || !IsFinite(directive.value) ||
                directive.value <= 0.0)
            {
                AddDiagnostic(result.diagnostics, source, lineNumber,
                              "#bpm requires a finite value greater than zero.");
                return;
            }
        }
        else if (command.name == "delay")
        {
            directive.type = TimingDirectiveType::DelayMilliseconds;
            if (!ParseDouble(command.argument, directive.value) || !IsFinite(directive.value))
            {
                AddDiagnostic(result.diagnostics, source, lineNumber,
                              "#delay requires a finite millisecond value.");
                return;
            }
        }
        else if (command.name == "measure")
        {
            AddDiagnostic(result.diagnostics, source, lineNumber,
                          "#measure must be a standalone command for the current measure.");
            return;
        }
        else
        {
            AddDiagnostic(result.diagnostics, source, lineNumber,
                          "Legacy effect directive should be moved to a YME file: #" + command.name,
                          DiagnosticSeverity::Warning);
            return;
        }
        result.document.timing.push_back(std::move(directive));
    }

    [[nodiscard]] inline std::vector<Rational> BuildMeasureLengths(const PatternDocument &document)
    {
        std::int64_t maximumMeasure = 0;
        std::map<std::int64_t, Rational> changes;
        for (const TimingDirective &directive : document.timing)
        {
            maximumMeasure = std::max(maximumMeasure, directive.position.measure);
            if (directive.type == TimingDirectiveType::MeasureLength)
            {
                changes[directive.position.measure] = directive.ratio;
            }
        }
        for (const PatternNote &note : document.notes)
        {
            maximumMeasure = std::max(maximumMeasure, note.position.measure);
        }

        std::vector<Rational> lengths(static_cast<std::size_t>(maximumMeasure + 1), Rational{1, 1});
        Rational currentLength{1, 1};
        for (std::int64_t measure = 0; measure <= maximumMeasure; ++measure)
        {
            if (const auto change = changes.find(measure); change != changes.end())
            {
                currentLength = change->second;
            }
            lengths[static_cast<std::size_t>(measure)] = currentLength;
        }
        return lengths;
    }

    inline void RemoveOutOfMeasureEntries(ParseResult<PatternDocument> &result)
    {
        const std::vector<Rational> measureLengths = BuildMeasureLengths(result.document);
        const auto isOutsideMeasure = [&measureLengths](const MusicalPosition &position) {
            return position.measure < 0 || position.fraction < Rational{0, 1} ||
                   position.fraction >=
                       measureLengths.at(static_cast<std::size_t>(position.measure));
        };

        std::erase_if(result.document.notes, [&result, &isOutsideMeasure](const PatternNote &note) {
            if (!isOutsideMeasure(note.position))
            {
                return false;
            }
            AddDiagnostic(result.diagnostics, note.source.file, note.source.line,
                          "The note lies outside the current measure and was ignored.",
                          DiagnosticSeverity::Warning);
            return true;
        });

        std::erase_if(result.document.timing, [&result, &isOutsideMeasure](
                                                  const TimingDirective &directive) {
            if (directive.type == TimingDirectiveType::MeasureLength ||
                !isOutsideMeasure(directive.position))
            {
                return false;
            }
            AddDiagnostic(result.diagnostics, directive.source.file, directive.source.line,
                          "The timing directive lies outside the current measure and was ignored.",
                          DiagnosticSeverity::Warning);
            return true;
        });
    }
} // namespace finger_drum::chart::parsing
