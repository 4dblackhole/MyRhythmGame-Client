#include "LogoView.h"
#include "LogoStyle.h"
#include "App/AssetPaths.h"
#include "Texts/GameScene/FingerDrumLogoScene/LogoTexts.h"
using namespace logo_ui;

LogoView::LogoView(mrg::visual2d::ScreenVisual2DManager &screenVisuals,
                   finger_drum::texts::TextCatalog &texts) noexcept
    : screenVisuals_(screenVisuals), texts_(texts), options_(texts)
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
    skinRevision_ = mrg_client::SkinSetSelection::Instance().Revision();
    options_.Initialize(*canvas_);
    ApplyTexts();
    UpdateLogoStripLayout();
}

void LogoView::BeginScene()
{
    RefreshSkinImages();
    if (textRevision_ != texts_.Revision())
    {
        ApplyTexts();
    }
    static_cast<void>(canvasHandle_.SetVisible(true));
}

void LogoView::EndScene() noexcept
{
    static_cast<void>(canvasHandle_.SetVisible(false));
}

std::optional<std::size_t> LogoView::Update(const mrg::platform::InputState &input,
                                            const double deltaSeconds)
{
    command_.reset();
    if (!canvas_)
        return command_;
    RefreshSkinImages();
    if (textRevision_ != texts_.Revision())
        ApplyTexts();
    options_.Update(deltaSeconds);
    const bool controlDown =
        input.IsKeyDown(VK_LCONTROL) || input.IsKeyDown(VK_RCONTROL);
    if (controlDown && input.WasKeyPressed(static_cast<std::uint16_t>('O')))
        options_.Toggle();
    ProcessPointer(input);
    options_.ProcessInput(input, canvasPointer_, inputRouter_.HoveredNode(), inputRouter_);
    const bool handled = ApplyMenuActions();
    if (!options_.IsVisible())
        UpdateSelectionFromPointer(input);
    if (!handled && !options_.IsVisible())
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

    options_.Shutdown();
    canvasPointer_.reset();
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
