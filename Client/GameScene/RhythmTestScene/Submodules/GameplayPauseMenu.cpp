#include "GameplayPauseMenu.h"
#include "Presentation/UiColors.h"
#include "Texts/GameScene/RhythmTestScene/PauseTexts.h"
#include <algorithm>
#include <format>
#include <stdexcept>
namespace v = mrg::visual2d;
namespace
{
    namespace colors = finger_drum::presentation::ui_colors;
    constexpr auto White = colors::White, Muted = colors::pause::Muted;
    constexpr std::array<GameplayPauseAction, 4> Actions{GameplayPauseAction::Resume, GameplayPauseAction::Rewind,
                                                         GameplayPauseAction::Retry, GameplayPauseAction::Retire};
} // namespace
void GameplayPauseMenu::Show(std::uint32_t width, std::uint32_t height, bool canResume, std::size_t measureCount)
{
    const bool changed = canResume_ != canResume;
    canResume_ = canResume;
    maxMeasures_ = std::max(std::size_t{4}, measureCount);
    if (visible_)
    {
        if (changed)
            RefreshRows();
        return;
    }
    width_ = width;
    height_ = height;
    if (!handle_)
        handle_ = visuals_.CreateOwnedCanvas({{1280, 720}, v::CanvasScaleMode::FixedHeight, {}, 10});
    if (!handle_)
        throw std::runtime_error("Failed to create pause menu Canvas.");
    handle_.Get()->SetViewportSize({static_cast<float>(width), static_cast<float>(height)});
    visible_ = true;
    focus_ = 0;
    lastPointer_.reset();
    Build();
    static_cast<void>(handle_.SetVisible(true));
}
v::Visual2DNode &GameplayPauseMenu::Label(v::Visual2DNode &parent, v::Rect bounds, std::wstring text, float size,
                                          v::Color color)
{
    auto &node = v::CreateLabel(parent, bounds, std::move(text), "Pause.Label");
    auto &component = *node.GetComponent<v::TextVisualComponent>();
    texts_.ApplyFont(component);
    component.SetFontSize(size);
    component.SetTextColor(color);
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
    auto &dim = v::CreatePanel(root, {-size.width / 2, -size.height / 2, size.width, size.height}, "Pause.Dim");
    dim.GetComponent<v::SpriteVisualComponent>()->SetTint({0, 0, 0, .7F});
    const float width = std::min(500.0F, std::max(220.0F, size.width - 32));
    auto &panel = v::CreatePanel(root, {-width / 2, -250, width, 500}, "Pause.Panel");
    auto &visual = *panel.GetComponent<v::SpriteVisualComponent>();
    visual.SetTint(colors::pause::Surface);
    visual.SetCornerRadius(20);
    const auto &text = finger_drum::texts::Pause(texts_);
    Label(panel, {24, 444, width - 48, 40}, std::wstring(text.title), 28, White);
    Label(panel, {24, 414, width - 48, 24}, std::wstring(text.practice), 15, Muted);
    for (std::size_t i = 0; i < rows_.size(); ++i)
    {
        auto &row = v::CreateButton(panel, {24, 320 - static_cast<float>(i) * 78, width - 48, 68}, L"",
                                    std::format("Pause.{}", i));
        row.GetComponent<v::SpriteVisualComponent>()->SetCornerRadius(10);
        Label(row, {16, 34, width - 80, 28}, std::wstring(text.actions[i]), 20, White);
        descriptions_[i] = Label(row, {16, 6, width - 80, 24}, L"", 13, Muted).Id();
        rows_[i] = row.Id();
    }
    Label(panel, {24, 18, width - 48, 26}, std::wstring(text.controls), 13, Muted);
    textRevision_ = texts_.Revision();
    RefreshRows();
    router_.InvalidateHitTest();
}
void GameplayPauseMenu::RefreshRows()
{
    if (!handle_)
        return;
    const auto &text = finger_drum::texts::Pause(texts_);
    const double seconds = static_cast<double>(choice_.leadIn.count()) / 1'000'000;
    const std::array<std::wstring, 4> descriptions{
        canResume_ ? std::vformat(text.resumeFormat, std::make_wformat_args(seconds)) : std::wstring(text.preparing),
        canResume_ ? std::vformat(text.rewindFormat, std::make_wformat_args(choice_.measures))
                   : std::wstring(text.preparing),
        std::wstring(text.retry), std::wstring(text.retire)};
    for (std::size_t i = 0; i < rows_.size(); ++i)
    {
        auto *row = handle_.Get()->FindNode(rows_[i]);
        if (!row)
            continue;
        row->SetEnabled(i >= 2 || canResume_);
        v::VisualStyle style;
        style.normal = i == focus_ ? colors::pause::Focus : colors::pause::Row;
        style.hovered = colors::pause::Hover;
        style.pressed = colors::pause::Pressed;
        style.disabled = colors::pause::Disabled;
        row->GetComponent<v::SpriteVisualComponent>()->SetStyle(style);
        if (auto *label = handle_.Get()->FindNode(descriptions_[i]))
            label->GetComponent<v::TextVisualComponent>()->SetText(descriptions[i]);
    }
}
void GameplayPauseMenu::ProcessPointer(const mrg::platform::InputState &input)
{
    auto &canvas = *handle_.Get();
    const auto p = input.IsMouseInsideWindow()
                       ? v::MapScreenPointer(
                             {static_cast<float>(input.MousePositionX()), static_cast<float>(input.MousePositionY())},
                             {static_cast<float>(width_), static_cast<float>(height_)}, canvas)
                       : std::nullopt;
    if (p && (!lastPointer_ || p->x != lastPointer_->x || p->y != lastPointer_->y))
        for (std::size_t i = 0; i < rows_.size(); ++i)
        {
            const auto *row = canvas.FindNode(rows_[i]);
            if (!row)
                continue;
            const auto b = row->BoundsInCanvas();
            if (p->x >= b.x && p->x <= b.x + b.width && p->y >= b.y && p->y <= b.y + b.height)
                focus_ = i;
        }
    lastPointer_ = p;
    v::PointerInput pointer;
    pointer.available = p.has_value();
    pointer.position = p.value_or(v::Point{});
    pointer.leftButtonDown = input.IsMouseButtonDown(mrg::platform::MouseButton::Left);
    pointer.leftButtonPressed = input.WasMouseButtonPressed(mrg::platform::MouseButton::Left);
    pointer.leftButtonReleased = input.WasMouseButtonReleased(mrg::platform::MouseButton::Left);
    router_.Process(canvas, pointer);
}
GameplayPauseChoice GameplayPauseMenu::HandleKey(std::uint16_t key)
{
    GameplayPauseChoice result = choice_;
    result.action = GameplayPauseAction::None;
    if (!visible_ || !handle_)
        return result;
    const auto oldFocus = focus_;
    if (key == VK_UP)
        focus_ = (focus_ + 3) % 4;
    if (key == VK_DOWN)
        focus_ = (focus_ + 1) % 4;
    const int direction = static_cast<int>(key == VK_RIGHT) - static_cast<int>(key == VK_LEFT);
    if (focus_ == 0 && direction)
        choice_.leadIn = finger_drum::rhythm::RhythmDuration{
            std::clamp(choice_.leadIn.count() + direction * 500'000, 500'000LL, 3'000'000LL)};
    if (focus_ == 1 && direction)
    {
        if (direction < 0 && choice_.measures > 1)
            --choice_.measures;
        if (direction > 0 && choice_.measures < maxMeasures_)
            ++choice_.measures;
    }
    if (oldFocus != focus_ || result.leadIn != choice_.leadIn || result.measures != choice_.measures)
        RefreshRows();
    result = choice_;
    result.action = GameplayPauseAction::None;
    if (key == VK_RETURN && (focus_ >= 2 || canResume_))
        result.action = Actions[focus_];
    return result;
}
GameplayPauseChoice GameplayPauseMenu::Update(const mrg::platform::InputState &input)
{
    GameplayPauseChoice result = choice_;
    result.action = GameplayPauseAction::None;
    if (!visible_ || !handle_)
        return result;
    if (textRevision_ != texts_.Revision())
        Build();
    const auto oldFocus = focus_;
    ProcessPointer(input);
    if (oldFocus != focus_)
        RefreshRows();
    for (const auto key : {VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT, VK_RETURN})
        if (input.WasKeyPressed(static_cast<std::uint16_t>(key)))
            result = HandleKey(static_cast<std::uint16_t>(key));
    for (const auto &action : handle_.Get()->TakeActions())
        if (action.type == v::ActionType::Clicked)
            for (std::size_t i = 0; i < rows_.size(); ++i)
                if (action.source == rows_[i])
                {
                    focus_ = i;
                    result.action = Actions[i];
                }
    return result;
}
void GameplayPauseMenu::Hide() noexcept
{
    if (handle_)
        router_.Reset(*handle_.Get());
    static_cast<void>(handle_.SetVisible(false));
    visible_ = false;
    lastPointer_.reset();
}
void GameplayPauseMenu::OnResize(std::uint32_t width, std::uint32_t height)
{
    if (!width || !height)
        return;
    width_ = width;
    height_ = height;
    if (handle_)
    {
        handle_.Get()->SetViewportSize({static_cast<float>(width), static_cast<float>(height)});
        if (visible_)
            Build();
    }
}
void GameplayPauseMenu::Shutdown() noexcept
{
    Hide();
    handle_.Reset();
    rows_ = {};
    descriptions_ = {};
    rootId_ = 0;
}
