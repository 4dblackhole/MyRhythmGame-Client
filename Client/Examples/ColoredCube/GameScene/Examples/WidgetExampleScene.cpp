#include "WidgetExampleScene.h"

#include "App/AssetPaths.h"
#include "Examples/ColoredCube/GameFlow/SceneIds.h"

#include <Windows.h>

#include <algorithm>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <utility>

namespace
{
    constexpr mrg::visual2d::Size CanvasSize{520.0F, 360.0F};
    constexpr std::size_t MaximumDynamicWidgets = 6;
    constexpr mrg::visual2d::Size WidgetImageSize{1672.0F, 941.0F};

    [[nodiscard]] constexpr mrg::visual2d::Rect TopLeftBounds(
        const mrg::visual2d::Rect bounds,
        const float parentHeight) noexcept
    {
        return {
            bounds.x,
            parentHeight - bounds.y - bounds.height,
            bounds.width,
            bounds.height};
    }

    template <typename ComponentType>
    [[nodiscard]] ComponentType& RequireComponent(
        mrg::visual2d::Visual2DNode& node)
    {
        ComponentType* component = node.GetComponent<ComponentType>();
        if (component == nullptr)
        {
            throw std::logic_error("The Visual2D node is missing a component.");
        }
        return *component;
    }
}

WidgetExampleScene::WidgetExampleScene(
    mrg::visual2d::ScreenVisual2DManager& screenVisuals) noexcept
    : screenVisuals_(screenVisuals)
{
}

void WidgetExampleScene::Initialize(const mrg::EngineServices& services)
{
    width_ = services.windowWidth;
    height_ = services.windowHeight;
    canvasHandle_ = screenVisuals_.CreateOwnedCanvas();
    canvas_ = canvasHandle_.Get();
    if (canvas_ == nullptr)
    {
        throw std::runtime_error("Failed to create the widget screen Canvas.");
    }
    const mrg::visual2d::ImageHandle widgetImage =
        screenVisuals_.RegisterImage(
            mrg::platform::ResolveExecutableRelativePath(
                mrg_client::asset_paths::default_skin::widget::First));
    const mrg::visual2d::Size nativeSize =
        screenVisuals_.GetImageSize(widgetImage);
    if (nativeSize.width != WidgetImageSize.width ||
        nativeSize.height != WidgetImageSize.height)
    {
        throw std::runtime_error(
            "Visual2D image metadata did not match Widget1.png.");
    }
    BuildCanvas();

    // Begin with one runtime-created widget so the mutable child region is
    // visible immediately. Further additions follow the exact same path.
    AddDynamicWidget();
}

void WidgetExampleScene::BeginScene()
{
    static_cast<void>(canvasHandle_.SetVisible(true));
}

void WidgetExampleScene::EndScene() noexcept
{
    static_cast<void>(canvasHandle_.SetVisible(false));
}

