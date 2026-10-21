#include "EditorMenuBar.h"
#include "EditorSupport.h"
#include <stdexcept>

namespace
{

    using namespace editor_ui;
    template <typename Component> Component &Require(Visual2DNode &node)
    {
        auto *component = node.GetComponent<Component>();
        if (component == nullptr)
            throw std::logic_error("Editor menu node is missing a component.");
        return *component;
    }
    void ConfigureText(Visual2DNode &node, TextCatalog &texts)
    {
        auto &label = Require<TextVisualComponent>(node);
        texts.ApplyFont(label);
        label.SetFontSize(18.0F);
        label.SetTextColor(Ink);
    }
    void Tint(Visual2DNode &node, const Color color)
    {
        Require<SpriteVisualComponent>(node).SetTint(color);
    }
}

void EditorMenuBar::Initialize(Visual2DCanvas &canvas, TextCatalog &texts)
{
    if (canvas_ != nullptr)
        throw std::logic_error("Editor menu is already initialized.");
    canvas_ = &canvas;
    texts_ = &texts;
    // Keep this sibling above the editor's batched content and slider nodes.
    bar_ = &CreatePanel(canvas.Root(), {}, "Editor.MenuBar");
    bar_->SetZIndex(100);
    Tint(*bar_, Paper);
    file_ = &CreateLabel(*bar_, {8, 0, 92, Height}, L"", "Editor.Menu.File");
    ConfigureText(*file_, texts);
    popup_ = &CreatePanel(*bar_, {8, -34, 240, 34}, "Editor.Menu.Popup");
    popup_->SetZIndex(10);
    Tint(*popup_, Ink);
    save_ = &CreatePanel(*popup_, {1, 1, 238, 32}, "Editor.Menu.Save");
    Tint(*save_, Paper);
    saveCaption_ = &CreateLabel(*save_, {12, 4, 145, 24}, L"", "Editor.Menu.SaveCaption");
    shortcut_ = &CreateLabel(*save_, {157, 4, 69, 24}, L"", "Editor.Menu.Shortcut");
    ConfigureText(*saveCaption_, texts);
    ConfigureText(*shortcut_, texts);
    Require<TextVisualComponent>(*shortcut_).SetHorizontalAlignment(TextAlignment::Trailing);
    Resize();
    Refresh(false);
    SetOpen(false, false);
}

void EditorMenuBar::Resize()
{
    if (canvas_ == nullptr)
        return;
    const auto size = canvas_->LogicalSize();
    bar_->SetBounds({-size.width * 0.5F, size.height * 0.5F - Height, size.width, Height});
}

void EditorMenuBar::Refresh(const bool dirty)
{
    if (canvas_ == nullptr || (labelsInitialized_ && dirty_ == dirty && textRevision_ == texts_->Revision()))
        return;
    const auto &text = Editor(*texts_);
    Require<TextVisualComponent>(*shortcut_).SetText(std::wstring(texts_->Get("Editor.shortcutSave")));
    Require<TextVisualComponent>(*file_).SetText(std::wstring(text.fileMenu));
    Require<TextVisualComponent>(*saveCaption_).SetText(std::wstring(text.menuSave) + (dirty ? L" *" : L""));
    ConfigureText(*file_, *texts_);
    ConfigureText(*saveCaption_, *texts_);
    ConfigureText(*shortcut_, *texts_);
    dirty_ = dirty;
    textRevision_ = texts_->Revision();
    labelsInitialized_ = true;
    RefreshHighlight();
}

bool EditorMenuBar::UpdateKeyboard(const EditorMenuInput &input, EditorMenuInputResult &result)
{
    if (input.toggle)
    {
        SetOpen(!open_, true);
        result.consumeKeyboard = result.consumePointer = true;
        return true;
    }
    if (!open_)
        return false;
    result.consumeKeyboard = result.consumePointer = true;
    if (input.accept)
    {
        result.saveRequested = true;
        SetOpen(false, false);
        return true;
    }
    else if (input.dismiss)
    {
        SetOpen(false, false);
        return true;
    }
    else if (input.select)
    {
        keyboardSelection_ = true;
        return true;
    }
    return false;
}

void EditorMenuBar::UpdatePointer(const EditorMenuInput &input, EditorMenuInputResult &result)
{
    const auto &point = input.pointer;
    const bool insideBar = point && bar_->BoundsInCanvas().Contains(*point);
    hoverFile_ = point && file_->BoundsInCanvas().Contains(*point);
    hoverSave_ = open_ && point && save_->BoundsInCanvas().Contains(*point);
    result.consumePointer = insideBar || open_;
    const bool left = input.leftPressed;
    const bool right = input.rightPressed;
    if (!left && !right)
        return;
    if (open_)
    {
        // Closing an overlay consumes this click so it cannot place a note or
        // trigger a tab/slider beneath the dropdown in the same update.
        result.consumeKeyboard = result.consumePointer = true;
        result.saveRequested = left && hoverSave_;
        SetOpen(false, false);
    }
    else if (left && hoverFile_)
    {
        result.consumeKeyboard = result.consumePointer = true;
        SetOpen(true, false);
    }
}

EditorMenuInputResult EditorMenuBar::Update(const EditorMenuInput &input, const bool dirty)
{
    EditorMenuInputResult result;
    if (canvas_ == nullptr)
        return result;
    Refresh(dirty);
    const bool wasOpen = open_;
    const bool wasFileHovered = hoverFile_;
    const bool wasSaveSelected = hoverSave_ || keyboardSelection_;
    const bool keyboardHandled = UpdateKeyboard(input, result);
    if (!keyboardHandled)
        UpdatePointer(input, result);
    if (wasOpen != open_ || wasFileHovered != hoverFile_ || wasSaveSelected != (hoverSave_ || keyboardSelection_))
        RefreshHighlight();
    return result;
}

void EditorMenuBar::SetOpen(const bool open, const bool keyboardSelection)
{
    open_ = open;
    keyboardSelection_ = keyboardSelection;
    hoverSave_ = false;
    popup_->SetVisible(open);
}

void EditorMenuBar::RefreshHighlight()
{
    Tint(*file_, open_ ? Blue : hoverFile_ ? Pale : Paper);
    Require<TextVisualComponent>(*file_).SetTextColor(open_ ? White : Ink);
    const bool selected = hoverSave_ || keyboardSelection_;
    Tint(*save_, selected ? Blue : Paper);
    Require<TextVisualComponent>(*saveCaption_).SetTextColor(selected ? White : Ink);
    Require<TextVisualComponent>(*shortcut_).SetTextColor(selected ? White : Ink);
}

void EditorMenuBar::Shutdown() noexcept
{
    if (bar_ != nullptr)
        bar_->SetVisible(false);
    canvas_ = nullptr;
    texts_ = nullptr;
    bar_ = file_ = popup_ = save_ = saveCaption_ = shortcut_ = nullptr;
    open_ = keyboardSelection_ = hoverFile_ = hoverSave_ = labelsInitialized_ = false;
}
