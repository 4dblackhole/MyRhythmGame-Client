#pragma once
#include "../IEditorMode.h"
#include "TaikoEditorNoteInspector.h"
#include <array>
#include <cstdint>
#include <map>
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
    void HandleToolKeys(IEditorContext &, const mrg::platform::InputState &) override;
    bool HandleToolKey(IEditorContext &, std::uint16_t);
    void PlaceNote(IEditorContext &, finger_drum::chart::MusicalPosition) override;
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
    void DrawTools(IEditorModeCanvas &, IEditorContext &, finger_drum::texts::Language) override;
    void DrawChart(IEditorModeCanvas &, IEditorContext &, finger_drum::texts::Language) override;
    void DrawToolMenu(IEditorModeCanvas &, IEditorContext &, finger_drum::texts::Language) override;
    void EditScore(IEditorContext &, mrg::visual2d::Point, bool erase) override;
    void ScrollScore(IEditorContext &, int direction) override;
    void DrawMetadata(IEditorModeCanvas &, IEditorContext &, finger_drum::texts::Language) override;
    std::vector<EditorSoundChoice> SoundChoices(finger_drum::texts::Language) const override;
    std::wstring_view SoundEffectHelp(finger_drum::texts::Language) const override;
    std::map<std::string, std::filesystem::path> AudioFiles(const finger_drum::chart::IEditorDocument &,
                                                            const finger_drum::GameplayLaunchRequest &) const override;
    std::vector<AudioMarker> AudioMarkers(const finger_drum::chart::IEditorDocument &) const override;

  private:
    void DrawOverview(IEditorModeCanvas &, IEditorContext &, finger_drum::texts::Language);
    void DrawRealtime(IEditorModeCanvas &, IEditorContext &, finger_drum::texts::Language);
    void Circle(IEditorModeCanvas &, float x, float y, int type, float radius);
    const std::filesystem::path &SkinImage(std::wstring_view);
    std::uint64_t skinRevision_{};
    std::map<std::wstring, std::filesystem::path, std::less<>> skinImagePaths_;
    static int VisualTool(const finger_drum::chart::PatternNote &);
    std::wstring LongNoteCaption(const finger_drum::chart::IEditorDocument &, const finger_drum::chart::PatternNote &,
                                 const finger_drum::chart::PatternNote &, finger_drum::texts::Language) const;
    struct NoteHit
    {
        mrg::visual2d::Point point;
        std::size_t order;
    };
    struct NoteBodyHit
    {
        mrg::visual2d::Rect bounds;
        std::size_t order;
    };
    std::vector<NoteBodyHit> bodyHits_;
    TaikoEditorNoteInspector inspector_;
    std::vector<NoteHit> noteHits_;
    std::vector<std::pair<mrg::visual2d::Point, finger_drum::chart::MusicalPosition>> overviewGrid_;
    std::vector<std::pair<float, finger_drum::chart::MusicalPosition>> realtimeGrid_;
    std::optional<finger_drum::chart::MusicalPosition> pending_;
    int tool_{1}, popup_{-1};
    void ActivateGroup(IEditorContext &, std::size_t);
    std::array<int, 6> groupTools_{1, 3, 11, 12, 15, 17};
};
