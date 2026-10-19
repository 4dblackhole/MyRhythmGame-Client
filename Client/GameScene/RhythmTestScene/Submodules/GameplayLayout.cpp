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
    if (!backgroundPath_.empty())
    {
        try
        {
            backgroundImage_ = screenVisuals_.RegisterImage(backgroundPath_);
            backgroundSize_ = screenVisuals_.GetImageSize(backgroundImage_);
            if (backgroundSize_.width <= 0 || backgroundSize_.height <= 0)
                throw std::runtime_error("Background image has no size.");
            RequireComponent<mrg::visual2d::SpriteVisualComponent>(*background_).SetImage(backgroundImage_);
            SetColor(*background_, {1, 1, 1, 1});
        }
        catch (const std::exception &error)
        {
            backgroundWarning_ = error.what();
            backgroundImage_ = {};
        }
    }

    errorLabel_ = &mrg::visual2d::CreateLabel(root, {-500.0F, 320.0F, 1000.0F, 28.0F}, L"", "Hud.Error");
    errorLabel_->SetZIndex(20);
    errorLabel_->SetVisible(false);
    auto &errorText = RequireComponent<mrg::visual2d::TextVisualComponent>(*errorLabel_);
    texts_.ApplyFont(errorText);
    errorText.SetFontSize(16.0F);
    errorText.SetTextColor({0.78F, 0.06F, 0.08F, 1.0F});
    errorText.SetHorizontalAlignment(mrg::visual2d::TextAlignment::Center);

    progressView_.Initialize(screenVisuals_, root, *session_, texts_);

    const auto accuracyImage = screenVisuals_.RegisterImage(InGameSkinAssetPath(L"AccuracyIndicator"));
    const auto accuracySize = ScaledImageSize(screenVisuals_, accuracyImage);
    const float accuracyWidth = std::max(accuracySize.width, 270.0F);
    accuracyIndicator_ = &mrg::visual2d::CreateLabel(
        root,
        {CanvasReferenceWidth * 0.5F - GearMargin - accuracyWidth,
         CanvasReferenceHeight * 0.5F - 50.0F - accuracySize.height, accuracyWidth, accuracySize.height},
        std::wstring(texts_.Get("Gameplay.emptyAccuracy")), "Hud.Accuracy");
    accuracyIndicator_->SetZIndex(5);
    auto &accuracyText = RequireComponent<mrg::visual2d::TextVisualComponent>(*accuracyIndicator_);
    texts_.ApplyFont(accuracyText);
    accuracyText.SetFontSize(32.0F);
    accuracyText.SetTextColor({0.12F, 0.16F, 0.24F, 1.0F});
    accuracyText.SetHorizontalAlignment(mrg::visual2d::TextAlignment::Trailing);

#if defined(_DEBUG)
    noteDebugLabel_ = &mrg::visual2d::CreateLabel(
        root, {-CanvasReferenceWidth * 0.5F + 12.0F, 160.0F, CanvasReferenceWidth - 24.0F, 120.0F},
        L"", "Debug.NoteState");
    noteDebugLabel_->SetZIndex(30);
    auto &debugText = RequireComponent<mrg::visual2d::TextVisualComponent>(*noteDebugLabel_);
    texts_.ApplyFont(debugText);
    debugText.SetFontSize(16.0F);
    debugText.SetTextColor({0.12F, 0.16F, 0.24F, 1.0F});
#endif

    // All gear parts inherit one transform. Final bounds depend on skin sizes.
    scrollGearRoot_ = &root.CreateChild("ScrollGear");
    scrollGearBorder_ = &mrg::visual2d::CreatePanel(*scrollGearRoot_, {}, "ScrollGear.Border");
    SetColor(*scrollGearBorder_, {0.16F, 0.31F, 0.58F, 1.0F});
    scrollGearSurface_ = &mrg::visual2d::CreatePanel(*scrollGearRoot_, {}, "ScrollGear.Surface");
    SetColor(*scrollGearSurface_, {0.73F, 0.82F, 0.93F, 1.0F});

    CreateLaneVisuals(*scrollGearRoot_);
    inputPresentationRoot_ = &scrollGearRoot_->CreateChild("InputPresentation");
    inputPresentationRoot_->SetZIndex(2);
    const auto inputPanelImage = screenVisuals_.RegisterImage(InGameSkinAssetPath(L"InputPanel"));
    inputPanelSize_ = ScaledImageSize(screenVisuals_, inputPanelImage);
    inputPanel_ = &mrg::visual2d::CreateSprite(
        *inputPresentationRoot_, {0.0F, 0.0F, inputPanelSize_.width, inputPanelSize_.height},
        inputPanelImage, "Input.Panel");
    inputPanel_->SetZIndex(0);
    CreateKeyIndicators(*inputPanel_);

    UpdatePresentationLayout();
}

