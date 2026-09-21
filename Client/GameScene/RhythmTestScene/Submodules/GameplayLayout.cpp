#include "GameplayPresenter.h"
#include "GameplaySupport.h"
using namespace gameplay;

void GameplayPresenter::CreatePresentation()
{
    auto &root = canvas_->Root();

    background_ =
        &mrg::visual2d::CreatePanel(root,
                                    {-CanvasReferenceWidth * 0.5F, -CanvasReferenceHeight * 0.5F,
                                     CanvasReferenceWidth, CanvasReferenceHeight},
                                    "Background");
    SetColor(*background_, {0.941F, 0.973F, 1.0F, 1.0F});

    audioErrorLabel_ =
        &mrg::visual2d::CreateLabel(root, {-500.0F, 320.0F, 1000.0F, 28.0F}, L"", "Hud.AudioError");
    audioErrorLabel_->SetZIndex(20);
    audioErrorLabel_->SetVisible(false);
    auto &audioErrorText = RequireComponent<mrg::visual2d::TextVisualComponent>(*audioErrorLabel_);
    audioErrorText.SetFontSize(16.0F);
    audioErrorText.SetTextColor({0.78F, 0.06F, 0.08F, 1.0F});
    audioErrorText.SetHorizontalAlignment(mrg::visual2d::TextAlignment::Center);

    const auto progressImage =
        screenVisuals_.RegisterImage(InGameSkinAssetPath(L"GameProgressBar.png"));
    const auto progressSize = ScaledImageSize(screenVisuals_, progressImage);
    gameProgressBar_ =
        &mrg::visual2d::CreateSprite(root,
                                     {-CanvasReferenceWidth * 0.5F + GearMargin,
                                      CanvasReferenceHeight * 0.5F - 16.0F - progressSize.height,
                                      progressSize.width, progressSize.height},
                                     progressImage, "Hud.GameProgress");
    gameProgressBar_->SetZIndex(5);

    const auto accuracyImage =
        screenVisuals_.RegisterImage(InGameSkinAssetPath(L"AccuracyIndicator.png"));
    const auto accuracySize = ScaledImageSize(screenVisuals_, accuracyImage);
    accuracyIndicator_ =
        &mrg::visual2d::CreateLabel(root,
                                    {CanvasReferenceWidth * 0.5F - GearMargin - accuracySize.width,
                                     CanvasReferenceHeight * 0.5F - 50.0F - accuracySize.height,
                                     accuracySize.width, accuracySize.height},
                                    L"--.--%", "Hud.Accuracy");
    accuracyIndicator_->SetZIndex(5);
    auto &accuracyText = RequireComponent<mrg::visual2d::TextVisualComponent>(*accuracyIndicator_);
    accuracyText.SetFontSize(32.0F);
    accuracyText.SetTextColor({0.12F, 0.16F, 0.24F, 1.0F});
    accuracyText.SetHorizontalAlignment(mrg::visual2d::TextAlignment::Trailing);

#if defined(_DEBUG)
    noteDebugLabel_ = &mrg::visual2d::CreateLabel(
        root, {-CanvasReferenceWidth * 0.5F + 12.0F, 160.0F, CanvasReferenceWidth - 24.0F, 120.0F},
        L"", "Debug.NoteState");
    noteDebugLabel_->SetZIndex(30);
    auto &debugText = RequireComponent<mrg::visual2d::TextVisualComponent>(*noteDebugLabel_);
    debugText.SetFontSize(16.0F);
    debugText.SetTextColor({0.12F, 0.16F, 0.24F, 1.0F});
#endif

    const auto judgementImage =
        screenVisuals_.RegisterImage(InGameSkinAssetPath(L"JudgementIndicator.png"));
    const auto judgementSize = ScaledImageSize(screenVisuals_, judgementImage);
    judgementIndicator_ = &mrg::visual2d::CreateSprite(root,
                                                       {-judgementSize.width * 0.5F,
                                                        -CanvasReferenceHeight * 0.5F + 18.0F,
                                                        judgementSize.width, judgementSize.height},
                                                       judgementImage, "Hud.JudgementGuide");
    judgementIndicator_->SetZIndex(5);

    scrollGearBorder_ = &mrg::visual2d::CreatePanel(
        root,
        {-CanvasReferenceWidth * 0.5F + GearMargin - GearPadding,
         LaneCenterY - laneWidth_ * 0.5F - GearPadding,
         CanvasReferenceWidth - GearMargin - GearRightMargin + GearPadding * 2.0F,
         laneWidth_ + GearPadding * 2.0F},
        "ScrollGear.Border");
    SetColor(*scrollGearBorder_, {0.16F, 0.31F, 0.58F, 1.0F});
    scrollGearSurface_ = &mrg::visual2d::CreatePanel(
        root,
        {-CanvasReferenceWidth * 0.5F + GearMargin, LaneCenterY - laneWidth_ * 0.5F,
         CanvasReferenceWidth - GearMargin - GearRightMargin, laneWidth_},
        "ScrollGear.Surface");
    SetColor(*scrollGearSurface_, {0.73F, 0.82F, 0.93F, 1.0F});

    CreateLaneVisuals(root);
    inputPresentationRoot_ = &root.CreateChild("InputPresentation");
    inputPresentationRoot_->SetZIndex(2);
    const auto inputPanelImage =
        screenVisuals_.RegisterImage(InGameSkinAssetPath(L"InputPanel.png"));
    inputPanelSize_ = ScaledImageSize(screenVisuals_, inputPanelImage);
    inputPanel_ = &mrg::visual2d::CreateSprite(
        *inputPresentationRoot_, {0.0F, 0.0F, inputPanelSize_.width, inputPanelSize_.height},
        inputPanelImage, "Input.Panel");
    inputPanel_->SetZIndex(0);
    CreateKeyIndicators(*inputPanel_);

    UpdatePresentationLayout();
}

