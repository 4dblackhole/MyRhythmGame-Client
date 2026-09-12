#include "FingerDrumLogoScene.h"

#include "GameFlow/FingerDrumSceneIds.h"

#include <Windows.h>

#include <array>
#include <filesystem>
#include <optional>
#include <stdexcept>

namespace
{
    constexpr mrg::visual2d::Size LeftFadeSourceSize{1680.0F, 1280.0F};
    constexpr mrg::visual2d::Size CenterLogoSourceSize{2400.0F, 1280.0F};
    constexpr mrg::visual2d::Size RightFadeSourceSize{1680.0F, 1280.0F};
    constexpr float StripSourceHeight = 1280.0F;
    constexpr mrg::visual2d::Size MenuSize{360.0F, 280.0F};
    constexpr float MenuCenterOffsetY = -190.0F;
    constexpr float ButtonLeft = 60.0F;
    constexpr float ButtonWidth = 300.0F;
    constexpr float ButtonHeight = 52.0F;
    constexpr float ButtonVerticalStep = 76.0F;
    constexpr mrg::visual2d::Size CursorSize{48.0F, 48.0F};
    constexpr float CursorCenterX = 28.0F;
    constexpr std::size_t GameStartIndex = 0;
    constexpr std::size_t EditorIndex = 1;
    constexpr std::size_t OptionIndex = 2;
    constexpr std::size_t ExitIndex = 3;

    [[nodiscard]] std::filesystem::path RuntimeAssetPath(
        const std::filesystem::path& relativePath)
    {
        return mrg::platform::ResolveExecutableRelativePath(
            std::filesystem::path(L"assets\\images") / relativePath);
    }

    template <typename ComponentType>
    [[nodiscard]] ComponentType& RequireComponent(
        mrg::visual2d::Visual2DNode& node)
    {
        ComponentType* component = node.GetComponent<ComponentType>();
        if (component == nullptr)
        {
            throw std::logic_error(
                "The FingerDrum menu node is missing a component.");
        }
        return *component;
    }

    void SetCenteredImageLayout(
        mrg::visual2d::Visual2DNode& node,
        const float centerX,
        const float centerY,
        const mrg::visual2d::Size size)
    {
        node.SetPivot({0.5F, 0.5F});
        node.SetSize(size);
        node.SetPosition({centerX, centerY});
    }

    void ApplyButtonSelectionStyle(
        mrg::visual2d::Visual2DNode& button,
        const bool selected)
    {
        mrg::visual2d::VisualStyle style{};
        if (selected)
        {
            style.normal = {0.22F, 0.56F, 0.88F, 0.94F};
            style.hovered = {0.30F, 0.66F, 0.96F, 1.0F};
            style.pressed = {0.12F, 0.39F, 0.72F, 1.0F};
            style.disabled = {0.20F, 0.35F, 0.52F, 0.55F};
        }
        else
        {
            style.normal = {0.78F, 0.89F, 0.98F, 0.88F};
            style.hovered = style.normal;
            style.pressed = {0.20F, 0.48F, 0.78F, 1.0F};
            style.disabled = {0.55F, 0.64F, 0.72F, 0.50F};
        }

        RequireComponent<mrg::visual2d::SpriteVisualComponent>(button).
            SetStyle(style);
        RequireComponent<mrg::visual2d::TextVisualComponent>(button).
            SetTextColor(
                selected
                    ? mrg::visual2d::Color{1.0F, 1.0F, 1.0F, 1.0F}
                    : mrg::visual2d::Color{0.08F, 0.28F, 0.52F, 1.0F});
    }
}

FingerDrumLogoScene::FingerDrumLogoScene(
    mrg::visual2d::ScreenVisual2DManager& screenVisuals) noexcept
    : screenVisuals_(screenVisuals)
{
}

