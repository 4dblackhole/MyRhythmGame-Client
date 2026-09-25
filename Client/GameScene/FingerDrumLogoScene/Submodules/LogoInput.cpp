#include "LogoView.h"
#include "LogoStyle.h"
#include "App/AssetPaths.h"
using namespace logo_ui;

void LogoView::ProcessPointer(const mrg::platform::InputState &input)
{
    canvasPointer_ =
        input.IsMouseInsideWindow()
            ? mrg::visual2d::MapScreenPointer(
                  {static_cast<float>(input.MousePositionX()),
                   static_cast<float>(input.MousePositionY())},
                  {static_cast<float>(width_), static_cast<float>(height_)}, *canvas_)
            : std::nullopt;

    mrg::visual2d::PointerInput pointer{};
    pointer.available = canvasPointer_.has_value();
    pointer.position = canvasPointer_.value_or(mrg::visual2d::Point{});
    pointer.leftButtonDown = input.IsMouseButtonDown(mrg::platform::MouseButton::Left);
    pointer.leftButtonPressed = input.WasMouseButtonPressed(mrg::platform::MouseButton::Left);
    pointer.leftButtonReleased = input.WasMouseButtonReleased(mrg::platform::MouseButton::Left);
    pointer.timestampTicks = LatestPointerTimestamp(input);
    inputRouter_.Process(*canvas_, pointer);
}

void LogoView::UpdateSelectionFromPointer(const mrg::platform::InputState &input)
{
    // A stationary mouse must not steal focus back after keyboard navigation.
    // Selection follows the pointer only when a new mouse event occurred.
    if (!HasPointerActivity(input))
    {
        return;
    }

    const mrg::visual2d::NodeId hovered = inputRouter_.HoveredNode();
    for (std::size_t index = 0; index < menuButtonIds_.size(); ++index)
    {
        if (hovered == menuButtonIds_[index])
        {
            SetSelectedMenuItem(index);
            return;
        }
    }
}

void LogoView::HandleKeyboard(const mrg::platform::InputState &input)
{
    const bool previousPressed =
        input.WasKeyPressed(VK_UP) || input.WasKeyPressed(static_cast<std::uint16_t>('W'));
    const bool nextPressed =
        input.WasKeyPressed(VK_DOWN) || input.WasKeyPressed(static_cast<std::uint16_t>('S'));

    if (previousPressed)
    {
        SetSelectedMenuItem((selectedMenuIndex_ + menuButtons_.size() - 1) % menuButtons_.size());
    }
    else if (nextPressed)
    {
        SetSelectedMenuItem((selectedMenuIndex_ + 1) % menuButtons_.size());
    }

    if (input.WasKeyPressed(VK_RETURN) || input.WasKeyPressed(VK_SPACE))
    {
        ActivateMenuItem(selectedMenuIndex_);
    }
}

bool LogoView::ApplyMenuActions()
{
    for (const mrg::visual2d::Action &action : canvas_->TakeActions())
    {
        if (options_.ProcessAction(action))
        {
            RefreshSkinImages();
            ApplyTexts();
            return true;
        }
        if (options_.IsVisible())
        {
            continue;
        }
        if (action.type != mrg::visual2d::ActionType::Clicked)
        {
            continue;
        }

        for (std::size_t index = 0; index < menuButtonIds_.size(); ++index)
        {
            if (action.source == menuButtonIds_[index])
            {
                SetSelectedMenuItem(index);
                ActivateMenuItem(index);
                return true;
            }
        }
    }
    return false;
}

void LogoView::SetSelectedMenuItem(const std::size_t index)
{
    if (index >= menuButtons_.size() || selectionCursor_ == nullptr)
    {
        return;
    }

    selectedMenuIndex_ = index;
    for (std::size_t buttonIndex = 0; buttonIndex < menuButtons_.size(); ++buttonIndex)
    {
        ApplyButtonSelectionStyle(*menuButtons_[buttonIndex], buttonIndex == selectedMenuIndex_);
    }

    selectionCursor_->SetSize(CursorSize);
    selectionCursor_->SetPosition(
        {CursorCenterX, MenuSize.height - ButtonHeight * 0.5F -
                            ButtonVerticalStep * static_cast<float>(selectedMenuIndex_)});
}

void LogoView::ActivateMenuItem(std::size_t index)
{
    if (index == OptionIndex)
    {
        options_.Toggle();
        return;
    }
    command_ = index;
}

bool LogoView::HasPointerActivity(const mrg::platform::InputState &input) noexcept
{
    for (const mrg::platform::InputEvent &event : input.Events())
    {
        if (event.type == mrg::platform::InputEventType::MouseMoved ||
            event.type == mrg::platform::InputEventType::MouseButtonPressed ||
            event.type == mrg::platform::InputEventType::MouseButtonReleased)
        {
            return true;
        }
    }
    return false;
}

std::int64_t LogoView::LatestPointerTimestamp(const mrg::platform::InputState &input) noexcept
{
    std::int64_t result{};
    for (const mrg::platform::InputEvent &event : input.Events())
    {
        if (event.type == mrg::platform::InputEventType::MouseMoved ||
            event.type == mrg::platform::InputEventType::MouseButtonPressed ||
            event.type == mrg::platform::InputEventType::MouseButtonReleased)
        {
            result = event.performanceCounterTicks;
        }
    }
    return result;
}