void GameplayPresenter::CreateLaneVisuals(mrg::visual2d::Visual2DNode &sceneRoot)
{
    // Author the lane in its reusable default orientation: local +Y is the
    // future-note direction and every lane-owned visual is a descendant.
    const float laneScreenLeft = GearMargin + inputPanelSize_.width;
    const float laneLength =
        std::max(canvas_->LogicalSize().width - laneScreenLeft - GearRightMargin, 1.0F);
    laneRoot_ = &sceneRoot.CreateChild("ScrollGear.Lane");
    laneRoot_->SetPivot({0.5F, 0.5F});
    laneRoot_->SetSize({laneWidth_, laneLength});
    laneRoot_->SetPosition(
        {-canvas_->LogicalSize().width * 0.5F + laneScreenLeft + laneLength * 0.5F, LaneCenterY});
    laneRoot_->SetZIndex(1);

    // Taiko is a presentation of the same vertical lane rotated clockwise.
    // Notes continue to update only local Y, so changing this one transform
    // redirects the background, effects, judgement line and notes together.
    laneRoot_->Transform().SetRotationRollPitchYaw(0.0F, 0.0F, -DirectX::XM_PIDIV2);

    CreateLaneSurface();
    CreateMeasureLineVisuals();

    const auto beamImage = screenVisuals_.RegisterImage(InGameSkinAssetPath(L"LaneLight.png"));
    keyBeam_.Initialize(*laneRoot_, beamImage, ScaledImageSize(screenVisuals_, beamImage));

    const auto judgementImage =
        screenVisuals_.RegisterImage(InGameSkinAssetPath(L"JudgementCircle.png"));
    const auto judgementSize = ScaledImageSize(screenVisuals_, judgementImage);
    auto &judgementLine = mrg::visual2d::CreateSprite(
        *laneRoot_,
        {(laneWidth_ - judgementSize.width) * 0.5F, judgementLocalY_ - judgementSize.height * 0.5F,
         judgementSize.width, judgementSize.height},
        judgementImage, "Lane.JudgementCircle");
    judgementLine.SetZIndex(4);
}

void GameplayPresenter::CreateLaneSurface()
{
    if (laneRoot_ == nullptr)
    {
        throw std::logic_error("Lane background requires a Lane root.");
    }

    laneImage_ = screenVisuals_.RegisterImage(InGameSkinAssetPath(L"Lane.png"));
    const auto tileSize = ScaledImageSize(screenVisuals_, laneImage_);
    laneWidth_ = tileSize.width;
    laneTileLength_ = tileSize.height;
    judgementLocalY_ = laneWidth_ * 0.5F;
    const float laneScreenLeft = GearMargin + inputPanelSize_.width;
    UpdateLaneSurfaceLayout(
        std::max(canvas_->LogicalSize().width - laneScreenLeft - GearRightMargin, 1.0F));
}

