#include "GameplayPauseMenu.h"
#include "Presentation/SkinVisualComponent.h"
#include "Presentation/UiColors.h"
#include "Texts/GameScene/RhythmTestScene/PauseTexts.h"
#include <algorithm>
#include <format>
#include <stdexcept>
namespace v = mrg::visual2d;
namespace
{
    namespace colors = finger_drum::presentation::ui_colors;
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
void GameplayPauseMenu::RefreshRows()
{
    if (!handle_)
        return;
    const auto &text = finger_drum::texts::Pause(texts_);
    const double seconds = static_cast<double>(choice_.leadIn.count()) / 1'000'000;
    const std::array<std::wstring, 2> values{
        canResume_ ? std::vformat(text.resumeValueFormat, std::make_wformat_args(seconds)) : std::wstring(text.preparing),
        canResume_ ? std::vformat(text.rewindValueFormat, std::make_wformat_args(choice_.measures))
                   : std::wstring(text.preparing)};
    auto &canvas = *handle_.Get();
    for (std::size_t i = 0; i < rows_.size(); ++i)
    {
        const bool available = i >= 2 || canResume_;
        auto *row = canvas.FindNode(rows_[i]);
        auto *frame = canvas.FindNode(frames_[i]);
        if (!row || !frame)
            continue;
        row->SetEnabled(available);
        if (auto *fill = canvas.FindNode(fills_[i]))
            fill->GetComponent<v::SpriteVisualComponent>()->SetTint(
                available ? (i == focus_ ? colors::pause::Focus : colors::pause::Surface) : colors::pause::Disabled);
        if (auto *caption = canvas.FindNode(captions_[i]))
            caption->GetComponent<v::TextVisualComponent>()->SetTextColor(
                i == focus_ && available ? colors::White : colors::pause::Ink);
        if (i < values_.size())
        {
            if (auto *value = canvas.FindNode(values_[i]))
                value->GetComponent<v::TextVisualComponent>()->SetText(values[i]);
            for (const auto id : {previous_[i], next_[i]})
                if (auto *arrow = canvas.FindNode(id))
                    arrow->SetEnabled(available);
        }
    }
}

void GameplayPauseMenu::AdjustValue(std::size_t index, int direction)
{
    if (index == 0)
        choice_.leadIn = finger_drum::rhythm::RhythmDuration{
            std::clamp(choice_.leadIn.count() + direction * 500'000, 500'000LL, 3'000'000LL)};
    if (index == 1)
    {
        if (direction < 0 && choice_.measures > 1)
            --choice_.measures;
        if (direction > 0 && choice_.measures < maxMeasures_)
            ++choice_.measures;
    }
}
void GameplayPauseMenu::ProcessActions(GameplayPauseChoice &result)
{
    const auto actions = handle_.Get()->TakeActions();
    for (const auto &action : actions)
    {
        if (action.type != v::ActionType::Clicked)
            continue;
        for (std::size_t i = 0; i < rows_.size(); ++i)
        {
            if (action.source != rows_[i] || (i < 2 && !canResume_))
                continue;
            focus_ = i;
            result = choice_;
            result.action = Actions[i];
        }
        for (std::size_t i = 0; i < values_.size(); ++i)
        {
            const int direction = static_cast<int>(action.source == next_[i]) -
                                  static_cast<int>(action.source == previous_[i]);
            if (!direction || !canResume_)
                continue;
            focus_ = i;
            AdjustValue(i, direction);
            result = choice_;
            result.action = GameplayPauseAction::None;
        }
    }
    if (!actions.empty())
        RefreshRows();
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
            const auto *row = canvas.FindNode(frames_[i]);
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
    if (direction && canResume_)
        AdjustValue(focus_, direction);
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
    ProcessActions(result);
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
    frames_ = {};
    fills_ = {};
    captions_ = {};
    values_ = {};
    previous_ = {};
    next_ = {};
    rootId_ = 0;
}