void WidgetExampleScene::BuildCanvas()
{
    auto& panel = canvas_->CreateNode(
        mrg::visual2d::Anchor::Center,
        "WidgetExample.Panel");
    panel.SetBounds({0.0F, 0.0F, CanvasSize.width, CanvasSize.height});
    panel.AddComponent<mrg::visual2d::SpriteVisualComponent>().SetStyle({
        {0.045F, 0.065F, 0.11F, 0.96F},
        {0.055F, 0.080F, 0.13F, 0.96F},
        {0.035F, 0.050F, 0.09F, 0.96F},
        {0.045F, 0.065F, 0.11F, 0.70F}});

    auto& title = mrg::visual2d::CreateLabel(
        panel,
        TopLeftBounds({20.0F, 12.0F, 480.0F, 36.0F}, CanvasSize.height),
        L"3  RUNTIME COMPONENT WIDGETS");
    auto& titleText = RequireComponent<mrg::visual2d::TextVisualComponent>(title);
    titleText.SetFontSize(23.0F);
    titleText.SetTextColor({0.52F, 0.84F, 1.0F, 1.0F});

    auto& addButton = mrg::visual2d::CreateButton(
        panel,
        TopLeftBounds({20.0F, 56.0F, 232.0F, 46.0F}, CanvasSize.height),
        L"ADD WIDGET  [A]");
    RequireComponent<mrg::visual2d::TextVisualComponent>(addButton).
        SetFontSize(17.0F);
    addButtonId_ = addButton.Id();

    auto& removeButton = mrg::visual2d::CreateButton(
        panel,
        TopLeftBounds({268.0F, 56.0F, 232.0F, 46.0F}, CanvasSize.height),
        L"REMOVE LAST  [D]");
    RequireComponent<mrg::visual2d::TextVisualComponent>(removeButton).
        SetFontSize(17.0F);
    removeButtonId_ = removeButton.Id();

    auto& dynamicContainer = mrg::visual2d::CreatePanel(
        panel,
        TopLeftBounds({20.0F, 118.0F, 480.0F, 150.0F}, CanvasSize.height),
        "DynamicWidgetContainer");
    RequireComponent<mrg::visual2d::SpriteVisualComponent>(dynamicContainer).
        SetStyle({
        {0.08F, 0.10F, 0.16F, 0.95F},
        {0.08F, 0.10F, 0.16F, 0.95F},
        {0.08F, 0.10F, 0.16F, 0.95F},
        {0.08F, 0.10F, 0.16F, 0.65F}});
    dynamicContainerId_ = dynamicContainer.Id();

    auto& status = mrg::visual2d::CreateLabel(
        panel,
        TopLeftBounds({20.0F, 280.0F, 480.0F, 32.0F}, CanvasSize.height),
        L"Canvas owns nodes; behavior is attached as components.");
    auto& statusText = RequireComponent<mrg::visual2d::TextVisualComponent>(status);
    statusText.SetFontSize(16.0F);
    statusText.SetTextColor({0.64F, 1.0F, 0.72F, 1.0F});
    statusLabelId_ = status.Id();

    auto& hint = mrg::visual2d::CreateLabel(
        panel,
        TopLeftBounds({20.0F, 318.0F, 480.0F, 28.0F}, CanvasSize.height),
        L"CLICK A DYNAMIC WIDGET   |   SPACE: BACK");
    auto& hintText = RequireComponent<mrg::visual2d::TextVisualComponent>(hint);
    hintText.SetFontSize(15.0F);
    hintText.SetTextColor({0.72F, 0.76F, 0.88F, 1.0F});
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
    const std::optional<mrg::visual2d::Point> canvasPointer =
        input.IsMouseInsideWindow()
            ? mrg::visual2d::MapScreenPointer(
                  {static_cast<float>(input.MousePositionX()),
                   static_cast<float>(input.MousePositionY())},
                  {static_cast<float>(width_), static_cast<float>(height_)},
                  *canvas_)
            : std::nullopt;

    mrg::visual2d::PointerInput pointer{};
    pointer.available = canvasPointer.has_value();
    pointer.position = canvasPointer.value_or(mrg::visual2d::Point{});
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
    for (const mrg::visual2d::Action& action : canvas_->TakeActions())
    {
        if (action.type != mrg::visual2d::ActionType::Clicked)
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

    auto* container = canvas_->FindNode(dynamicContainerId_);
    if (container == nullptr)
    {
        throw std::runtime_error("The dynamic widget container is missing.");
    }

    // The Canvas owns the node while the button behavior remains a removable
    // component. Only a stable ID is retained across later mutations.
    const std::size_t slot = dynamicWidgetIds_.size();
    const float x = slot % 2 == 0 ? 8.0F : 244.0F;
    const float y = static_cast<float>(slot / 2) * 48.0F + 6.0F;
    auto& widget = mrg::visual2d::CreateButton(
        *container,
        TopLeftBounds({x, y, 228.0F, 40.0F}, 150.0F),
        L"DYNAMIC WIDGET #" + std::to_wstring(nextWidgetNumber_++));
    RequireComponent<mrg::visual2d::TextVisualComponent>(widget).
        SetFontSize(15.0F);
    RequireComponent<mrg::visual2d::SpriteVisualComponent>(widget).SetStyle({
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

    auto* container = canvas_->FindNode(dynamicContainerId_);
    if (container == nullptr)
    {
        throw std::runtime_error("The dynamic widget container is missing.");
    }

    // Reset first so the input router cannot retain hover/capture state for a
    // child that RemoveChild is about to destroy.
    inputRouter_.Reset(*canvas_);
    const mrg::visual2d::NodeId removedId = dynamicWidgetIds_.back();
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
    if (mrg::visual2d::Visual2DNode* add = canvas_->FindNode(addButtonId_))
    {
        add->SetEnabled(dynamicWidgetIds_.size() < MaximumDynamicWidgets);
    }
    if (mrg::visual2d::Visual2DNode* remove = canvas_->FindNode(removeButtonId_))
    {
        remove->SetEnabled(!dynamicWidgetIds_.empty());
    }
}

void WidgetExampleScene::SetStatus(std::wstring text)
{
    if (mrg::visual2d::Visual2DNode* node = canvas_->FindNode(statusLabelId_))
    {
        if (auto* label = node->GetComponent<mrg::visual2d::TextVisualComponent>())
        {
            label->SetText(std::move(text));
        }
    }
}

void WidgetExampleScene::Render(
    const mrg::graphics::RenderContext&)
{
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
    canvas_ = nullptr;
    canvasHandle_.Reset();
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
