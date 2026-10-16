#include "GameplayPauseMenu.h"
#include "App/AssetPaths.h"
#include "Presentation/SkinVisualComponent.h"
#include "Presentation/UiColors.h"
#include "Texts/GameScene/RhythmTestScene/PauseTexts.h"
#include <algorithm>
#include <format>

namespace v = mrg::visual2d;
namespace
{
    namespace colors = finger_drum::presentation::ui_colors;
    // Penpot is authored at 1920x1080; the gameplay Canvas is 1280x720.
    constexpr float Scale = 2.0F / 3.0F;
    constexpr float MenuWidth = 720 * Scale, MenuHeight = 336 * Scale;
    constexpr float RowHeight = 72 * Scale, RowStep = 88 * Scale;
    constexpr float Border = 2 * Scale;

    v::Visual2DNode &Panel(v::Visual2DNode &parent, v::Rect bounds, v::Color color,
                           float radius, std::string name)
    {
        auto &node = v::CreatePanel(parent, bounds, std::move(name));
        auto &sprite = *node.GetComponent<v::SpriteVisualComponent>();
        sprite.SetTint(color);
        sprite.SetCornerRadius(radius);
        return node;
    }

    void Arrow(v::Visual2DNode &button, int direction)
    {
        // The Penpot chevron has two 6x6 diagonal strokes with round caps.
        auto &icon = button.CreateChild("Pause.Arrow");
        icon.SetPosition({16 * Scale, 13 * Scale});
        for (const float sign : {-1.0F, 1.0F})
        {
            auto &stroke = Panel(icon, {0, 0, 8.485281F * Scale, 2.5F * Scale},
                                 colors::pause::Focus, 1.25F * Scale, "Pause.ArrowStroke");
            stroke.SetPivot({.5F, .5F});
            stroke.SetPosition({6 * Scale, (9 + sign * 3) * Scale});
            stroke.Transform().SetRotationRollPitchYaw(0, 0, -direction * sign * DirectX::XM_PIDIV4);
        }
    }
} // namespace

v::Visual2DNode &GameplayPauseMenu::Label(v::Visual2DNode &parent, v::Rect bounds, std::wstring text,
                                          float size, v::Color color)
{
    auto &node = v::CreateLabel(parent, bounds, std::move(text), "Pause.Label");
    auto &component = *node.GetComponent<v::TextVisualComponent>();
    component.SetFont({v::TextFontSource::File, mrg_client::asset_paths::fonts::NotoSansKR().wstring()});
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
    auto &root = canvas.CreateNode(v::Anchor::Center, "Pause.Root");
    rootId_ = root.Id();
    // Image sprites use the engine's alpha-compositing path on screen. Keep
    // this translucent backdrop separate from the opaque panel primitives.
    static_cast<void>(v::CreateSprite(root, {-size.width / 2, -size.height / 2, size.width, size.height},
        visuals_.RegisterImage(mrg_client::asset_paths::skin::InGame(L"PauseBackdrop")), "Pause.Dim"));
    auto &menu = root.CreateChild("Pause.Panel");
    menu.SetSize({MenuWidth, MenuHeight});
    menu.SetPivot({.5F, .5F});
    const float scale = std::min(1.0F, std::max(size.width - 32, 1.0F) / MenuWidth);
    menu.Transform().SetScale(scale, scale, 1);
    // Custom skins may still provide an overall panel; the default has none.
    const auto panelPath = mrg_client::asset_paths::skin::InGame(L"PausePanel");
    if (!panelPath.empty())
        static_cast<void>(v::CreateSprite(menu, {0, 0, MenuWidth, MenuHeight}, visuals_.RegisterImage(panelPath),
                                         "Pause.SkinPanel"));
    for (std::size_t i = 0; i < rows_.size(); ++i)
        BuildRow(menu, i);
    textRevision_ = texts_.Revision();
    RefreshRows();
    router_.InvalidateHitTest();
}

void GameplayPauseMenu::BuildRow(v::Visual2DNode &parent, std::size_t index)
{
    auto &frame = Panel(parent, {0, MenuHeight - RowHeight - RowStep * static_cast<float>(index), MenuWidth, RowHeight},
                        colors::pause::Focus, 20 * Scale, "Pause.Row." + std::to_string(index));
    frames_[index] = frame.Id();
    auto &fill = Panel(frame, {Border, Border, MenuWidth - 2 * Border, RowHeight - 2 * Border},
                       colors::pause::Surface, 18 * Scale, "Pause.RowFill");
    fills_[index] = fill.Id();
    constexpr std::array keys{"PauseResume", "PauseRewind", "PauseRetry", "PauseRetire"};
    finger_drum::presentation::BindSkinImage(frame, visuals_, "InGame", keys[index], "Pause.RowFill");
    auto &action = v::CreateButton(frame, {0, 0, index < 2 ? 436 * Scale : MenuWidth, RowHeight}, L"",
                                   std::format("Pause.{}", index));
    v::VisualStyle transparent;
    transparent.normal = transparent.hovered = transparent.pressed = transparent.disabled = {0, 0, 0, 0};
    action.GetComponent<v::SpriteVisualComponent>()->SetStyle(transparent);
    rows_[index] = action.Id();
    captions_[index] = Label(action, {28 * Scale, 14 * Scale, (index < 2 ? 384 : 664) * Scale, 44 * Scale},
                             std::wstring(finger_drum::texts::Pause(texts_).actions[index]), 26 * Scale,
                             colors::pause::Ink).Id();
    if (index < 2)
        BuildSelector(frame, index);
}

void GameplayPauseMenu::BuildSelector(v::Visual2DNode &parent, std::size_t index)
{
    auto &selector = Panel(parent, {452 * Scale, 12 * Scale, 252 * Scale, 48 * Scale},
                           colors::pause::Keyline, 12 * Scale, "Pause.Selector");
    Panel(selector, {Border, Border, 248 * Scale, 44 * Scale}, colors::White, 10 * Scale, "Pause.SelectorFill");
    for (const int direction : {-1, 1})
    {
        auto &button = v::CreateButton(selector, {(direction < 0 ? 2 : 206) * Scale, Border, 44 * Scale, 44 * Scale},
                                       L"", std::format("Pause.{}.{}", index, direction < 0 ? "Previous" : "Next"));
        auto &sprite = *button.GetComponent<v::SpriteVisualComponent>();
        v::VisualStyle style;
        style.normal = colors::pause::Selector;
        style.hovered = colors::sky::Focus;
        style.pressed = colors::pause::Keyline;
        style.disabled = colors::pause::Disabled;
        sprite.SetStyle(style);
        sprite.SetCornerRadius(10 * Scale);
        Arrow(button, direction);
        (direction < 0 ? previous_ : next_)[index] = button.Id();
    }
    auto &value = Label(selector, {48 * Scale, Border, 156 * Scale, 44 * Scale}, L"", 18 * Scale, colors::pause::Ink);
    value.GetComponent<v::TextVisualComponent>()->SetHorizontalAlignment(v::TextAlignment::Center);
    values_[index] = value.Id();
}
