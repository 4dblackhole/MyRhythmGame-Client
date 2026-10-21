#pragma once
#include "GameFlow/GameplayLaunchRequest.h"
#include "IEditorAudioSource.h"
#include "IEditorContext.h"
#include "PhantomCore.h"
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
    virtual void Box(Rect, Color, float radius = 0) = 0;
    virtual void Text(Rect, std::wstring, float size = 20,
                      Color color = {.17F, .34F, .47F, 1}) = 0;
    virtual void Image(Rect, const std::filesystem::path &, Color) = 0;
    virtual void Button(Rect, std::wstring, std::function<void()>, bool selected = false) = 0;
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
    virtual std::unique_ptr<IEditorDocument> OpenDocument(
        const GameplayLaunchRequest &) const = 0;
    virtual void SelectTool(int) = 0;
    virtual void HandleToolKeys(IEditorContext &, const InputState &)
    {
    }
    virtual void PlaceNote(IEditorContext &, MusicalPosition) = 0;
    virtual bool HasPendingPlacement() const noexcept = 0;
    virtual bool CancelInteraction() noexcept = 0;
    virtual void CloseToolMenu() noexcept = 0;
    virtual bool OpenToolMenu(Point) = 0;
    virtual void DrawTools(IEditorModeCanvas &, IEditorContext &, const TextCatalog &) = 0;
    virtual void DrawChart(IEditorModeCanvas &, IEditorContext &, const TextCatalog &) = 0;
    virtual void DrawToolMenu(IEditorModeCanvas &, IEditorContext &, const TextCatalog &) = 0;
    virtual void EditScore(IEditorContext &, Point, bool erase) = 0;
    virtual void ScrollScore(IEditorContext &, int direction) = 0;
    virtual bool BeginScoreSelection(IEditorContext &, Point)
    {
        return false;
    }
    virtual bool UpdateScoreSelection(IEditorContext &, Point, bool)
    {
        return false;
    }
    virtual void DrawMetadata(IEditorModeCanvas &, IEditorContext &, const TextCatalog &) = 0;
    virtual std::vector<EditorSoundChoice> SoundChoices(const TextCatalog &) const = 0;
    virtual std::wstring_view SoundEffectHelp(const TextCatalog &) const = 0;
};