void FingerDrumLogoScene::Initialize(const mrg::EngineServices& services)
{
    width_ = services.windowWidth;
    height_ = services.windowHeight;

    // FixedHeight keeps the logo composition measured against a 720-unit
    // vertical reference while expanding the logical width for wider screens.
    canvasHandle_ = screenVisuals_.CreateOwnedCanvas({
        {1280.0F, 720.0F},
        mrg::visual2d::CanvasScaleMode::FixedHeight});
    canvas_ = canvasHandle_.Get();
    if (canvas_ == nullptr)
    {
        throw std::runtime_error("Failed to create the logo screen Canvas.");
    }

    CreateLogoStrip();
    CreateMenu();
    UpdateLogoStripLayout();
}

void FingerDrumLogoScene::BeginScene()
{
    static_cast<void>(canvasHandle_.SetVisible(true));
}

void FingerDrumLogoScene::EndScene() noexcept
{
    static_cast<void>(canvasHandle_.SetVisible(false));
}

void FingerDrumLogoScene::Update(
    const mrg::UpdateContext& context,
    mrg::scene::SceneManager& scenes)
{
    if (canvas_ == nullptr)
    {
        return;
    }

    ProcessPointer(context.input);
    UpdateSelectionFromPointer(context.input);
    if (ApplyMenuActions(scenes))
    {
        return;
    }
    HandleKeyboard(context.input, scenes);
}

void FingerDrumLogoScene::Render(const mrg::graphics::RenderContext&)
{
}

void FingerDrumLogoScene::OnResize(
    const std::uint32_t width,
    const std::uint32_t height)
{
    width_ = width;
    height_ = height;
    if (canvas_ == nullptr)
    {
        return;
    }

    UpdateLogoStripLayout();
    inputRouter_.InvalidateHitTest();
}

void FingerDrumLogoScene::Shutdown() noexcept
{
    if (canvas_ != nullptr)
    {
        inputRouter_.Reset(*canvas_);
    }

    // Nodes are owned by the Canvas. Clear observers before its tree is
    // destroyed so this Scene never retains dangling node pointers.
    selectionCursor_ = nullptr;
    menuButtons_ = {};
    menuButtonIds_ = {};
    rightFade_ = nullptr;
    centerLogo_ = nullptr;
    leftFade_ = nullptr;
    logoStrip_ = nullptr;
    canvas_ = nullptr;
    canvasHandle_.Reset();
}

void FingerDrumLogoScene::CreateLogoStrip()
{
    auto& strip = canvas_->CreateNode(
        mrg::visual2d::Anchor::Center,
        "FingerDrum.LogoStrip");
    strip.SetPivot({0.5F, 0.5F});
    logoStrip_ = &strip;

    // Every panel receives its own centered pivot. Their positions are then
    // calculated within the strip, which prevents aspect-ratio changes from
    // favoring either edge of the composition.
    leftFade_ = &mrg::visual2d::CreateSprite(
        strip,
        {0.0F, 0.0F, 1.0F, 1.0F},
        screenVisuals_.RegisterImage(
            RuntimeAssetPath(L"logo\\LeftFade.png")),
        "FingerDrum.LeftFade");
    centerLogo_ = &mrg::visual2d::CreateSprite(
        strip,
        {0.0F, 0.0F, 1.0F, 1.0F},
        screenVisuals_.RegisterImage(
            RuntimeAssetPath(L"logo\\Center.png")),
        "FingerDrum.CenterLogo");
    rightFade_ = &mrg::visual2d::CreateSprite(
        strip,
        {0.0F, 0.0F, 1.0F, 1.0F},
        screenVisuals_.RegisterImage(
            RuntimeAssetPath(L"logo\\RightFade.png")),
        "FingerDrum.RightFade");
}

