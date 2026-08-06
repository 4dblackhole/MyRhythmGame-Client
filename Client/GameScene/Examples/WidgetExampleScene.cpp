#include "WidgetExampleScene.h"

#include "GameFlow/SceneIds.h"

#include <Windows.h>

#include <algorithm>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <utility>

namespace
{
    constexpr mrg::ui::UiSize CanvasSize{520.0F, 360.0F};
    constexpr std::size_t MaximumDynamicWidgets = 6;
}

void WidgetExampleScene::Initialize(const mrg::EngineServices& services)
{
    width_ = services.windowWidth;
    height_ = services.windowHeight;
    uiRenderer_.Initialize(services.meshRendering, services.textRendering);
    canvas_ = std::make_unique<mrg::ui::UiCanvas>(CanvasSize);
    BuildCanvas();

    // Begin with one runtime-created widget so the mutable child region is
    // visible immediately. Further additions follow the exact same path.
    AddDynamicWidget();
}

void WidgetExampleScene::BuildCanvas()
{
    auto& panel = canvas_->Root().EmplaceChild<mrg::ui::UiPanel>();
    panel.SetBounds({0.0F, 0.0F, CanvasSize.width, CanvasSize.height});
    panel.SetStyle({
        {0.045F, 0.065F, 0.11F, 0.96F},
        {0.055F, 0.080F, 0.13F, 0.96F},
        {0.035F, 0.050F, 0.09F, 0.96F},
        {0.045F, 0.065F, 0.11F, 0.70F}});

    auto& title = panel.EmplaceChild<mrg::ui::UiLabel>(
        L"3  RUNTIME WIDGET OWNERSHIP");
    title.SetBounds({20.0F, 12.0F, 480.0F, 36.0F});
    title.SetFontSize(23.0F);
    title.SetTextColor({0.52F, 0.84F, 1.0F, 1.0F});

    auto& addButton = panel.EmplaceChild<mrg::ui::UiButton>(
        L"ADD WIDGET  [A]");
    addButton.SetBounds({20.0F, 56.0F, 232.0F, 46.0F});
    addButton.SetFontSize(17.0F);
    addButtonId_ = addButton.Id();

    auto& removeButton = panel.EmplaceChild<mrg::ui::UiButton>(
        L"REMOVE LAST  [D]");
    removeButton.SetBounds({268.0F, 56.0F, 232.0F, 46.0F});
    removeButton.SetFontSize(17.0F);
    removeButtonId_ = removeButton.Id();

    auto& dynamicContainer = panel.EmplaceChild<mrg::ui::UiPanel>();
    dynamicContainer.SetBounds({20.0F, 118.0F, 480.0F, 150.0F});
    dynamicContainer.SetStyle({
        {0.08F, 0.10F, 0.16F, 0.95F},
        {0.08F, 0.10F, 0.16F, 0.95F},
        {0.08F, 0.10F, 0.16F, 0.95F},
        {0.08F, 0.10F, 0.16F, 0.65F}});
    dynamicContainerId_ = dynamicContainer.Id();

    auto& status = panel.EmplaceChild<mrg::ui::UiLabel>(
        L"Canvas owns widgets through unique_ptr children.");
    status.SetBounds({20.0F, 280.0F, 480.0F, 32.0F});
    status.SetFontSize(16.0F);
    status.SetTextColor({0.64F, 1.0F, 0.72F, 1.0F});
    statusLabelId_ = status.Id();

    auto& hint = panel.EmplaceChild<mrg::ui::UiLabel>(
        L"CLICK A DYNAMIC WIDGET   |   SPACE: BACK");
    hint.SetBounds({20.0F, 318.0F, 480.0F, 28.0F});
    hint.SetFontSize(15.0F);
    hint.SetTextColor({0.72F, 0.76F, 0.88F, 1.0F});
}

