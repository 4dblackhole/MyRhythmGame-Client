#pragma once
#include "Judgement/JudgeLevel.h"
#include "Model/Submodules/ChartTypes.h"

enum class TimingDirectiveType : std::uint8_t
{
    Bpm,
    MeasureLength,
    DelayMilliseconds,
};

struct TimingDirective
{
    MusicalPosition position;
    TimingDirectiveType type{TimingDirectiveType::Bpm};
    double value{};
    Rational ratio{1, 1};
    SourceLocation source;
};

struct PatternNote
{
    MusicalPosition position;
    int keyType{};
    int actionType{};
    std::string hitSound;
    std::vector<std::string> extraData;
    SourceLocation source;
    std::size_t sourceOrder{};
};

struct PatternDocument
{
    int version{1};
    std::filesystem::path sourcePath;
    std::filesystem::path musicMetadataFile;
    std::filesystem::path effectFile;
    // Relative to <selected Songs>/BG, independently of the YMP folder.
    std::filesystem::path backgroundFile;
    double backgroundOffsetX{}, backgroundOffsetY{}; // 1280x720 logical space, +Y up.
    std::vector<std::string> makers;
    std::vector<std::string> tags;
    std::string name;
    std::string mode;
    double patternOffsetMilliseconds{};
    double baseBpm{120.0};
    JudgeLevel judgementLevel;
    std::vector<TimingDirective> timing;
    std::vector<PatternNote> notes;
    // Each entry is the zero-based measure that starts a new notation
    // system after a `---` boundary in the source YMP.
    std::vector<std::int64_t> systemBreakMeasures;
};