void FingerDrumLogoScene::CreateMenu()
{
    auto& menu = canvas_->CreateNode(
        mrg::visual2d::Anchor::Center,
        "FingerDrum.Menu");
    menu.SetPivot({0.5F, 0.5F});
    menu.SetSize(MenuSize);
    menu.SetPosition({0.0F, MenuCenterOffsetY});
    menu.SetZIndex(10);
    auto& gameStart = mrg::visual2d::CreateButton(
        menu,
        {ButtonLeft,
         MenuSize.height - ButtonHeight,
         ButtonWidth,
         ButtonHeight},
        L"Game Start",
        "FingerDrum.GameStart");
    auto& editor = mrg::visual2d::CreateButton(
        menu,
        {ButtonLeft,
         MenuSize.height - ButtonVerticalStep - ButtonHeight,
         ButtonWidth,
         ButtonHeight},
        L"Editor",
        "FingerDrum.Editor");
    auto& option = mrg::visual2d::CreateButton(
        menu,
        {ButtonLeft,
         MenuSize.height - ButtonVerticalStep * 2.0F - ButtonHeight,
         ButtonWidth,
         ButtonHeight},
        L"Option",
        "FingerDrum.Option");
    auto& exit = mrg::visual2d::CreateButton(
        menu,
        {ButtonLeft,
         MenuSize.height - ButtonVerticalStep * 3.0F - ButtonHeight,
         ButtonWidth,
         ButtonHeight},
        L"Exit",
        "FingerDrum.Exit");
    menuButtons_ = {&gameStart, &editor, &option, &exit};
    menuButtonIds_ = {
        gameStart.Id(), editor.Id(), option.Id(), exit.Id()};

    for (mrg::visual2d::Visual2DNode* button : menuButtons_)
    {
        RequireComponent<mrg::visual2d::TextVisualComponent>(*button).
            SetFontSize(25.0F);
    }

    const mrg::visual2d::ImageHandle cursorImage =
        screenVisuals_.RegisterImage(
            RuntimeAssetPath(L"menu\\SelectionCursor.png"));
    auto& cursor = mrg::visual2d::CreateSprite(
        menu,
        {0.0F, 0.0F, CursorSize.width, CursorSize.height},
        cursorImage,
        "FingerDrum.SelectionCursor");
    cursor.SetPivot({0.5F, 0.5F});
    cursor.SetZIndex(2);
    selectionCursor_ = &cursor;

    SetSelectedMenuItem(GameStartIndex);
}

void FingerDrumLogoScene::UpdateLogoStripLayout()
{
    if (canvas_ == nullptr || logoStrip_ == nullptr || leftFade_ == nullptr ||
        centerLogo_ == nullptr || rightFade_ == nullptr)
    {
        return;
    }

    // The authored panels share one source height, so scale the complete strip
    // from that height. FixedHeight Canvas scaling then keeps the art exactly
    // flush with the physical viewport vertically at every window size.
    // Extra horizontal art remains outside the viewport instead of distorting
    // the source aspect ratio.
    const mrg::visual2d::Size logicalSize = canvas_->LogicalSize();
    const float scale = logicalSize.height / StripSourceHeight;

    const mrg::visual2d::Size leftSize{
        LeftFadeSourceSize.width * scale,
        LeftFadeSourceSize.height * scale};
    const mrg::visual2d::Size centerSize{
        CenterLogoSourceSize.width * scale,
        CenterLogoSourceSize.height * scale};
    const mrg::visual2d::Size rightSize{
        RightFadeSourceSize.width * scale,
        RightFadeSourceSize.height * scale};
    const mrg::visual2d::Size stripSize{
        leftSize.width + centerSize.width + rightSize.width,
        leftSize.height};

    logoStrip_->SetSize(stripSize);
    logoStrip_->SetPosition({0.0F, 0.0F});

    const float centerY = stripSize.height * 0.5F;
    SetCenteredImageLayout(
        *leftFade_,
        leftSize.width * 0.5F,
        centerY,
        leftSize);
    SetCenteredImageLayout(
        *centerLogo_,
        leftSize.width + centerSize.width * 0.5F,
        centerY,
        centerSize);
    SetCenteredImageLayout(
        *rightFade_,
        leftSize.width + centerSize.width + rightSize.width * 0.5F,
        centerY,
        rightSize);
}

