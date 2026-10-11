#include "TaikoEditorMode.h"
#include "Parsing/ChartParser.h"
#include "../../EditorSupport.h"
#include "TaikoEditorTool.h"
#include "Texts/EditorScene/Taiko/TaikoEditorTexts.h"
#include <algorithm>
#include <limits>
using namespace editor_ui;

#include "Editing/ChartEditor.h"

namespace
{
    void ValidatePlacement(const chart::IEditorDocument &document, chart::MusicalPosition position,
                           int keyType, std::optional<chart::MusicalPosition> end = {})
    {
        const auto beginTime = document.Timeline().Compile(position);
        const auto endTime = end ? document.Timeline().Compile(*end) : beginTime;
        if (end && (*end <= position || endTime <= beginTime))
            throw std::invalid_argument("Long note end must follow its start in musical and playback time.");

        // The range query includes both endpoints of every intersecting long note.
        // New IDs follow existing IDs, matching the play session's equal-time order.
        auto notes = document.NotesInTimeRange(beginTime, endTime);
        notes.push_back({{position, keyType, end ? 1 : 0, {}, {}, {},
                          std::numeric_limits<std::size_t>::max() - 1}, beginTime});
        if (end)
            notes.push_back({{*end, keyType, 2, {}, {}, {}, std::numeric_limits<std::size_t>::max()}, endTime});
        std::ranges::sort(notes, [](const auto &a, const auto &b) {
            return std::pair{a.timing, a.note.sourceOrder} < std::pair{b.timing, b.note.sourceOrder};
        });
        std::optional<int> activeLongType;
        for (const auto &compiled : notes)
        {
            const auto &note = compiled.note;
            if (note.actionType == 1)
            {
                if (activeLongType)
                    throw std::invalid_argument("Taiko long notes cannot overlap another long note.");
                activeLongType = note.keyType;
            }
            else if (note.actionType == 2 && activeLongType == note.keyType)
                activeLongType.reset();
            else if (note.actionType == 0 && activeLongType)
                throw std::invalid_argument("A Taiko ordinary note cannot be placed inside a long note.");
        }
    }
}

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
    return std::make_unique<chart::ChartEditor>(std::move(p.document), std::move(e.document));
}

bool TaikoEditorMode::CancelInteraction() noexcept
{
    const bool hadInteraction = pending_.has_value() || popup_ >= 0;
    pending_.reset();
    popup_ = -1;
    return hadInteraction;
}
bool TaikoEditorMode::OpenToolMenu(v::Point point)
{
    if (point.x < 24 || point.x >= 94 || point.y < 202 || point.y >= 522)
        return false;
    popup_ = point.y < 282 ? 1 : point.y < 362 ? 2 : 3;
    pending_.reset();
    return true;
}

void TaikoEditorMode::SelectTool(int value)
{
    tool_ = value;
    if (const auto *selected = editor_tools::Find(value))
    {
        switch (selected->group)
        {
        case editor_tools::Group::Small:
            smallTool_ = value;
            break;
        case editor_tools::Group::Big:
            bigTool_ = value;
            break;
        case editor_tools::Group::Focus:
            focusTool_ = value;
            break;
        case editor_tools::Group::Roll:
            rollTool_ = value;
            break;
        }
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
            ValidatePlacement(state.Document(), std::min(*pending_, p), noteId, std::max(*pending_, p));
            state.AddNote(std::min(*pending_, p), noteId, std::max(*pending_, p), editor_tools::ExtraData(*selected));
            pending_.reset();
        }
    }
    else
    {
        ValidatePlacement(state.Document(), p, noteId);
        state.AddNote(p, noteId);
    }
    state.RequestRebuild();
}