void GameplayPresenter::CreateLaneVisuals(mrg::visual2d::Visual2DNode &gearRoot)
{
    // Author the lane in its reusable default orientation: local +Y is the
    // future-note direction and every lane-owned visual is a descendant.
    const float laneScreenLeft = GearMargin + inputPanelSize_.width;
    const float laneLength =
        std::max(canvas_->LogicalSize().width - laneScreenLeft - GearRightMargin, 1.0F);
    laneRoot_ = &gearRoot.CreateChild("ScrollGear.Lane");
    laneRoot_->SetPivot({0.5F, 0.5F});
    laneRoot_->SetSize({laneWidth_, laneLength});
    laneRoot_->SetZIndex(1);

    // Taiko is a presentation of the same vertical lane rotated clockwise.
    // Notes continue to update only local Y, so changing this one transform
    // redirects the background, effects, judgement line and notes together.
    laneRoot_->Transform().SetRotationRollPitchYaw(0.0F, 0.0F, -DirectX::XM_PIDIV2);

    CreateLaneSurface();

    const auto beamImage = screenVisuals_.RegisterImage(InGameSkinAssetPath(L"LaneLight"));
    keyBeam_.Initialize(*laneRoot_, beamImage, ScaledImageSize(screenVisuals_, beamImage));

    // Note order is local to this layer, above the key beam and judgement circle.
    noteRoot_ = &laneRoot_->CreateChild("Lane.Notes");
    noteRoot_->SetZIndex(4);

    const auto judgementImage = screenVisuals_.RegisterImage(InGameSkinAssetPath(L"JudgementCircle"));
    const auto judgementSize = ScaledImageSize(screenVisuals_, judgementImage);
    auto &judgementLine = mrg::visual2d::CreateSprite(
        *laneRoot_,
        {(laneWidth_ - judgementSize.width) * 0.5F, judgementLocalY_ - judgementSize.height * 0.5F,
         judgementSize.width, judgementSize.height},
        judgementImage, "Lane.JudgementCircle");
    judgementLine.SetZIndex(3);
    finger_drum::rhythm::JudgeLevel level;
    const auto &lanes = session_->Gear().Lanes();
    if (!lanes.empty() && !lanes.front()->Notes().empty())
        level = lanes.front()->Notes().front()->Profile().Level();
    // Keep the guide fixed for this play session. In particular, do not use
    // the current note's padded syncopation range to resize the HUD mid-play.
    const finger_drum::rhythm::AccuracyRange baseRange{"Hud.Default", level};
    judgementView_.Initialize(screenVisuals_, *canvas_, judgementLine, baseRange, texts_);
}