void WidgetExampleScene::Update(
    const mrg::UpdateContext& context,
    mrg::scene::SceneManager& scenes)
{
    if (context.input.WasKeyPressed(VK_ESCAPE))
    {
        scenes.Quit();
        return;
    }
    if (context.input.WasKeyPressed(VK_SPACE))
    {
        if (!scenes.ChangeScene(game::scene_ids::ColoredCube))
        {
            throw std::runtime_error(
                "Failed to return from the widget example Scene.");
        }
        return;
    }

    ProcessPointer(context.input);
    if (context.input.WasKeyPressed(static_cast<std::uint16_t>('A')))
    {
        AddDynamicWidget();
    }
    if (context.input.WasKeyPressed(static_cast<std::uint16_t>('D')))
    {
        RemoveLastDynamicWidget();
    }
}

void WidgetExampleScene::ProcessPointer(
    const mrg::platform::InputState& input)
{
    const std::optional<mrg::ui::UiPoint> canvasPointer =
        input.IsMouseInsideWindow()
            ? mrg::ui::MapScreenPointer(
                  {static_cast<float>(input.MousePositionX()),
                   static_cast<float>(input.MousePositionY())},
                  {static_cast<float>(width_), static_cast<float>(height_)},
                  CanvasSize,
                  CanvasOrigin())
            : std::nullopt;

    mrg::ui::UiPointerInput pointer{};
    pointer.available = canvasPointer.has_value();
    pointer.position = canvasPointer.value_or(mrg::ui::UiPoint{});
    pointer.leftButtonDown = input.IsMouseButtonDown(
        mrg::platform::MouseButton::Left);
    pointer.leftButtonPressed = input.WasMouseButtonPressed(
        mrg::platform::MouseButton::Left);
    pointer.leftButtonReleased = input.WasMouseButtonReleased(
        mrg::platform::MouseButton::Left);
    pointer.timestampTicks = LatestPointerTimestamp(input);
    inputRouter_.Process(*canvas_, pointer);
    ApplyUiActions();
}

void WidgetExampleScene::ApplyUiActions()
{
    for (const mrg::ui::UiAction& action : canvas_->TakeActions())
    {
        if (action.type != mrg::ui::UiActionType::Clicked)
        {
            continue;
        }
        if (action.source == addButtonId_)
        {
            AddDynamicWidget();
            continue;
        }
        if (action.source == removeButtonId_)
        {
            RemoveLastDynamicWidget();
            continue;
        }

        const auto found = std::find(
            dynamicWidgetIds_.begin(),
            dynamicWidgetIds_.end(),
            action.source);
        if (found != dynamicWidgetIds_.end())
        {
            const std::size_t index = static_cast<std::size_t>(
                std::distance(dynamicWidgetIds_.begin(), found));
            SetStatus(
                L"Clicked dynamic widget slot " +
                std::to_wstring(index + 1) + L".");
        }
    }
}

void WidgetExampleScene::AddDynamicWidget()
{
    if (dynamicWidgetIds_.size() >= MaximumDynamicWidgets)
    {
        SetStatus(L"The example region is full (maximum 6 widgets).");
        return;
    }

    auto* container = dynamic_cast<mrg::ui::UiPanel*>(
        canvas_->FindElement(dynamicContainerId_));
    if (container == nullptr)
    {
        throw std::runtime_error("The dynamic widget container is missing.");
    }

    // EmplaceChild transfers ownership to the retained UI tree. Only the
    // stable ID is retained by the Scene because vector growth and deletion
    // make long-lived raw child pointers unnecessary.
    const std::size_t slot = dynamicWidgetIds_.size();
    auto& widget = container->EmplaceChild<mrg::ui::UiButton>(
        L"DYNAMIC WIDGET #" + std::to_wstring(nextWidgetNumber_++));
    const float x = slot % 2 == 0 ? 8.0F : 244.0F;
    const float y = static_cast<float>(slot / 2) * 48.0F + 6.0F;
    widget.SetBounds({x, y, 228.0F, 40.0F});
    widget.SetFontSize(15.0F);
    widget.SetStyle({
        {0.18F, 0.28F, 0.52F, 1.0F},
        {0.26F, 0.42F, 0.72F, 1.0F},
        {0.10F, 0.18F, 0.38F, 1.0F},
        {0.12F, 0.16F, 0.24F, 0.65F}});
    dynamicWidgetIds_.push_back(widget.Id());

    SetStatus(
        L"Added widget; child count = " +
        std::to_wstring(dynamicWidgetIds_.size()) + L".");
    RefreshControlState();
}

