#pragma once
#include "../IEditorMode.h"
#include "TaikoEditorNoteInspector.h"
#include "Texts/EditorScene/Taiko/TaikoEditorTexts.h"
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
    std::unique_ptr<IEditorDocument> OpenDocument(
        const GameplayLaunchRequest &) const override;
    void SelectTool(int) override;
    void HandleToolKeys(IEditorContext &, const InputState &) override;
    bool HandleToolKey(IEditorContext &, std::uint16_t);
    void PlaceNote(IEditorContext &, MusicalPosition) override;
    bool HasPendingPlacement() const noexcept override
    {
        return pending_.has_value();
    }
    bool CancelInteraction() noexcept override;
    void CloseToolMenu() noexcept override
    {
        popup_ = -1;
        selectionAnchor_.reset();
        selecting_ = false;
    }
    bool OpenToolMenu(Point) override;
    void DrawTools(IEditorModeCanvas &, IEditorContext &, const TextCatalog &) override;
    void DrawChart(IEditorModeCanvas &, IEditorContext &, const TextCatalog &) override;
    void DrawToolMenu(IEditorModeCanvas &, IEditorContext &, const TextCatalog &) override;
    void EditScore(IEditorContext &, Point, bool erase) override;
    void ScrollScore(IEditorContext &, int direction) override;
    bool BeginScoreSelection(IEditorContext &, Point) override;
    bool UpdateScoreSelection(IEditorContext &, Point, bool released) override;
    void DrawMetadata(IEditorModeCanvas &, IEditorContext &, const TextCatalog &) override;
    std::vector<EditorSoundChoice> SoundChoices(const TextCatalog &) const override;
    std::wstring_view SoundEffectHelp(const TextCatalog &) const override;
    std::map<std::string, std::filesystem::path> AudioFiles(const IEditorDocument &,
                                                            const GameplayLaunchRequest &) const override;
    std::vector<AudioMarker> AudioMarkers(const IEditorDocument &) const override;

  private:
    std::optional<MusicalPosition> SnapPosition(IEditorContext &, Point) const;
    std::optional<MusicalPosition> selectionAnchor_;
    Point selectionStart_{};
    bool selecting_{};
    void DrawOverview(IEditorModeCanvas &, IEditorContext &, const TextCatalog &);
    void DrawRealtime(IEditorModeCanvas &, IEditorContext &, const TextCatalog &);
    void Circle(IEditorModeCanvas &, float x, float y, int type, float radius);
    const std::filesystem::path &SkinImage(std::wstring_view);
    std::uint64_t skinRevision_{};
    std::map<std::wstring, std::filesystem::path, std::less<>> skinImagePaths_;
    static int VisualTool(const PatternNote &);
    std::wstring LongNoteCaption(const IEditorDocument &, const PatternNote &,
                                 const PatternNote &,
                                 const TaikoEditorTextSet &) const;
    struct NoteHit
    {
        Point point;
        std::size_t order;
    };
    struct NoteBodyHit
    {
        Rect bounds;
        std::size_t order;
    };
    std::vector<NoteBodyHit> bodyHits_;
    TaikoEditorNoteInspector inspector_;
    std::vector<NoteHit> noteHits_;
    std::vector<std::pair<Point, MusicalPosition>> overviewGrid_;
    std::vector<std::pair<float, MusicalPosition>> realtimeGrid_;
    std::optional<MusicalPosition> pending_;
    int tool_{1}, popup_{-1};
    void ActivateGroup(IEditorContext &, std::size_t);
    std::array<int, 6> groupTools_{1, 3, 11, 12, 15, 17};
};
