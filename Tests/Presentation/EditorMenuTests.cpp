#include "EditorMenuTests.h"
#include "EditorScene/Submodules/EditorMenuBar.h"
#include <stdexcept>

namespace
{
    namespace v = mrg::visual2d;
    void Check(bool value, const char *message)
    {
        if (!value)
            throw std::runtime_error(message);
    }
    v::Visual2DNode *Find(v::Visual2DNode &node, std::string_view name)
    {
        if (node.Name() == name)
            return &node;
        for (const auto &child : node.Children())
            if (auto *found = Find(*child, name))
                return found;
        return nullptr;
    }
}

void TestEditorMenu()
{
    v::Visual2DCanvas canvas({1920, 1080});
    finger_drum::texts::TextCatalog texts;
    EditorMenuBar menu;
    menu.Initialize(canvas, texts);
    auto *bar = Find(canvas.Root(), "Editor.MenuBar");
    auto *file = Find(canvas.Root(), "Editor.Menu.File");
    auto *popup = Find(canvas.Root(), "Editor.Menu.Popup");
    auto *caption = Find(canvas.Root(), "Editor.Menu.SaveCaption");
    if (!bar || !file || !popup || !caption)
        throw std::runtime_error("Editor menu nodes are missing.");
    Check(popup->Children().size() == 1 && !popup->IsVisible(),
          "File menu must start closed and contain only Save.");
    Check(file->GetComponent<v::TextVisualComponent>()->Text() == L"파일" &&
              caption->GetComponent<v::TextVisualComponent>()->Text() == L"저장하기",
          "Korean menu labels are missing.");
    menu.Refresh(true);
    Check(caption->GetComponent<v::TextVisualComponent>()->Text() == L"저장하기 *",
          "Dirty state must be reflected in Save.");
    texts.SetLanguage(finger_drum::texts::Language::English);
    menu.Refresh(false);
    Check(file->GetComponent<v::TextVisualComponent>()->Text() == L"File" &&
              caption->GetComponent<v::TextVisualComponent>()->Text() == L"Save",
          "Menu must refresh language and remove the dirty marker.");

    // Exercise the production screen mapping at standard, narrow and wide sizes.
    for (const v::Size viewport : {v::Size{1920, 1080}, {1280, 720}, {900, 720}, {2560, 1080}})
    {
        canvas.SetViewportSize(viewport);
        menu.Resize();
        const auto bounds = bar->BoundsInCanvas();
        Check(bounds.width == canvas.LogicalSize().width && bounds.y + bounds.height == 540,
              "Menu must span the viewport and stay at its top edge.");
        const auto pointer = [&](float y) {
            return v::MapScreenPointer({20 * canvas.PixelScale(), y * canvas.PixelScale()}, viewport, canvas);
        };
        const auto opened = menu.Update({.pointer = pointer(10), .leftPressed = true}, false);
        Check(opened.consumePointer && opened.consumeKeyboard && !opened.saveRequested && popup->IsVisible(),
              "File click must open without editing behind the menu.");
        const auto idle = menu.Update({}, false);
        Check(idle.consumeKeyboard && idle.consumePointer && !idle.saveRequested,
              "Open menu must retain input ownership between events.");
        const auto saved = menu.Update({.pointer = pointer(45), .leftPressed = true}, false);
        Check(saved.saveRequested && saved.consumePointer && !popup->IsVisible(),
              "Save click must emit one intent and close the dropdown.");
        Check(!menu.Update({}, false).saveRequested, "Save must not repeat next frame.");
    }

    static_cast<void>(menu.Update({.toggle = true}, false));
    const auto dismissed = menu.Update({.dismiss = true}, false);
    Check(!dismissed.saveRequested && dismissed.consumeKeyboard && dismissed.consumePointer && !popup->IsVisible(),
          "Escape must close without leaving the editor.");
    static_cast<void>(menu.Update({.toggle = true}, false));
    static_cast<void>(menu.Update({.select = true}, false));
    const auto accepted = menu.Update({.accept = true}, false);
    Check(accepted.saveRequested && accepted.consumeKeyboard && !popup->IsVisible(),
          "Keyboard Save must be consumed once.");
    Check(!menu.Update({.accept = true}, false).saveRequested,
          "Closed menu must leave Ctrl+S to the existing editor handler.");
    static_cast<void>(menu.Update({.toggle = true}, false));
    const auto outside = menu.Update({.pointer = v::Point{600, -200}, .leftPressed = true}, false);
    Check(!outside.saveRequested && outside.consumePointer && outside.consumeKeyboard && !popup->IsVisible(),
          "Outside dismissal must not click through to a note, slider or tab.");
    const auto resumed = menu.Update({.pointer = v::Point{600, -200}, .leftPressed = true}, false);
    Check(!resumed.consumePointer && !resumed.consumeKeyboard, "Closed menu must release editor input.");
    static_cast<void>(menu.Update({.toggle = true}, false));
    const auto right = menu.Update({.pointer = v::Point{600, -200}, .rightPressed = true}, false);
    Check(right.consumePointer && !right.saveRequested && !popup->IsVisible(),
          "Right-click dismissal must not erase a note.");
    menu.Shutdown();
    Check(!bar->IsVisible() && !menu.Update({.toggle = true}, false).consumeKeyboard,
          "Shutdown must hide the menu and release Canvas observers.");
}
