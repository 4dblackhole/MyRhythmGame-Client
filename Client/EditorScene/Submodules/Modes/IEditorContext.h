#pragma once
#include "Editing/IEditorDocument.h"
#include <set>
#include <span>

struct EditorScoreState
{
    int division{4};
    std::int64_t firstMeasure{};
    bool realtime{};
    static constexpr double MinimumRealtimeZoom = .25;
    static constexpr double MaximumRealtimeZoom = 4;
    double realtimeZoom{1};
};

// Modes can inspect the score and submit edits, but cannot access the worker,
// launch request, other mode, or common view's internal rebuild flag.
class IEditorContext
{
  public:
    virtual ~IEditorContext() = default;
    virtual const IEditorDocument &Document() const noexcept = 0;
    virtual const EditorScoreState &Score() const noexcept = 0;
    virtual void SetFirstMeasure(std::int64_t) = 0;
    virtual double TimeMilliseconds() const noexcept = 0;
    virtual void Seek(double milliseconds) = 0;
    virtual void SelectTool(int) = 0;
    virtual void SelectTimingPosition(MusicalPosition) = 0;
    virtual void RequestRebuild() noexcept = 0;
    virtual void Replace(PatternDocument, EffectDocument) = 0;
    virtual void AddNote(MusicalPosition, int keyType,
                         std::optional<MusicalPosition> end = {},
                         std::vector<std::string> extra = {}) = 0;
    virtual void UpdateNote(std::size_t, PatternNote,
                            std::optional<MusicalPosition>) = 0;
    virtual void DeleteNote(std::size_t sourceOrder) = 0;
    virtual const std::set<std::size_t> &SelectedNoteIds() const noexcept = 0;
    virtual void SelectNotes(std::span<const std::size_t>) = 0;
    virtual void SelectNoteRange(MusicalPosition, MusicalPosition) = 0;
    virtual const std::optional<std::pair<MusicalPosition, MusicalPosition>> &
    NoteSelectionRange() const noexcept = 0;
};
