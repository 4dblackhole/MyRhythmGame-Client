#include "TaikoEditorMode.h"
#include "../../EditorSupport.h"
#include "Parsing/ChartParser.h"
#include "TaikoEditorPlacement.h"
#include "TaikoEditorTool.h"
#include "Texts/EditorScene/Taiko/TaikoEditorTexts.h"
#include <algorithm>
#include <limits>
using namespace editor_ui;

#include "Editing/ChartEditor.h"


std::unique_ptr<chart::IEditorDocument> TaikoEditorMode::OpenDocument(
    const finger_drum::GameplayLaunchRequest &request) const
{
    chart::ChartParser parser;
    auto p = parser.ParsePatternFile(request.patternPath);
    chart::ParseResult<chart::EffectDocument> e;
    if (request.effectPath)
        e = parser.ParseEffectFile(*request.effectPath);
    else if (!p.document.effectFile.empty())
        e = parser.ParseEffectFile(request.patternPath.parent_path() / p.document.effectFile);
    if (!p.Succeeded() || !e.Succeeded())
        throw std::runtime_error("Editor could not parse the selected chart.");
    if (p.document.mode != Id())
        throw std::invalid_argument("Selected chart is not a Taiko chart.");
    return std::make_unique<chart::ChartEditor>(
        std::move(p.document), std::move(e.document),
        finger_drum::rhythm::RhythmDuration{static_cast<std::int64_t>(request.globalOffsetMs) * 1000});
}

bool TaikoEditorMode::CancelInteraction() noexcept
{
    const bool hadSelection = inspector_.Clear();
    const bool hadInteraction = pending_.has_value() || popup_ >= 0 || hadSelection || selectionAnchor_.has_value();
    selectionAnchor_.reset();
    selecting_ = false;
    pending_.reset();
    popup_ = -1;
    return hadInteraction;
}
bool TaikoEditorMode::OpenToolMenu(v::Point point)
{
    if (point.x < 24 || point.x >= 94 || point.y < 202 || point.y >= 682)
        return false;
    popup_ = static_cast<int>((point.y - 202) / 80);
    pending_.reset();
    return true;
}

void TaikoEditorMode::SelectTool(int value)
{
    selectionAnchor_.reset();
    selecting_ = false;
    tool_ = value;
    if (const auto *selected = editor_tools::Find(value))
    {
        groupTools_[static_cast<std::size_t>(selected->group)] = value;
    }
    pending_.reset();
    popup_ = -1;
}

void TaikoEditorMode::PlaceNote(IEditorContext &state, chart::MusicalPosition p)
{
    // Placing the first realtime endpoint must not pan the second-click grid.
    if (!state.Score().realtime || tool_ == 0)
        state.Seek(state.Document().Timeline().Compile(p).count() / 1000.0);
    if (tool_ == 0)
    {
        state.RequestRebuild();
        return;
    }
    if (tool_ < 0)
    {
        state.SelectTimingPosition(p);
        state.RequestRebuild();
        return;
    }
    const auto *selected = editor_tools::Find(tool_);
    if (!selected)
        throw std::invalid_argument("Unknown editor note tool.");
    const int noteId = static_cast<int>(selected->note);
    if (finger_drum::mode::FindTaikoNote(noteId)->longNote)
    {
        if (!pending_)
            pending_ = p;
        else
        {
            taiko_editor::ValidatePlacement(state.Document(), std::min(*pending_, p), noteId, std::max(*pending_, p));
            state.AddNote(std::min(*pending_, p), noteId, std::max(*pending_, p), editor_tools::ExtraData(*selected));
            pending_.reset();
        }
    }
    else
    {
        taiko_editor::ValidatePlacement(state.Document(), p, noteId);
        state.AddNote(p, noteId);
    }
    state.RequestRebuild();
}
