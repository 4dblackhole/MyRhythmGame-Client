#pragma once
#include "PhantomCore.h"
#include "Texts/TextCatalog.h"
#include <cstdint>
#include <optional>

// EditorInput translates physical keys/screen coordinates into menu intent.
struct EditorMenuInput
{
    std::optional<Point> pointer;
    bool leftPressed{};
    bool rightPressed{};
    bool toggle{};
    bool accept{};
    bool dismiss{};
    bool select{};
};

struct EditorMenuInputResult
{
    bool saveRequested{};
    bool consumeKeyboard{};
    bool consumePointer{};
};

// Canvas owns the nodes; this view owns menu state only. It returns the Save
// intent to EditorView and never accesses a document or changes scenes.
class EditorMenuBar final
{
  public:
    static constexpr float Height = 28.0F;
    void Initialize(Visual2DCanvas &canvas, TextCatalog &texts);
    void Resize();
    void Refresh(bool dirty);
    EditorMenuInputResult Update(const EditorMenuInput &input, bool dirty);
    void Shutdown() noexcept;

  private:
    bool UpdateKeyboard(const EditorMenuInput &input, EditorMenuInputResult &result);
    void UpdatePointer(const EditorMenuInput &input, EditorMenuInputResult &result);
    void SetOpen(bool open, bool keyboardSelection);
    void RefreshHighlight();

    Visual2DCanvas *canvas_{};
    TextCatalog *texts_{};
    Visual2DNode *bar_{};
    Visual2DNode *file_{};
    Visual2DNode *popup_{};
    Visual2DNode *save_{};
    Visual2DNode *saveCaption_{};
    Visual2DNode *shortcut_{};
    bool open_{};
    bool keyboardSelection_{};
    bool hoverFile_{};
    bool hoverSave_{};
    bool labelsInitialized_{};
    bool dirty_{};
    std::uint64_t textRevision_{};
};
