#include "FingerDrumLogoScene.h"

#include <algorithm>
#include <filesystem>

namespace
{
    constexpr mrg::visual2d::Size LeftFadeSourceSize{1680.0F, 1280.0F};
    constexpr mrg::visual2d::Size CenterLogoSourceSize{2400.0F, 1280.0F};
    constexpr mrg::visual2d::Size RightFadeSourceSize{1680.0F, 1280.0F};
    constexpr float StripSourceHeight = 1280.0F;
    constexpr float StripSideMargin = 24.0F;

    [[nodiscard]] std::filesystem::path RuntimeAssetPath(
        const wchar_t* fileName)
    {
        return mrg::platform::ResolveExecutableRelativePath(
            std::filesystem::path(L"assets\\images\\logo") / fileName);
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
}

void FingerDrumLogoScene::Initialize(const mrg::EngineServices& services)
{
    width_ = services.windowWidth;
    height_ = services.windowHeight;

    // FixedHeight keeps the logo composition measured against a 720-unit
    // vertical reference while expanding the logical width for wider screens.
    canvas_ = std::make_unique<mrg::visual2d::Visual2DCanvas>(
        mrg::visual2d::Size{1280.0F, 720.0F},
        mrg::visual2d::CanvasScaleMode::FixedHeight);
    canvas_->SetViewportSize({
        static_cast<float>(width_),
        static_cast<float>(height_)});

    CreateLogoStrip(services);
    UpdateLogoStripLayout();
}

void FingerDrumLogoScene::Update(
    const mrg::UpdateContext& context,
    mrg::scene::SceneManager&)
{
    if (canvas_ != nullptr)
    {
        canvas_->Update(context.deltaSeconds);
    }
}

void FingerDrumLogoScene::Render(const mrg::graphics::RenderContext& context)
{
    if (canvas_ == nullptr || context.visual2DRendering == nullptr)
    {
        return;
    }

    // The Canvas has no interactive content yet; it simply submits the logo
    // strip in the Engine's standard screen-space Visual2D pass.
    context.visual2DRendering->SubmitScreen(*canvas_, context);
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

    canvas_->SetViewportSize({
        static_cast<float>(width_),
        static_cast<float>(height_)});
    UpdateLogoStripLayout();
}

void FingerDrumLogoScene::Shutdown() noexcept
{
    // Nodes are owned by the Canvas. Clear observers before its tree is
    // destroyed so this Scene never retains dangling node pointers.
    rightFade_ = nullptr;
    centerLogo_ = nullptr;
    leftFade_ = nullptr;
    logoStrip_ = nullptr;
    canvas_.reset();
}

void FingerDrumLogoScene::CreateLogoStrip(
    const mrg::EngineServices& services)
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
        services.visual2DRendering.LoadImage(RuntimeAssetPath(L"LeftFade.png")),
        "FingerDrum.LeftFade");
    centerLogo_ = &mrg::visual2d::CreateSprite(
        strip,
        {0.0F, 0.0F, 1.0F, 1.0F},
        services.visual2DRendering.LoadImage(RuntimeAssetPath(L"Center.png")),
        "FingerDrum.CenterLogo");
    rightFade_ = &mrg::visual2d::CreateSprite(
        strip,
        {0.0F, 0.0F, 1.0F, 1.0F},
        services.visual2DRendering.LoadImage(RuntimeAssetPath(L"RightFade.png")),
        "FingerDrum.RightFade");
}

void FingerDrumLogoScene::UpdateLogoStripLayout()
{
    if (canvas_ == nullptr || logoStrip_ == nullptr || leftFade_ == nullptr ||
        centerLogo_ == nullptr || rightFade_ == nullptr)
    {
        return;
    }

    // Fit the complete source strip rather than each image independently.
    // This guarantees the outer edges remain in the viewport on narrow and
    // wide aspect ratios alike, while preserving all image proportions.
    const mrg::visual2d::Size logicalSize = canvas_->LogicalSize();
    const float stripSourceWidth =
        LeftFadeSourceSize.width + CenterLogoSourceSize.width +
        RightFadeSourceSize.width;
    const float availableWidth = std::max(
        logicalSize.width - StripSideMargin * 2.0F,
        1.0F);
    const float availableHeight = std::max(
        logicalSize.height - StripSideMargin * 2.0F,
        1.0F);
    const float scale = std::min(
        availableWidth / stripSourceWidth,
        availableHeight / StripSourceHeight);

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
