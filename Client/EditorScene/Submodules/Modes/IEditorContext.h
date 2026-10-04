#pragma once
#include "Editing/IEditorDocument.h"

struct EditorScoreState
{
    int division{4};
    std::int64_t firstMeasure{};
    bool realtime{};
};

// Modes can inspect the score and submit edits, but cannot access the worker,
// launch request, other mode, or common view's internal rebuild flag.
class IEditorContext
{
  public:
    virtual ~IEditorContext() = default;
    virtual const finger_drum::chart::IEditorDocument &Document() const noexcept = 0;
    virtual const EditorScoreState &Score() const noexcept = 0;
    virtual void SetFirstMeasure(std::int64_t) = 0;
    virtual double TimeMilliseconds() const noexcept = 0;
    virtual void Seek(double milliseconds) = 0;
    virtual void SelectTool(int) = 0;
    virtual void SelectTimingPosition(finger_drum::chart::MusicalPosition) = 0;
    virtual void RequestRebuild() noexcept = 0;
    virtual void Replace(finger_drum::chart::PatternDocument, finger_drum::chart::EffectDocument) = 0;
    virtual void AddNote(finger_drum::chart::MusicalPosition, int keyType,
                         std::optional<finger_drum::chart::MusicalPosition> end = {},
                         std::vector<std::string> extra = {}) = 0;
    virtual void DeleteNote(std::size_t sourceOrder) = 0;
};
