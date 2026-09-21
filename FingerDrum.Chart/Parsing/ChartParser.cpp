#include "Parsing/Submodules/ParserSupport.h"

namespace finger_drum::chart
{
    using namespace parsing;

    ParseResult<MusicDocument> ChartParser::ParseMusicFile(const std::filesystem::path &path) const
    {
        return ParseMusic(ReadUtf8File(path), path);
    }

    ParseResult<PatternDocument> ChartParser::ParsePatternFile(
        const std::filesystem::path &path) const
    {
        return ParsePattern(ReadUtf8File(path), path);
    }

    ParseResult<EffectDocument> ChartParser::ParseEffectFile(
        const std::filesystem::path &path) const
    {
        return ParseEffect(ReadUtf8File(path), path);
    }
} // namespace finger_drum::chart