void GameplayPresenter::UpdateLaneSurfaceLayout(const float laneLength)
{
    keyBeam_.SetLayout(laneWidth_, laneLength);
    if (laneRoot_ == nullptr || !laneImage_ || laneTileLength_ <= 0.0F)
    {
        return;
    }

    const std::size_t requiredTiles =
        static_cast<std::size_t>(std::ceil(laneLength / laneTileLength_));
    while (laneTiles_.size() < requiredTiles)
    {
        auto &tile = mrg::visual2d::CreateSprite(*laneRoot_, {}, laneImage_, "Lane.SurfaceTile");
        tile.SetZIndex(0);
        laneTiles_.push_back(&tile);
    }
    for (std::size_t index = 0; index < laneTiles_.size(); ++index)
    {
        mrg::visual2d::Visual2DNode &tile = *laneTiles_[index];
        const float start = static_cast<float>(index) * laneTileLength_;
        const float visibleLength = std::clamp(laneLength - start, 0.0F, laneTileLength_);
        tile.SetVisible(visibleLength > 0.0F);
        if (visibleLength <= 0.0F)
        {
            continue;
        }
        tile.SetBounds({0.0F, start, laneWidth_, visibleLength});
        RequireComponent<mrg::visual2d::SpriteVisualComponent>(tile).SetUvTransform(
            {1.0F, visibleLength / laneTileLength_}, {0.0F, 0.0F});
    }
}

void GameplayPresenter::UpdatePresentationLayout()
{
    if (canvas_ == nullptr)
    {
        return;
    }

    const float logicalWidth = canvas_->LogicalSize().width;
#if defined(_DEBUG)
    if (noteDebugLabel_ != nullptr)
    {
        noteDebugLabel_->SetBounds(
            {-logicalWidth * 0.5F + 12.0F, 160.0F, std::max(logicalWidth - 24.0F, 1.0F), 120.0F});
    }
#endif
    const float laneScreenLeft = GearMargin + inputPanelSize_.width;
    const float laneLength = std::max(logicalWidth - laneScreenLeft - GearRightMargin, 1.0F);
    const float gearHeight = std::max(laneWidth_, inputPanelSize_.height);

    if (background_ != nullptr)
    {
        background_->SetBounds({-logicalWidth * 0.5F, -CanvasReferenceHeight * 0.5F, logicalWidth,
                                CanvasReferenceHeight});
    }
    if (gameProgressBar_ != nullptr)
    {
        gameProgressBar_->SetBounds(
            {-logicalWidth * 0.5F + GearMargin,
             CanvasReferenceHeight * 0.5F - 16.0F - gameProgressBar_->Bounds().height,
             std::max(logicalWidth - GearMargin * 2.0F, 1.0F), gameProgressBar_->Bounds().height});
    }
    if (accuracyIndicator_ != nullptr)
    {
        const auto bounds = accuracyIndicator_->Bounds();
        accuracyIndicator_->SetBounds({logicalWidth * 0.5F - GearMargin - bounds.width, bounds.y,
                                       bounds.width, bounds.height});
    }
    if (judgementIndicator_ != nullptr)
    {
        const auto bounds = judgementIndicator_->Bounds();
        judgementIndicator_->SetBounds(
            {-bounds.width * 0.5F, bounds.y, bounds.width, bounds.height});
    }
    if (scrollGearBorder_ != nullptr)
    {
        scrollGearBorder_->SetBounds(
            {-logicalWidth * 0.5F + GearMargin - GearPadding,
             LaneCenterY - gearHeight * 0.5F - GearPadding,
             std::max(logicalWidth - GearMargin - GearRightMargin + GearPadding * 2.0F, 1.0F),
             gearHeight + GearPadding * 2.0F});
    }
    if (scrollGearSurface_ != nullptr)
    {
        scrollGearSurface_->SetBounds(
            {-logicalWidth * 0.5F + GearMargin, LaneCenterY - gearHeight * 0.5F,
             std::max(logicalWidth - GearMargin - GearRightMargin, 1.0F), gearHeight});
    }
    if (laneRoot_ != nullptr)
    {
        laneRoot_->SetSize({laneWidth_, laneLength});
        laneRoot_->SetPosition(
            {-logicalWidth * 0.5F + laneScreenLeft + laneLength * 0.5F, LaneCenterY});
        UpdateLaneSurfaceLayout(laneLength);
    }
    if (inputPresentationRoot_ != nullptr)
    {
        inputPresentationRoot_->SetPosition(
            {-logicalWidth * 0.5F + GearMargin, LaneCenterY - inputPanelSize_.height * 0.5F});
    }
}
