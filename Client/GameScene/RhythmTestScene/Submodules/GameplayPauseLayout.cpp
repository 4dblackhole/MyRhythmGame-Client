#include "GameplayPauseMenu.h"
#include "App/AssetPaths.h"
#include "Presentation/SkinVisualComponent.h"
#include "Presentation/UiColors.h"
#include "Texts/GameScene/RhythmTestScene/PauseTexts.h"
#include <algorithm>
#include <format>

namespace
{

    // Penpot is authored at 1920x1080; the gameplay Canvas is 1280x720.
    constexpr float Scale = 2.0F / 3.0F;
    constexpr float MenuWidth = 720 * Scale, MenuHeight = 336 * Scale;
    constexpr float RowHeight = 72 * Scale, RowStep = 88 * Scale;
    constexpr float Border = 2 * Scale;

    Visual2DNode &Panel(Visual2DNode &parent, Rect bounds, Color color,
                           float radius, std::string name)
    {
        auto &node = CreatePanel(parent, bounds, std::move(name));
        auto &sprite = *node.GetComponent<SpriteVisualComponent>();
        sprite.SetTint(color);
        sprite.SetCornerRadius(radius);
        return node;
    }

    void Arrow(Visual2DNode &button, int direction)
    {
        // The Penpot chevron has two 6x6 diagonal strokes with round caps.
        auto &icon = button.CreateChild("Pause.Arrow");
        icon.SetPosition({16 * Scale, 13 * Scale});
        for (const float sign : {-1.0F, 1.0F})
        {
            auto &stroke = Panel(icon, {0, 0, 8.485281F * Scale, 2.5F * Scale},
                                 ui_colors::pause::Focus, 1.25F * Scale, "Pause.ArrowStroke");
            stroke.SetPivot({.5F, .5F});
            stroke.SetPosition({6 * Scale, (9 + sign * 3) * Scale});
            stroke.Transform().SetRotationRollPitchYaw(0, 0, -direction * sign * DirectX::XM_PIDIV4);
        }
    }
} // namespace

Visual2DNode &GameplayPauseMenu::Label(Visual2DNode &parent, Rect bounds, std::wstring text,
                                          float size, Color color)
{
    auto &node = CreateLabel(parent, bounds, std::move(text), "Pause.Label");
    auto &component = *node.GetComponent<TextVisualComponent>();
    component.SetFont({TextFontSource::File, NotoSansKR().wstring()});
    component.SetFontSize(size);
    component.SetFontWeight(700);
    component.SetTextColor(color);
    node.SetClipRect({0, 0, bounds.width, bounds.height});
    return node;
}

void GameplayPauseMenu::Build()
{
    auto &canvas = *handle_.Get();
    router_.Reset(canvas);
    if (rootId_)
        static_cast<void>(canvas.RemoveNode(rootId_));
    const auto size = canvas.LogicalSize();
    auto &root = canvas.CreateNode(Anchor::Center, "Pause.Root");
    rootId_ = root.Id();
    // Image sprites use the engine's alpha-compositing path on screen. Keep
    // this translucent backdrop separate from the opaque panel primitives.
    static_cast<void>(CreateSprite(root, {-size.width / 2, -size.height / 2, size.width, size.height},
        visuals_.RegisterImage(InGame(L"PauseBackdrop")), "Pause.Dim"));
    auto &menu = root.CreateChild("Pause.Panel");
    menu.SetSize({MenuWidth, MenuHeight});
    menu.SetPivot({.5F, .5F});
    const float scale = std::min(1.0F, std::max(size.width - 32, 1.0F) / MenuWidth);
    menu.Transform().SetScale(scale, scale, 1);
    // Custom skins may still provide an overall panel; the default has none.
    const auto panelPath = InGame(L"PausePanel");
    if (!panelPath.empty())
        static_cast<void>(CreateSprite(menu, {0, 0, MenuWidth, MenuHeight}, visuals_.RegisterImage(panelPath),
                                         "Pause.SkinPanel"));
    for (std::size_t i = 0; i < rows_.size(); ++i)
        BuildRow(menu, i);
    textRevision_ = texts_.Revision();
    RefreshRows();
    router_.InvalidateHitTest();
}

void GameplayPauseMenu::BuildRow(Visual2DNode &parent, std::size_t index)
{
    auto &frame = Panel(parent, {0, MenuHeight - RowHeight - RowStep * static_cast<float>(index), MenuWidth, RowHeight},
                        ui_colors::pause::Focus, 20 * Scale, "Pause.Row." + std::to_string(index));
    frames_[index] = frame.Id();
    auto &fill = Panel(frame, {Border, Border, MenuWidth - 2 * Border, RowHeight - 2 * Border},
                       ui_colors::pause::Surface, 18 * Scale, "Pause.RowFill");
    fills_[index] = fill.Id();
    constexpr std::array keys{"PauseResume", "PauseRewind", "PauseRetry", "PauseRetire"};
    BindSkinImage(frame, visuals_, "InGame", keys[index], "Pause.RowFill");
    auto &action = CreateButton(frame, {0, 0, index < 2 ? 436 * Scale : MenuWidth, RowHeight}, L"",
                                   std::format("Pause.{}", index));
    VisualStyle transparent;
    transparent.normal = transparent.hovered = transparent.pressed = transparent.disabled = {0, 0, 0, 0};
    action.GetComponent<SpriteVisualComponent>()->SetStyle(transparent);
    rows_[index] = action.Id();
    captions_[index] = Label(action, {28 * Scale, 14 * Scale, (index < 2 ? 384 : 664) * Scale, 44 * Scale},
                             std::wstring(Pause(texts_).actions[index]), 26 * Scale,
                             ui_colors::pause::Ink).Id();
    if (index < 2)
        BuildSelector(frame, index);
}

void GameplayPauseMenu::BuildSelector(Visual2DNode &parent, std::size_t index)
{
    auto &selector = Panel(parent, {452 * Scale, 12 * Scale, 252 * Scale, 48 * Scale},
                           ui_colors::pause::Keyline, 12 * Scale, "Pause.Selector");
    Panel(selector, {Border, Border, 248 * Scale, 44 * Scale}, ui_colors::White, 10 * Scale, "Pause.SelectorFill");
    for (const int direction : {-1, 1})
    {
        auto &button = CreateButton(selector, {(direction < 0 ? 2 : 206) * Scale, Border, 44 * Scale, 44 * Scale},
                                       L"", std::format("Pause.{}.{}", index, direction < 0 ? "Previous" : "Next"));
        auto &sprite = *button.GetComponent<SpriteVisualComponent>();
        VisualStyle style;
        style.normal = ui_colors::pause::Selector;
        style.hovered = ui_colors::sky::Focus;
        style.pressed = ui_colors::pause::Keyline;
        style.disabled = ui_colors::pause::Disabled;
        sprite.SetStyle(style);
        sprite.SetCornerRadius(10 * Scale);
        Arrow(button, direction);
        (direction < 0 ? previous_ : next_)[index] = button.Id();
    }
    auto &value = Label(selector, {48 * Scale, Border, 156 * Scale, 44 * Scale}, L"", 18 * Scale, ui_colors::pause::Ink);
    value.GetComponent<TextVisualComponent>()->SetHorizontalAlignment(TextAlignment::Center);
    values_[index] = value.Id();
}