void FingerDrumLogoScene::ProcessPointer(
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
}

void FingerDrumLogoScene::UpdateSelectionFromPointer(
    const mrg::platform::InputState& input)
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

void FingerDrumLogoScene::HandleKeyboard(
    const mrg::platform::InputState& input,
    mrg::scene::SceneManager& scenes)
{
    const bool previousPressed = input.WasKeyPressed(VK_UP) ||
        input.WasKeyPressed(static_cast<std::uint16_t>('W'));
    const bool nextPressed = input.WasKeyPressed(VK_DOWN) ||
        input.WasKeyPressed(static_cast<std::uint16_t>('S'));

    if (previousPressed)
    {
        SetSelectedMenuItem(
            (selectedMenuIndex_ + menuButtons_.size() - 1) %
            menuButtons_.size());
    }
    else if (nextPressed)
    {
        SetSelectedMenuItem(
            (selectedMenuIndex_ + 1) % menuButtons_.size());
    }

    if (input.WasKeyPressed(VK_RETURN) || input.WasKeyPressed(VK_SPACE))
    {
        ActivateMenuItem(selectedMenuIndex_, scenes);
    }
}

bool FingerDrumLogoScene::ApplyMenuActions(
    mrg::scene::SceneManager& scenes)
{
    for (const mrg::visual2d::Action& action : canvas_->TakeActions())
    {
        if (action.type != mrg::visual2d::ActionType::Clicked)
        {
            continue;
        }

        for (std::size_t index = 0; index < menuButtonIds_.size(); ++index)
        {
            if (action.source == menuButtonIds_[index])
            {
                SetSelectedMenuItem(index);
                ActivateMenuItem(index, scenes);
                return true;
            }
        }
    }
    return false;
}

void FingerDrumLogoScene::SetSelectedMenuItem(const std::size_t index)
{
    if (index >= menuButtons_.size() || selectionCursor_ == nullptr)
    {
        return;
    }

    selectedMenuIndex_ = index;
    for (std::size_t buttonIndex = 0;
         buttonIndex < menuButtons_.size();
         ++buttonIndex)
    {
        ApplyButtonSelectionStyle(
            *menuButtons_[buttonIndex],
            buttonIndex == selectedMenuIndex_);
    }

    selectionCursor_->SetSize(CursorSize);
    selectionCursor_->SetPosition({
        CursorCenterX,
        MenuSize.height - ButtonHeight * 0.5F -
            ButtonVerticalStep * static_cast<float>(selectedMenuIndex_)});
}

void FingerDrumLogoScene::ActivateMenuItem(
    const std::size_t index,
    mrg::scene::SceneManager& scenes)
{
    if (index == GameStartIndex)
    {
        if (!scenes.ChangeScene(finger_drum::scene_ids::Lobby))
        {
            throw std::runtime_error(
                "Failed to enter the temporary FingerDrum Lobby Scene.");
        }
        return;
    }
    if (index == EditorIndex)
    {
        if (!scenes.ChangeScene(finger_drum::scene_ids::EditorSongSelect))
        {
            throw std::runtime_error(
                "Failed to enter the editor song-select Scene.");
        }
        return;
    }
    if (index == OptionIndex)
    {
        return;
    }
    if (index == ExitIndex)
    {
        scenes.Quit();
    }
}

bool FingerDrumLogoScene::HasPointerActivity(
    const mrg::platform::InputState& input) noexcept
{
    for (const mrg::platform::InputEvent& event : input.Events())
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

std::int64_t FingerDrumLogoScene::LatestPointerTimestamp(
    const mrg::platform::InputState& input) noexcept
{
    std::int64_t result{};
    for (const mrg::platform::InputEvent& event : input.Events())
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
