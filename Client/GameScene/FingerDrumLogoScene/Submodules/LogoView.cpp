#include "LogoView.h"
#include "LogoStyle.h"
#include "App/AssetPaths.h"
using namespace logo_ui;

LogoView::LogoView(mrg::visual2d::ScreenVisual2DManager &screenVisuals) noexcept
    : screenVisuals_(screenVisuals)
{
}

void LogoView::Initialize(const mrg::EngineServices &services)
{
    width_ = services.windowWidth;
    height_ = services.windowHeight;

    // FixedHeight keeps the logo composition measured against a 720-unit
    // vertical reference while expanding the logical width for wider screens.
    canvasHandle_ = screenVisuals_.CreateOwnedCanvas(
        {{1280.0F, 720.0F}, mrg::visual2d::CanvasScaleMode::FixedHeight});
    canvas_ = canvasHandle_.Get();
    if (canvas_ == nullptr)
    {
        throw std::runtime_error("Failed to create the logo screen Canvas.");
    }

    CreateLogoStrip();
    CreateMenu();
    UpdateLogoStripLayout();
}

void LogoView::BeginScene()
{
    static_cast<void>(canvasHandle_.SetVisible(true));
}

void LogoView::EndScene() noexcept
{
    static_cast<void>(canvasHandle_.SetVisible(false));
}

std::optional<std::size_t> LogoView::Update(const mrg::platform::InputState &input)
{
    command_.reset();
    if (!canvas_)
        return command_;
    ProcessPointer(input);
    UpdateSelectionFromPointer(input);
    if (!ApplyMenuActions())
        HandleKeyboard(input);
    return command_;
}

void LogoView::OnResize(const std::uint32_t width, const std::uint32_t height)
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

void LogoView::Shutdown() noexcept
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
