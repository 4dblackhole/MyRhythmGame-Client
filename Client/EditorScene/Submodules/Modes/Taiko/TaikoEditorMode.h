#pragma once
#include "../IEditorMode.h"
#include <optional>

class TaikoEditorMode final : public IEditorMode
{
  public:
    std::string_view Id() const noexcept override
    {
        return "Taiko";
    }
    std::unique_ptr<finger_drum::chart::IEditorDocument> OpenDocument(
        const finger_drum::GameplayLaunchRequest &) const override;
    void SelectTool(int) override;
    void PlaceNote(EditorWorkspace &, finger_drum::chart::MusicalPosition) override;
    bool HasPendingPlacement() const noexcept override
    {
        return pending_.has_value();
    }
    bool CancelInteraction() noexcept override;
    void CloseToolMenu() noexcept override
    {
        popup_ = -1;
    }
    bool OpenToolMenu(mrg::visual2d::Point) override;
    void DrawTools(IEditorModeCanvas &, EditorWorkspace &, finger_drum::texts::Language) override;
    void DrawChart(IEditorModeCanvas &, EditorWorkspace &) override;
    void DrawToolMenu(IEditorModeCanvas &, EditorWorkspace &, finger_drum::texts::Language) override;
    void EditScore(EditorWorkspace &, mrg::visual2d::Point, bool erase) override;
    void ScrollScore(EditorWorkspace &, int direction) override;
    void DrawMetadata(IEditorModeCanvas &, EditorWorkspace &, finger_drum::texts::Language) override;
    std::vector<EditorSoundChoice> SoundChoices(finger_drum::texts::Language) const override;
    std::wstring_view SoundEffectHelp(finger_drum::texts::Language) const override;
    std::map<std::string, std::filesystem::path> AudioFiles(const finger_drum::chart::IEditorDocument &,
                                                            finger_drum::GameplayLaunchRequest &) const override;
    std::vector<AudioMarker> AudioMarkers(const finger_drum::chart::IEditorDocument &) const override;

  private:
    static void Circle(IEditorModeCanvas &, float x, float y, int type, float radius);
    struct NoteHit
    {
        mrg::visual2d::Point point;
        std::size_t order;
    };
    std::vector<NoteHit> noteHits_;
    std::vector<std::pair<float, finger_drum::chart::MusicalPosition>> realtimeGrid_;
    std::optional<finger_drum::chart::MusicalPosition> pending_;
    int tool_{1}, popup_{-1};
    int smallTool_{1}, bigTool_{3}, rollTool_{11}, focusTool_{15};
};
