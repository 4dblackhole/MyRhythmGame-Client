#pragma once
#include "GameFlow/GameplayLaunchRequest.h"
#include "IEditorAudioSource.h"
#include "IEditorContext.h"
#include "MRG_Core.h"
#include "Texts/TextCatalog.h"
#include <functional>
#include <map>
#include <memory>

// Drawing primitives supplied by the common view. A mode owns its layout,
// tool palette and hit testing; it never retains this per-build drawing target.
class IEditorModeCanvas
{
  public:
    virtual ~IEditorModeCanvas() = default;
    virtual void Box(mrg::visual2d::Rect, mrg::visual2d::Color, float radius = 0) = 0;
    virtual void Text(mrg::visual2d::Rect, std::wstring, float size = 20,
                      mrg::visual2d::Color color = {.17F, .34F, .47F, 1}) = 0;
    virtual void Button(mrg::visual2d::Rect, std::wstring, std::function<void()>, bool selected = false) = 0;
};

struct EditorSoundChoice
{
    int keyType{};
    std::wstring label;
    std::wstring effectLabel;
};

class IEditorMode : public IEditorAudioSource
{
  public:
    virtual ~IEditorMode() = default;
    virtual std::string_view Id() const noexcept = 0;
    virtual std::unique_ptr<finger_drum::chart::IEditorDocument> OpenDocument(
        const finger_drum::GameplayLaunchRequest &) const = 0;
    virtual void SelectTool(int) = 0;
    virtual void PlaceNote(IEditorContext &, finger_drum::chart::MusicalPosition) = 0;
    virtual bool HasPendingPlacement() const noexcept = 0;
    virtual bool CancelInteraction() noexcept = 0;
    virtual void CloseToolMenu() noexcept = 0;
    virtual bool OpenToolMenu(mrg::visual2d::Point) = 0;
    virtual void DrawTools(IEditorModeCanvas &, IEditorContext &, finger_drum::texts::Language) = 0;
    virtual void DrawChart(IEditorModeCanvas &, IEditorContext &) = 0;
    virtual void DrawToolMenu(IEditorModeCanvas &, IEditorContext &, finger_drum::texts::Language) = 0;
    virtual void EditScore(IEditorContext &, mrg::visual2d::Point, bool erase) = 0;
    virtual void ScrollScore(IEditorContext &, int direction) = 0;
    virtual void DrawMetadata(IEditorModeCanvas &, IEditorContext &, finger_drum::texts::Language) = 0;
    virtual std::vector<EditorSoundChoice> SoundChoices(finger_drum::texts::Language) const = 0;
    virtual std::wstring_view SoundEffectHelp(finger_drum::texts::Language) const = 0;
};
