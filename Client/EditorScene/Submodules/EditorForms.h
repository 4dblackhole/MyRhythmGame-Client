#pragma once
#include "Model/Submodules/EffectDocument.h"
#include <array>
#include <variant>

enum class EditorTab
{
    Pattern,
    Timing,
    Metadata,
    Effects,
    Audio
};

struct EditorTimingForm
{
    std::string measure{"1"}, fraction{"0/4"}, bpm{"120"}, measureLength{"4/4"};
};

struct EditorEffectDefinition
{
    EffectCommandType type;
    std::size_t labelIndex;
    bool hasAudioBus;
};
inline constexpr std::array EditorEffectDefinitions{
    EditorEffectDefinition{EffectCommandType::BusVolume, 0, true},
    EditorEffectDefinition{EffectCommandType::MeasureLineVisible, 1, false},
    EditorEffectDefinition{EffectCommandType::NoteSpeed, 2, false},
    EditorEffectDefinition{EffectCommandType::ScrollSpeed, 3, false}};

struct EditorHitSoundTarget
{
    int keyType{};
};
using EditorEffectSelection = std::variant<EffectCommandType, EditorHitSoundTarget>;

// Text fields are drafts. Apply builds a candidate; document validation publishes it.
class EditorEffectForm
{
  public:
    std::string startMeasure{"1"}, startFraction{"0/4"}, endMeasure, endFraction;
    std::string beginValue{"1"}, endValue{"1"}, audioBus{"HitSound"}, curveName;
    EditorEffectSelection selection{EffectCommandType::BusVolume};
    AutomationCurve curve{AutomationCurve::Linear};
    void Load(const EffectDocument &, std::size_t row);
    [[nodiscard]] EffectDocument Apply(const EffectDocument &) const;
};
