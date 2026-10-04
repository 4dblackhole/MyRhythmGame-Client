#include "../../EditorSupport.h"
#include "TaikoEditorMode.h"
#include "TaikoEditorTool.h"
#include "Texts/EditorScene/Taiko/TaikoEditorTexts.h"
#include <algorithm>
using namespace editor_ui;

void TaikoEditorMode::DrawTools(IEditorModeCanvas &canvas, IEditorContext &state, finger_drum::texts::Language language)
{
    const auto &text = finger_drum::texts::TaikoEditor(language);
    canvas.Box({24, 76, 70, 918}, Paper, 18);
    canvas.Text({38, 86, 55, 25}, std::wstring(finger_drum::texts::Editor(language).tools), 16);
    const auto toolName = [&text](const int id) -> std::wstring {
        for (std::size_t index = 0; index < editor_tools::Tools.size(); ++index)
        {
            if (editor_tools::Tools[index].id == id)
                return std::wstring(text.toolVariants[index]);
        }
        return {};
    };
    const std::array<std::wstring, 7> names{std::wstring(text.toolGroups[0]),
                                            toolName(smallTool_),
                                            toolName(bigTool_),
                                            toolName(rollTool_),
                                            toolName(focusTool_),
                                            std::wstring(text.toolGroups[5]),
                                            std::wstring(text.toolGroups[6])};
    const std::array<int, 7> ids{0, smallTool_, bigTool_, rollTool_, focusTool_, -2, -3};
    for (std::size_t i = 0; i < ids.size(); ++i)
    {
        const float y = 122 + static_cast<float>(i) * 80;
        canvas.Button(
            {35, y, 48, 48}, i < 5 ? L"" : names[i], [&state, id = ids[i]] { state.SelectTool(id); }, tool_ == ids[i]);
        if (i > 0 && i < 5)
            Circle(canvas, 59, y + 24, ids[i], i == 2 ? 15.0F : 10.0F);
        canvas.Text({27, y + 49, 67, 25}, names[i], 13);
    }
}

void TaikoEditorMode::DrawToolMenu(IEditorModeCanvas &canvas, IEditorContext &state,
                                   finger_drum::texts::Language language)
{
    if (popup_ >= 0)
    {
        std::vector<std::pair<std::wstring, int>> choices;
        const auto &text = finger_drum::texts::TaikoEditor(language);
        for (std::size_t toolIndex = 0; toolIndex < editor_tools::Tools.size(); ++toolIndex)
        {
            const auto &tool = editor_tools::Tools[toolIndex];
            using Group = editor_tools::Group;
            const bool matches = popup_ == 1   ? tool.group == Group::Small
                                 : popup_ == 2 ? tool.group == Group::Big
                                               : tool.group == Group::Roll || tool.group == Group::Focus;
            if (matches)
                choices.emplace_back(text.toolVariants[toolIndex], tool.id);
        }
        for (std::size_t i = 0; i < choices.size(); ++i)
            canvas.Button({105, 200 + static_cast<float>(i) * 40, 220, 38}, choices[i].first,
                          [&state, id = choices[i].second] { state.SelectTool(id); });
    }
}
