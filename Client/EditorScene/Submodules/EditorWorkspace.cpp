#include "EditorWorkspace.h"
#include "EditorSupport.h"
#include "Modes/EditorModeFactory.h"
using namespace editor_ui;

void EditorWorkspace::Initialize()
{
    if (!mode)
        mode = CreateEditorMode(request.mode);
    editor = mode->OpenDocument(request);
    if (!editor)
        throw std::runtime_error("Editor mode did not provide a document.");
}
void EditorWorkspace::UpdateAnalysis()
{
    if (analysis.Update(*editor, *mode, request, status))
        rebuild = true;
}

std::pair<double, double> EditorWorkspace::TimelineRangeMilliseconds() const
{
    double begin = 0, end = 1;
    for (const auto &note : editor->Notes())
    {
        const double milliseconds = note.timing.count() / 1000.0;
        begin = std::min(begin, milliseconds);
        end = std::max(end, milliseconds);
    }
    if (const auto music = analysis.Data().sounds.find("Music"); music != analysis.Data().sounds.end())
        end = std::max(end, music->second.durationSeconds * 1000.0);
    return {begin, end};
}

void EditorWorkspace::SelectTool(int value)
{
    mode->SelectTool(value);
    rebuild = true;
}

void EditorWorkspace::Save()
{
    editor->Save();
    const auto &effectPath = editor->Effects().sourcePath;
    request.effectPath = effectPath.empty() ? std::nullopt : std::optional{effectPath};
    status = "Saved: " + Utf8(editor->Pattern().sourcePath.wstring());
    rebuild = true;
}

void EditorWorkspace::PlaceNote(chart::MusicalPosition position)
{
    mode->PlaceNote(*this, position);
}