void WidgetExampleScene::RemoveLastDynamicWidget()
{
    if (dynamicWidgetIds_.empty())
    {
        SetStatus(L"There is no dynamic widget to remove.");
        return;
    }

    auto* container = dynamic_cast<mrg::ui::UiPanel*>(
        canvas_->FindElement(dynamicContainerId_));
    if (container == nullptr)
    {
        throw std::runtime_error("The dynamic widget container is missing.");
    }

    // Reset first so the input router cannot retain hover/capture state for a
    // child that RemoveChild is about to destroy.
    inputRouter_.Reset(*canvas_);
    const mrg::ui::UiElementId removedId = dynamicWidgetIds_.back();
    if (!container->RemoveChild(removedId))
    {
        throw std::runtime_error("Failed to remove the dynamic UI child.");
    }
    dynamicWidgetIds_.pop_back();

    SetStatus(
        L"Removed widget; child count = " +
        std::to_wstring(dynamicWidgetIds_.size()) + L".");
    RefreshControlState();
}

void WidgetExampleScene::RefreshControlState()
{
    if (mrg::ui::UiElement* add = canvas_->FindElement(addButtonId_))
    {
        add->SetEnabled(dynamicWidgetIds_.size() < MaximumDynamicWidgets);
    }
    if (mrg::ui::UiElement* remove = canvas_->FindElement(removeButtonId_))
    {
        remove->SetEnabled(!dynamicWidgetIds_.empty());
    }
}

void WidgetExampleScene::SetStatus(std::wstring text)
{
    if (auto* label = dynamic_cast<mrg::ui::UiLabel*>(
            canvas_->FindElement(statusLabelId_)))
    {
        label->SetText(std::move(text));
    }
}

void WidgetExampleScene::Render(
    const mrg::graphics::RenderContext& context)
{
    if (canvas_ != nullptr)
    {
        uiRenderer_.SubmitScreen(*canvas_, context, CanvasOrigin());
    }
}

void WidgetExampleScene::OnResize(
    const std::uint32_t width,
    const std::uint32_t height)
{
    width_ = width;
    height_ = height;
}

void WidgetExampleScene::Shutdown() noexcept
{
    if (canvas_ != nullptr)
    {
        inputRouter_.Reset(*canvas_);
    }
    dynamicWidgetIds_.clear();
    canvas_.reset();
    uiRenderer_.Shutdown();
}

mrg::ui::UiPoint WidgetExampleScene::CanvasOrigin() const noexcept
{
    return {
        std::max(20.0F, (static_cast<float>(width_) - CanvasSize.width) * 0.5F),
        std::max(92.0F, (static_cast<float>(height_) - CanvasSize.height) * 0.5F)};
}

std::int64_t WidgetExampleScene::LatestPointerTimestamp(
    const mrg::platform::InputState& input) const noexcept
{
    std::int64_t result{};
    for (const mrg::platform::InputEvent& event : input.Events())
    {
        if (event.type == mrg::platform::InputEventType::MouseButtonPressed ||
            event.type == mrg::platform::InputEventType::MouseButtonReleased ||
            event.type == mrg::platform::InputEventType::MouseMoved)
        {
            result = event.performanceCounterTicks;
        }
    }
    return result;
}
