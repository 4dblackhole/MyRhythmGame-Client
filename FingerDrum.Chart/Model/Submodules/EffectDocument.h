#pragma once
#include "Model/Submodules/ChartTypes.h"
#include <memory>

namespace finger_drum::chart
{
    class InterpolationExpression;
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
        MeasureLineVisible,
        Kiai,
    };

    enum class AutomationCurve : std::uint8_t
    {
        Step,
        Linear,
        Smoothstep,
        Exponential,
        Harmonic,
        Expression,
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
        std::optional<MusicalPosition> endPosition;
        std::string customCommand;
        std::string curveName;
        std::shared_ptr<const InterpolationExpression> interpolation;
        std::vector<std::size_t> excludedDivisions;
    };

    // YME uses one-based measure numbers on disk; positions remain zero-based.
    struct HitSoundChange
    {
        MusicalPosition position;
        std::string soundIndex;
        int keyType{};
        SourceLocation source;
    };

    struct EffectDocument
    {
        int version{1};
        std::filesystem::path sourcePath;
        std::map<std::string, std::filesystem::path, std::less<>> hitSounds;
        std::map<std::string, std::string, std::less<>> interpolations;
        std::vector<EffectCommand> commands;
        std::vector<HitSoundChange> hitSoundChanges;
    };
} // namespace finger_drum::chart