void GameplayPresenter::CreateLaneSurface()
{
    if (laneRoot_ == nullptr)
    {
        throw std::logic_error("Lane background requires a Lane root.");
    }

    laneImage_ = screenVisuals_.RegisterImage(InGameSkinAssetPath(L"Lane"));
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
    // Canvas +Y points up. Only the shared parent positions the entire gear:
    // its outer frame's bottom touches the screen's horizontal centre.
    if (scrollGearRoot_ != nullptr)
        scrollGearRoot_->SetPosition({0.0F, gearHeight * 0.5F + GearPadding});

    if (background_ != nullptr)
    {
        float width = logicalWidth, height = CanvasReferenceHeight;
        if (backgroundImage_.value != 0)
        {
            // Cover the viewport at the authored centre offset without blank edges.
            const auto scale =
                std::max((logicalWidth + 2 * std::abs(backgroundOffset_.x)) / backgroundSize_.width,
                         (CanvasReferenceHeight + 2 * std::abs(backgroundOffset_.y)) / backgroundSize_.height);
            width = backgroundSize_.width * scale;
            height = backgroundSize_.height * scale;
        }
        const auto offset = backgroundImage_.value ? backgroundOffset_ : mrg::visual2d::Point{};
        background_->SetBounds({-width * .5F + offset.x, -height * .5F + offset.y, width, height});
    }
    progressView_.SetLayout(
        {-logicalWidth * 0.5F + GearMargin, CanvasReferenceHeight * 0.5F - 16.0F - progressView_.Height(),
         std::max(logicalWidth - GearMargin * 2.0F, 1.0F), progressView_.Height()});
    if (accuracyIndicator_ != nullptr)
    {
        const auto bounds = accuracyIndicator_->Bounds();
        accuracyIndicator_->SetBounds({logicalWidth * 0.5F - GearMargin - bounds.width, bounds.y,
                                       bounds.width, bounds.height});
    }
    if (scrollGearBorder_ != nullptr)
    {
        scrollGearBorder_->SetBounds(
            {-logicalWidth * 0.5F + GearMargin - GearPadding,
             -gearHeight * 0.5F - GearPadding,
             std::max(logicalWidth - GearMargin - GearRightMargin + GearPadding * 2.0F, 1.0F),
             gearHeight + GearPadding * 2.0F});
    }
    if (scrollGearSurface_ != nullptr)
    {
        scrollGearSurface_->SetBounds(
            {-logicalWidth * 0.5F + GearMargin, -gearHeight * 0.5F,
             std::max(logicalWidth - GearMargin - GearRightMargin, 1.0F), gearHeight});
    }
    if (laneRoot_ != nullptr)
    {
        laneRoot_->SetSize({laneWidth_, laneLength});
        laneRoot_->SetPosition(
            {-logicalWidth * 0.5F + laneScreenLeft + laneLength * 0.5F, 0.0F});
        UpdateLaneSurfaceLayout(laneLength);
        UpdateNoteTravelLayout();
    }
    if (inputPresentationRoot_ != nullptr)
    {
        inputPresentationRoot_->SetPosition(
            {-logicalWidth * 0.5F + GearMargin, -inputPanelSize_.height * 0.5F});
    }
}

void GameplayPresenter::UpdateNoteTravelLayout()
{
    if (laneRoot_ == nullptr || session_ == nullptr)
    {
        return;
    }

    float largestHeadRadius = largestHeadRadius_;
    for (const auto &[id, layers] : noteVisuals_)
    {
        static_cast<void>(id);
        largestHeadRadius = std::max(largestHeadRadius, layers.diameter * 0.5F);
    }
    // The local +Y lane end meets the screen's right edge after the Taiko
    // rotation. Keep the entire largest head beyond that edge at spawn time.
    noteTravelDistance_ = std::max(
        laneRoot_->NodeSize().height - judgementLocalY_ + largestHeadRadius + 1.0F, 1.0F);
    RebuildVisibilityIndices();

    // Ticks retain their chart spacing when the window width changes.
    for (const auto &lane : session_->Gear().Lanes())
    {
        for (const auto &note : lane->Notes())
        {
            const auto found = noteVisuals_.find(note->Id());
            if (found == noteVisuals_.end())
            {
                continue;
            }
            const NoteVisualLayers &layers = found->second;
            const auto *presentation = session_->FindNotePresentation(note->Id());
            const float speed = presentation == nullptr
                                    ? 1.0F
                                    : static_cast<float>(presentation->scrollMultiplier);
            for (const TimedVisual &tick : layers.ticks)
            {
                const auto bounds = tick.node->Bounds();
                const float offset = TravelPixelsAt(tick.timing, layers.timing, speed);
                tick.node->SetBounds(
                    {(layers.diameter - bounds.width) * 0.5F,
                     layers.diameter * 0.5F + offset - bounds.height * 0.5F,
                     bounds.width, bounds.height});
            }
        }
    }
}
