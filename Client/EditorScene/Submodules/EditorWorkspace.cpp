#include "EditorWorkspace.h"
#include "EditorSupport.h"
#include "EditorTool.h"
using namespace editor_ui;

void EditorWorkspace::Initialize()
{
    chart::ChartParser parser;
    auto p = parser.ParsePatternFile(request.patternPath);
    chart::ParseResult<chart::EffectDocument> e;
    if (request.effectPath)
        e = parser.ParseEffectFile(*request.effectPath);
    if (!p.Succeeded() || !e.Succeeded())
        throw std::runtime_error("Editor could not parse the selected chart.");
    editor = std::make_unique<chart::ChartEditor>(std::move(p.document), std::move(e.document));
}
void EditorWorkspace::UpdateAnalysis()
{
    if (analysis.Update(*editor, request, status))
        rebuild = true;
}

void EditorWorkspace::SelectTool(int value)
{
    tool = value;
    if (const auto *selected = editor_tools::Find(value))
    {
        switch (selected->group)
        {
        case editor_tools::Group::Small:
            smallTool = value;
            break;
        case editor_tools::Group::Big:
            bigTool = value;
            break;
        case editor_tools::Group::Focus:
            focusTool = value;
            break;
        case editor_tools::Group::Roll:
            rollTool = value;
            break;
        }
    }
    pending.reset();
    popup = -1;
    rebuild = true;
}

void EditorWorkspace::Save()
{
    editor->Save();
    request.effectPath = editor->Effects().sourcePath;
    status = "Saved: " + Utf8(editor->Pattern().sourcePath.wstring());
    rebuild = true;
}

void EditorWorkspace::PlaceNote(chart::MusicalPosition p)
{
    // Placing the first realtime endpoint must not pan the second-click grid.
    if (!realtime || tool == 0)
        timeMs = editor->Timeline().Compile(p).count() / 1000.0;
    if (tool == 0)
    {
        rebuild = true;
        return;
    }
    if (tool < 0)
    {
        timingFields[0] = std::to_string(p.measure + 1);
        timingFields[1] = Fraction(p.fraction);
        tab = 1;
        rebuild = true;
        return;
    }
    const auto *selected = editor_tools::Find(tool);
    if (!selected)
        throw std::invalid_argument("Unknown editor note tool.");
    const int noteId = static_cast<int>(selected->note);
    if (finger_drum::mode::FindTaikoNote(noteId)->longNote)
    {
        if (!pending)
            pending = p;
        else
        {
            editor->AddNote(std::min(*pending, p), noteId, std::max(*pending, p),
                            editor_tools::ExtraData(*selected));
            pending.reset();
        }
    }
    else
        editor->AddNote(p, noteId);
    rebuild = true;
}
