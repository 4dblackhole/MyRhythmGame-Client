#include "OptionsPanelStyle.h"
#include "Presentation/OptionsPanel.h"
#include <algorithm>

using namespace options_ui;
namespace
{
    std::wstring KeyName(std::uint16_t key, const TextCatalog &catalog)
    {
        UINT scan = MapVirtualKeyW(key, MAPVK_VK_TO_VSC_EX);
        LONG flags = static_cast<LONG>((scan & 0xff) << 16);
        if ((scan & 0xff00) != 0)
            flags |= 1 << 24;
        wchar_t name[128]{};
        if (GetKeyNameTextW(flags, name, 128) > 0)
            return name;
        return std::wstring(catalog.Get("Options.virtualKeyPrefix")) + std::to_wstring(key);
    }
} // namespace
void OptionsPanel::CreateKeyControls()
{
    float top = 1148;
    for (const auto &layout : inputLayouts_)
    {
        modeLabels_.push_back(
            &CreateFieldLabel(*content_, contentHeight_ - top, PanelWidth, "Options.Keys." + layout.modeId));
        top += 52;
        for (const auto &binding : layout.bindings)
            for (std::size_t slot = 0; slot < binding.keys.size(); ++slot)
            {
                KeyRow row{layout.modeId, binding.id, binding.displayName, slot};
                const auto name = "Options.Key." + row.mode + "." + row.binding + "." + std::to_string(slot);
                row.label = &CreateFieldLabel(*content_, contentHeight_ - top, PanelWidth, name + ".Label");
                row.label->GetComponent<TextVisualComponent>()->SetFontSize(15);
                row.button = &CreateButton(
                    *content_, {24, contentHeight_ - top - 46, PanelWidth - 48, 38}, L"", name);
                VisualStyle style{};
                style.normal = FieldColor;
                style.hovered = FieldHoverColor;
                style.pressed = ui_colors::options::Pressed;
                style.disabled = ui_colors::options::Disabled;
                RequireComponent<SpriteVisualComponent>(*row.button).SetStyle(style);
                RequireComponent<TextVisualComponent>(*row.button).SetFontSize(18);
                RequireComponent<TextVisualComponent>(*row.button).SetTextColor(HeadingColor);
                keyRows_.push_back(std::move(row));
                top += 84;
            }
    }
}
void OptionsPanel::RefreshKeyControls()
{
    const auto &text = Options(texts_);
    std::size_t rowIndex = 0;
    for (std::size_t i = 0; i < inputLayouts_.size(); ++i)
    {
        auto &heading = RequireComponent<TextVisualComponent>(*modeLabels_[i]);
        heading.SetText(WideText(inputLayouts_[i].modeId) + L" · " + std::wstring(text.keys));
        texts_.ApplyFont(heading);
        const auto saved = options_.Values().modeKeys.find(inputLayouts_[i].modeId);
        const auto resolved = ResolveInputLayout(inputLayouts_[i], saved == options_.Values().modeKeys.end()
                                                                             ? InputKeyOverrides{}
                                                                             : saved->second);
        for (const auto &binding : resolved.bindings)
            for (std::size_t slot = 0; slot < binding.keys.size(); ++slot, ++rowIndex)
            {
                auto &row = keyRows_[rowIndex];
                auto name = InputBindingName(texts_, row.mode, row.binding);
                auto &label = RequireComponent<TextVisualComponent>(*row.label);
                label.SetText((name.empty() ? WideText(row.displayName) : std::wstring(name)) + L" · " +
                              std::wstring(slot == 0 ? text.primary : text.auxiliary));
                texts_.ApplyFont(label);
                auto &button = RequireComponent<TextVisualComponent>(*row.button);
                button.SetText(captureRow_ == rowIndex ? std::wstring(text.pressKey)
                                                       : KeyName(binding.keys[slot], texts_));
                texts_.ApplyFont(button);
            }
    }
}
void OptionsPanel::CaptureKey(std::uint16_t key)
{
    const auto &row = keyRows_.at(*captureRow_);
    auto candidate = options_.Values();
    const auto layout = std::ranges::find(inputLayouts_, row.mode, &ModeInputLayout::modeId);
    const auto saved = candidate.modeKeys.find(row.mode);
    auto resolved = ResolveInputLayout(*layout, saved == candidate.modeKeys.end() ? InputKeyOverrides{}
                                                                                        : saved->second);
    const auto binding = std::ranges::find(resolved.bindings, row.binding, &ModeInputBinding::id);
    binding->keys[row.slot] = key;
    candidate.modeKeys[row.mode][row.binding] = binding->keys;
    // OptionSettings validates conflicts against all other default/changed keys.
    if (SaveOptions(candidate))
        captureRow_.reset();
    RefreshKeyControls();
    RefreshStatus();
}
bool OptionsPanel::ProcessFieldAction(const Action &action)
{
    if (action.type == ActionType::SelectionChanged && action.source == resolutionCombo_->Id())
    {
        CancelKeyboardEdit();
        SelectResolution(action.selectedIndex);
        return true;
    }
    if (action.type != ActionType::Clicked)
        return false;
    if (action.source == songsField_->Id())
    {
        CancelKeyboardEdit();
        ChooseSongsFolder();
        return true;
    }
    if (action.source == songsDefault_->Id())
    {
        auto candidate = options_.Values();
        candidate.songsPath.clear();
        SaveOptions(candidate);
        RefreshSongsControls();
        RefreshStatus();
        return true;
    }
    for (std::size_t i = 0; i < keyRows_.size(); ++i)
        if (action.source == keyRows_[i].button->Id())
        {
            CancelKeyboardEdit();
            captureRow_ = i;
            RefreshKeyControls();
            return true;
        }
    if (action.source == offsetField_->Id())
    {
        CancelKeyboardEdit();
        offsetDraft_ = std::to_wstring(options_.Values().globalOffsetMs);
        editingOffset_ = replaceOffsetDraft_ = true;
        RefreshOffsetControls();
        return true;
    }
    return false;
}
