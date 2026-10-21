#include "OptionsPanelStyle.h"
#include "Presentation/OptionsPanel.h"
#include <charconv>

using namespace options_ui;
void OptionsPanel::CreateOffsetControls()
{
    offsetLabel_ = &CreateFieldLabel(*content_, contentHeight_ - 822, PanelWidth, "Options.Offset.Label");
    offsetField_ = &CreateButton(*content_, {24, contentHeight_ - 880, PanelWidth - 48, 46}, L"",
                                                "Options.Offset.Field");
    VisualStyle style{};
    style.normal = FieldColor;
    style.hovered = FieldHoverColor;
    style.pressed = ui_colors::options::Pressed;
    style.disabled = ui_colors::options::Disabled;
    RequireComponent<SpriteVisualComponent>(*offsetField_).SetStyle(style);
    auto &field = RequireComponent<TextVisualComponent>(*offsetField_);
    field.SetFontSize(19);
    field.SetTextColor(HeadingColor);
    offsetHint_ = &CreateLabel(*content_, {24, contentHeight_ - 944, PanelWidth - 48, 54}, L"",
                                              "Options.Offset.Hint");
    auto &hint = RequireComponent<TextVisualComponent>(*offsetHint_);
    hint.SetFontSize(13);
    hint.SetTextColor(LabelColor);
}
void OptionsPanel::RefreshOffsetControls()
{
    const auto &text = Options(texts_);
    auto &label = RequireComponent<TextVisualComponent>(*offsetLabel_);
    label.SetText(std::wstring(text.globalOffset));
    texts_.ApplyFont(label);
    auto &hint = RequireComponent<TextVisualComponent>(*offsetHint_);
    hint.SetText(std::wstring(text.offsetHint));
    texts_.ApplyFont(hint);
    auto &field = RequireComponent<TextVisualComponent>(*offsetField_);
    field.SetText(editingOffset_ ? offsetDraft_ + L"_"
                                 : std::to_wstring(options_.Values().globalOffsetMs) +
                                       std::wstring(texts_.Get("Common.millisecondsSuffix")));
    texts_.ApplyFont(field);
}
void OptionsPanel::CancelKeyboardEdit()
{
    captureRow_.reset();
    editingOffset_ = false;
    replaceOffsetDraft_ = false;
    if (offsetField_)
        RefreshOffsetControls();
    if (!keyRows_.empty())
        RefreshKeyControls();
}
void OptionsPanel::ApplyOffset()
{
    std::string draft;
    draft.reserve(offsetDraft_.size());
    for (const auto character : offsetDraft_)
        draft.push_back(static_cast<char>(character)); // Numeric ASCII draft only.
    std::int32_t value{};
    const auto parsed = std::from_chars(draft.data(), draft.data() + draft.size(), value);
    if (parsed.ec != std::errc{} || parsed.ptr != draft.data() + draft.size())
    {
        status_ = Status::InvalidInput;
        statusDetail_ = "Offset must be an integer number of milliseconds within int32 range.";
    }
    else
    {
        auto candidate = options_.Values();
        candidate.globalOffsetMs = value;
        if (SaveOptions(candidate))
            editingOffset_ = false;
    }
    RefreshStatus();
}
void OptionsPanel::EditOffset(std::uint16_t key)
{
    if (key == VK_RETURN)
        ApplyOffset();
    else if (key == VK_BACK)
    {
        if (replaceOffsetDraft_)
            offsetDraft_.clear();
        else if (!offsetDraft_.empty())
            offsetDraft_.pop_back();
        replaceOffsetDraft_ = false;
    }
    else if (key == VK_DELETE)
    {
        offsetDraft_.clear();
        replaceOffsetDraft_ = false;
    }
    else
    {
        wchar_t character = 0;
        if (key >= '0' && key <= '9')
            character = static_cast<wchar_t>(key);
        else if (key >= VK_NUMPAD0 && key <= VK_NUMPAD9)
            character = static_cast<wchar_t>('0' + key - VK_NUMPAD0);
        else if (key == VK_OEM_MINUS || key == VK_SUBTRACT)
            character = L'-';
        if (character)
        {
            if (replaceOffsetDraft_)
            {
                offsetDraft_.clear();
                replaceOffsetDraft_ = false;
            }
            if (offsetDraft_.size() < 11 && (character != L'-' || offsetDraft_.empty()))
                offsetDraft_ += character;
        }
    }
    RefreshOffsetControls();
}
