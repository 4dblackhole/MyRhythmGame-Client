#pragma once
#include "../IEditorMode.h"
#include <cstdint>
#include <optional>
#include <string>

// Owns only the selected stable ID and an unapplied property draft. The
// document owns source notes; no rule/gameplay object is retained by the UI.
class TaikoEditorNoteInspector final
{
  public:
    void Select(const finger_drum::chart::IEditorDocument &, std::size_t);
    bool Clear() noexcept;
    bool IsSelected(std::size_t id) const noexcept;
    void Draw(IEditorModeCanvas &, IEditorContext &, finger_drum::texts::Language);
    void Apply(IEditorContext &);
    struct Draft
    {
        int tool{};
        // UI measures are one-based; beat fractions use the YMP whole-note unit.
        std::string startMeasure, startBeat, endMeasure, endBeat;
        std::string hitCount, tickDivision, hitSound;
    };
    Draft &Form() noexcept
    {
        draftChanged_ = true;
        return draft_;
    }

  private:
    void Refresh(const finger_drum::chart::IEditorDocument &);
    void Field(IEditorModeCanvas &, IEditorContext &, finger_drum::texts::Language, float y, std::wstring_view label,
               std::string &value, float x = 1530, float width = 340);
    void PositionFields(IEditorModeCanvas &, IEditorContext &, finger_drum::texts::Language, float y,
                        std::wstring_view label, std::string &measure, std::string &beat);
    std::optional<finger_drum::chart::EditorNoteSelection> selected_;
    std::uint64_t revision_{};
    Draft draft_;
    bool draftChanged_{};
};
