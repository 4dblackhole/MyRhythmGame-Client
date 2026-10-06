#include "EditorForms.h"
#include "EditorValueParsing.h"
#include <algorithm>
using namespace editor_values;

void EditorEffectForm::Load(const chart::EffectDocument &effects, std::size_t row)
{
    auto draft = *this;
    draft.endMeasure.clear();
    draft.endFraction.clear();
    if (row < effects.commands.size())
    {
        const auto &command = effects.commands[row];
        if (std::ranges::find(EditorEffectDefinitions, command.type, &EditorEffectDefinition::type) ==
            EditorEffectDefinitions.end())
            throw std::runtime_error("This effect is preserved; its editor is not supported.");
        draft.selection = command.type;
        draft.startMeasure = std::to_string(command.position.measure + 1);
        draft.startFraction = Fraction(command.position.fraction);
        draft.beginValue = std::to_string(command.beginValue);
        draft.endValue = std::to_string(command.endValue);
        draft.audioBus = command.target;
        draft.curve = command.curve;
        draft.curveName = command.curveName;
        if (command.endPosition)
        {
            draft.endMeasure = std::to_string(command.endPosition->measure + 1);
            draft.endFraction = Fraction(command.endPosition->fraction);
        }
    }
    else
    {
        const auto &change = effects.hitSoundChanges.at(row - effects.commands.size());
        draft.selection = EditorHitSoundTarget{change.keyType};
        draft.startMeasure = std::to_string(change.position.measure + 1);
        draft.startFraction = Fraction(change.position.fraction);
        draft.beginValue = change.soundIndex;
    }
    *this = std::move(draft);
}
chart::EffectDocument EditorEffectForm::Apply(const chart::EffectDocument &source) const
{
    auto effects = source;
    const auto position = Position(startMeasure, startFraction);
    if (const auto *sound = std::get_if<EditorHitSoundTarget>(&selection))
    {
        std::erase_if(effects.hitSoundChanges,
                      [&](const auto &c) { return c.position == position && c.keyType == sound->keyType; });
        effects.hitSoundChanges.push_back({position, beginValue, sound->keyType});
    }
    else
    {
        chart::EffectCommand command;
        command.position = position;
        command.type = std::get<chart::EffectCommandType>(selection);
        const auto definition = std::ranges::find(EditorEffectDefinitions, command.type, &EditorEffectDefinition::type);
        if (definition == EditorEffectDefinitions.end())
            throw std::invalid_argument("Unsupported automation type.");
        command.target = definition->hasAudioBus ? audioBus : "";
        command.beginValue = Number(beginValue);
        command.endValue = Number(endValue);
        command.curve = curve;
        command.curveName = curveName;
        if (command.curve == chart::AutomationCurve::Expression)
        {
            const auto current = std::ranges::find_if(source.commands, [&](const auto &c) { return c.curveName == curveName; });
            if (current != source.commands.end()) command.interpolation = current->interpolation;
        }
        if (!endMeasure.empty() || !endFraction.empty())
            command.endPosition = Position(endMeasure, endFraction);
        else
            command.endValue = command.beginValue;
        std::erase_if(effects.commands, [&](const auto &c) {
            return c.position == position && c.type == command.type && c.target == command.target;
        });
        effects.commands.push_back(std::move(command));
    }
    return effects;
}
