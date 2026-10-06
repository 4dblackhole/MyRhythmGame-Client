#pragma once
#include "Model/ChartDocument.h"
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
    finger_drum::chart::EffectCommandType type;
    std::size_t labelIndex;
    bool hasAudioBus;
};
inline constexpr std::array EditorEffectDefinitions{
    EditorEffectDefinition{finger_drum::chart::EffectCommandType::BusVolume, 0, true},
    EditorEffectDefinition{finger_drum::chart::EffectCommandType::MeasureLineVisible, 1, false},
    EditorEffectDefinition{finger_drum::chart::EffectCommandType::NoteSpeed, 2, false},
    EditorEffectDefinition{finger_drum::chart::EffectCommandType::ScrollSpeed, 3, false}};

struct EditorHitSoundTarget
{
    int keyType{};
};
using EditorEffectSelection = std::variant<finger_drum::chart::EffectCommandType, EditorHitSoundTarget>;

// Text fields are drafts. Apply builds a candidate; document validation publishes it.
class EditorEffectForm
{
  public:
    std::string startMeasure{"1"}, startFraction{"0/4"}, endMeasure, endFraction;
    std::string beginValue{"1"}, endValue{"1"}, audioBus{"HitSound"}, curveName;
    EditorEffectSelection selection{finger_drum::chart::EffectCommandType::BusVolume};
    finger_drum::chart::AutomationCurve curve{finger_drum::chart::AutomationCurve::Linear};
    void Load(const finger_drum::chart::EffectDocument &, std::size_t row);
    [[nodiscard]] finger_drum::chart::EffectDocument Apply(const finger_drum::chart::EffectDocument &) const;
};
