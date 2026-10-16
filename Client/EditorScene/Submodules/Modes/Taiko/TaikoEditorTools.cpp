#include "../../EditorSupport.h"
#include "TaikoEditorMode.h"
#include "TaikoEditorTool.h"
#include "Texts/EditorScene/Taiko/TaikoEditorTexts.h"
#include <algorithm>
using namespace editor_ui;

namespace
{
    constexpr std::array<std::uint16_t, 6> ToolKeys{'W', 'E', 'R', 'T', 'Y', 'U'};

}

void TaikoEditorMode::ActivateGroup(IEditorContext &state, std::size_t group)
{
    int next = groupTools_.at(group);
    if (tool_ == next)
    {
        // Cycle only within the active set; returning to another set keeps its variant.
        next = editor_tools::NextInGroup(next);
    }
    state.SelectTool(next);
}

void TaikoEditorMode::HandleToolKeys(IEditorContext &state, const mrg::platform::InputState &input)
{
    if (input.WasKeyPressed('Q'))
    {
        HandleToolKey(state, 'Q');
        return;
    }
    for (const auto key : ToolKeys)
        if (input.WasKeyPressed(key))
            HandleToolKey(state, key);
}

bool TaikoEditorMode::HandleToolKey(IEditorContext &state, std::uint16_t key)
{
    if (key == 'Q')
    {
        state.SelectTool(0);
        return true;
    }
    const auto found = std::ranges::find(ToolKeys, key);
    if (found == ToolKeys.end())
        return false;
    ActivateGroup(state, static_cast<std::size_t>(found - ToolKeys.begin()));
    return true;
}

void TaikoEditorMode::DrawTools(IEditorModeCanvas &canvas, IEditorContext &state,
                                const finger_drum::texts::TextCatalog &catalog)
{
    const auto &text = finger_drum::texts::TaikoEditor(catalog);
    canvas.Box({24, 76, 70, 918}, Paper, 18);
    canvas.Text({38, 86, 55, 25}, std::wstring(finger_drum::texts::Editor(catalog).tools), 16);
    canvas.Button({35, 122, 48, 48}, L"", [&state] { state.SelectTool(0); }, tool_ == 0);
    canvas.Text({75, 125, 16, 18}, L"Q", 12);
    canvas.Text({27, 171, 67, 25}, std::wstring(text.toolGroups[0]), 13);
    for (std::size_t i = 0; i < groupTools_.size(); ++i)
    {
        const float y = 202 + static_cast<float>(i) * 80;
        const int id = groupTools_[i];
        canvas.Button({35, y, 48, 48}, L"", [this, &state, i] { ActivateGroup(state, i); }, tool_ == id);
        Circle(canvas, 59, y + 24, id, i == 1 ? 15.0F : 10.0F);
        canvas.Text({27, y + 49, 67, 25}, std::wstring(editor_tools::Name(id, text.toolVariants)), 11);
        canvas.Text({75, y + 3, 16, 18}, std::wstring(1, static_cast<wchar_t>(ToolKeys[i])), 12);
    }
    for (int i = 0; i < 2; ++i)
    {
        const int id = -2 - i;
        const float y = 682 + static_cast<float>(i) * 80;
        canvas.Button(
            {35, y, 48, 48}, std::wstring(text.toolGroups[5 + i]), [&state, id] { state.SelectTool(id); }, tool_ == id);
    }
}

void TaikoEditorMode::DrawToolMenu(IEditorModeCanvas &canvas, IEditorContext &state,
                                   const finger_drum::texts::TextCatalog &catalog)
{
    if (popup_ < 0)
        return;
    std::size_t row = 0;
    for (const auto &tool : editor_tools::Tools)
        if (static_cast<int>(tool.group) == popup_)
            canvas.Button(
                {105, 202 + static_cast<float>(row++) * 40, 220, 38},
                std::wstring(editor_tools::Name(tool.id, finger_drum::texts::TaikoEditor(catalog).toolVariants)),
                [&state, id = tool.id] { state.SelectTool(id); });
}
