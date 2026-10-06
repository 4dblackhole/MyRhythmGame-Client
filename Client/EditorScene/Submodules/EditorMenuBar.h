#pragma once
#include "MRG_Core.h"
#include "Texts/TextCatalog.h"
#include <cstdint>
#include <optional>

// EditorInput translates physical keys/screen coordinates into menu intent.
struct EditorMenuInput
{
    std::optional<mrg::visual2d::Point> pointer;
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
    void Initialize(mrg::visual2d::Visual2DCanvas &canvas, finger_drum::texts::TextCatalog &texts);
    void Resize();
    void Refresh(bool dirty);
    EditorMenuInputResult Update(const EditorMenuInput &input, bool dirty);
    void Shutdown() noexcept;

  private:
    bool UpdateKeyboard(const EditorMenuInput &input, EditorMenuInputResult &result);
    void UpdatePointer(const EditorMenuInput &input, EditorMenuInputResult &result);
    void SetOpen(bool open, bool keyboardSelection);
    void RefreshHighlight();

    mrg::visual2d::Visual2DCanvas *canvas_{};
    finger_drum::texts::TextCatalog *texts_{};
    mrg::visual2d::Visual2DNode *bar_{};
    mrg::visual2d::Visual2DNode *file_{};
    mrg::visual2d::Visual2DNode *popup_{};
    mrg::visual2d::Visual2DNode *save_{};
    mrg::visual2d::Visual2DNode *saveCaption_{};
    mrg::visual2d::Visual2DNode *shortcut_{};
    bool open_{};
    bool keyboardSelection_{};
    bool hoverFile_{};
    bool hoverSave_{};
    bool labelsInitialized_{};
    bool dirty_{};
    std::uint64_t textRevision_{};
};
