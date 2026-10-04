#include "EditorView.h"
#include <algorithm>
using namespace editor_ui;

std::wstring EditorView::EffectLabel(const EditorEffectSelection &selection) const
{
    if (const auto *type = std::get_if<chart::EffectCommandType>(&selection))
    {
        const auto definition = std::ranges::find(EditorEffectDefinitions, *type, &EditorEffectDefinition::type);
        return definition == EditorEffectDefinitions.end() ? std::to_wstring(static_cast<int>(*type))
                                                           : std::wstring(Texts().effectTypes[definition->labelIndex]);
    }
    const int key = std::get<EditorHitSoundTarget>(selection).keyType;
    for (const auto &choice : state_.Mode().SoundChoices(texts_.CurrentLanguage()))
        if (choice.keyType == key)
            return choice.effectLabel;
    return std::to_wstring(key);
}
void EditorView::CycleEffectType()
{
    std::vector<EditorEffectSelection> choices;
    for (const auto &definition : EditorEffectDefinitions)
        choices.emplace_back(definition.type);
    for (const auto &sound : state_.Mode().SoundChoices(texts_.CurrentLanguage()))
        choices.emplace_back(EditorHitSoundTarget{sound.keyType});
    const auto matches = [&](const EditorEffectSelection &value) {
        if (value.index() != state_.effectForm.selection.index())
            return false;
        if (const auto *type = std::get_if<chart::EffectCommandType>(&value))
            return *type == std::get<chart::EffectCommandType>(state_.effectForm.selection);
        return std::get<EditorHitSoundTarget>(value).keyType ==
               std::get<EditorHitSoundTarget>(state_.effectForm.selection).keyType;
    };
    auto next = std::ranges::find_if(choices, matches);
    if (next != choices.end())
        ++next;
    state_.effectForm.selection = next == choices.end() ? choices.front() : *next;
    state_.RequestRebuild();
}
void EditorView::SelectEffectRow(std::size_t row)
{
    auto draft = state_.effectForm;
    draft.Load(state_.Document().Effects(), row);
    if (const auto *sound = std::get_if<EditorHitSoundTarget>(&draft.selection))
    {
        const auto choices = state_.Mode().SoundChoices(texts_.CurrentLanguage());
        if (std::ranges::find(choices, sound->keyType, &EditorSoundChoice::keyType) == choices.end())
            throw std::runtime_error("This sound target is not supported by the selected editor mode.");
    }
    state_.effectForm = std::move(draft);
    state_.RequestRebuild();
}
void EditorView::DrawEffects()
{
    Text({136, 105, 1000, 40}, std::wstring(Texts().effectsTitle), 28);
    Box({132, 173, 1728, 310}, White, 12);
    Box({132, 503, 1728, 365}, White, 12);
    Text({156, 190, 1650, 38}, std::wstring(Texts().effectsColumns), 22);
    DrawEffectRows();
    DrawEffectFields();
    Text({156, 800, 1620, 50}, std::wstring(state_.Mode().SoundEffectHelp(texts_.CurrentLanguage())), 18);
}
void EditorView::DrawEffectRows()
{
    const auto &effects = state_.Document().Effects();
    const auto total = effects.commands.size() + effects.hitSoundChanges.size();
    for (std::size_t row = state_.listOffset; row < total && row < state_.listOffset + 4; ++row)
    {
        const float y = 245 + static_cast<float>(row - state_.listOffset) * 48;
        std::wstring description;
        if (row < effects.commands.size())
        {
            const auto &command = effects.commands[row];
            description = std::to_wstring(command.position.measure + 1) + L" / " +
                          Wide(Fraction(command.position.fraction)) + L"    " + EffectLabel(command.type) + L"    " +
                          std::to_wstring(command.beginValue) + L" → " + std::to_wstring(command.endValue);
        }
        else
        {
            const auto &change = effects.hitSoundChanges[row - effects.commands.size()];
            const auto choices = state_.Mode().SoundChoices(texts_.CurrentLanguage());
            const auto found = std::ranges::find(choices, change.keyType, &EditorSoundChoice::keyType);
            const auto label = found == choices.end() ? std::to_wstring(change.keyType) : found->label;
            description = std::to_wstring(change.position.measure + 1) + L" / " +
                          Wide(Fraction(change.position.fraction)) + L"    " + label + L"    " +
                          Wide(change.soundIndex);
        }
        Button({156, y, 1430, 40}, description, [this, row] { SelectEffectRow(row); });
        Button({1620, y, 195, 40}, std::wstring(Texts().remove), [this, row] { state_.RemoveEffect(row); });
    }
}
void EditorView::DrawEffectFields()
{
    const auto &text = Texts();
    auto &form = state_.effectForm;
    Field({156, 595, 188, 42}, text.startMeasure.data(), form.startMeasure);
    Field({370, 595, 188, 42}, text.startBeat.data(), form.startFraction);
    Field({584, 595, 188, 42}, text.endMeasureOptional.data(), form.endMeasure);
    Field({798, 595, 188, 42}, text.endBeatOptional.data(), form.endFraction);
    Button({1036, 595, 789, 42}, EffectLabel(form.selection), [this] { CycleEffectType(); });
    Field({156, 718, 319, 42},
          std::holds_alternative<EditorHitSoundTarget>(form.selection) ? text.hitSoundIndex.data()
                                                                       : text.startValue.data(),
          form.beginValue);
    Field({500, 718, 319, 42}, text.endValue.data(), form.endValue);
    Button({844, 718, 360, 42}, std::wstring(text.curves[static_cast<std::size_t>(form.curve)]), [this] {
        state_.effectForm.curve = static_cast<chart::AutomationCurve>(
            (static_cast<std::size_t>(state_.effectForm.curve) + 1) % Texts().curves.size());
        state_.RequestRebuild();
    });
    Field({1230, 718, 245, 42}, text.audioBus.data(), form.audioBus);
    Button({1508, 718, 317, 42}, std::wstring(text.addOrUpdateEffect), [this] { state_.ApplyEffect(); }, true);
}
