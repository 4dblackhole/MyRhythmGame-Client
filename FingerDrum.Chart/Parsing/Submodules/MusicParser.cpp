#include "Parsing/Submodules/ParserSupport.h"

namespace finger_drum::chart
{
    using namespace parsing;

    ParseResult<MusicDocument> ChartParser::ParseMusic(const std::string_view utf8,
                                                       std::filesystem::path source) const
    {
        ParseResult<MusicDocument> result;
        result.document.sourcePath = source;
        for (const auto &[lineNumber, rawLine] : EnumerateLines(utf8))
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
} // namespace finger_drum::chart
