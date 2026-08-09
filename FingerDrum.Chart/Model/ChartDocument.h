#pragma once

#include "Common/RhythmTypes.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace finger_drum::chart
{
    struct Rational
    {
        std::int64_t numerator{};
        std::int64_t denominator{1};

        Rational() = default;
        Rational(std::int64_t numeratorValue, std::int64_t denominatorValue);

        [[nodiscard]] long double Value() const noexcept;
        auto operator<=>(const Rational&) const = default;
    };

    struct MusicalPosition
    {
        std::int64_t measure{};
        Rational fraction{};

        auto operator<=>(const MusicalPosition&) const = default;
    };

    struct SourceLocation
    {
        std::filesystem::path file;
        std::size_t line{};
        std::size_t column{};
    };

    enum class DiagnosticSeverity : std::uint8_t
    {
        Warning,
        Error,
    };

    struct Diagnostic
    {
        DiagnosticSeverity severity{DiagnosticSeverity::Error};
        SourceLocation location;
        std::string message;
    };

    template <typename DocumentType>
    struct ParseResult
    {
        DocumentType document;
        std::vector<Diagnostic> diagnostics;

        [[nodiscard]] bool Succeeded() const noexcept
        {
            for (const Diagnostic& diagnostic : diagnostics)
            {
                if (diagnostic.severity == DiagnosticSeverity::Error)
                {
                    return false;
                }
            }
            return true;
        }
    };

    struct MusicDocument
    {
        int version{1};
        std::filesystem::path sourcePath;
        std::filesystem::path audioFile;
        std::vector<std::string> names;
        std::vector<std::string> artists;
        std::vector<std::string> tags;
    };

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
        std::vector<std::string> makers;
        std::vector<std::string> tags;
        std::string name;
        std::string mode;
        double patternOffsetMilliseconds{};
        double baseBpm{120.0};
        std::size_t judgementLevel{50};
        std::map<std::string, std::filesystem::path, std::less<>> hitSounds;
        std::vector<TimingDirective> timing;
        std::vector<PatternNote> notes;
    };

    enum class EffectCommandType : std::uint8_t
    {
        ScrollSpeed,
        NoteSpeed,
        BusVolume,
        ReverbSend,
        LowPassCutoff,
        HighPassCutoff,
        SyncopationZone,
        Custom,
    };

    enum class AutomationCurve : std::uint8_t
    {
        Step,
        Linear,
        Smoothstep,
        Exponential,
    };

    struct EffectCommand
    {
        MusicalPosition position;
        EffectCommandType type{EffectCommandType::Custom};
        std::string target;
        double beginValue{};
        double endValue{};
        double durationMilliseconds{};
        AutomationCurve curve{AutomationCurve::Step};
        std::vector<std::string> arguments;
        SourceLocation source;
    };

    struct EffectDocument
    {
        int version{1};
        std::filesystem::path sourcePath;
        std::vector<EffectCommand> commands;
    };

    struct CompiledPatternNote
    {
        PatternNote note;
        rhythm::RhythmTime timing{};
    };

    struct CompiledEffectCommand
    {
        EffectCommand command;
        rhythm::RhythmTime timing{};
        rhythm::RhythmDuration duration{};
    };
}
