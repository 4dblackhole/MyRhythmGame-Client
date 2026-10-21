#include "GameplayJudgementView.h"
#include "App/AssetPaths.h"
#include "Presentation/LaneKeyBeam.h"
#include "Texts/GameScene/RhythmTestScene/GameplayTexts.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace
{

    constexpr float ReferencePixelsPerMicrosecond = 400.0F / 180'000.0F;
    constexpr float BarHeight = 8.0F;
    constexpr std::array<std::string_view, 8> ImageNames{"MAX", "PERFECT", "GREAT", "GOOD",
                                                         "BAD", "MISS",    "POOR",  "OK"};
    constexpr std::size_t PoorImageIndex = 6, OkImageIndex = 7;
    constexpr Color Ink{0.06F, 0.09F, 0.16F, 1.0F};

    void Tint(Visual2DNode &node, const Color color)
    {
        node.GetComponent<SpriteVisualComponent>()->SetTint(color);
    }

    // Only real timing judgements get a marker. Roll/count-only ticks have no
    // grade; an automatic miss does not represent a player's input timestamp.
    bool HasInputTiming(const NoteEvent &event) noexcept
    {
        return (event.type == NoteEventType::HitAccepted || event.type == NoteEventType::HoldStarted ||
                event.type == NoteEventType::TickAccepted || event.type == NoteEventType::InputRejected) &&
               IsAtLeastAsAccurateAs(event.judgement.grade, JudgementGrade::Bad);
    }
} // namespace

void GameplayJudgementView::Initialize(ScreenVisual2DManager &visuals, Visual2DCanvas &canvas,
                                       Visual2DNode &judgementCircle, const AccuracyRange &baseRange,
                                       TextCatalog &texts)
{
    if (guide_ != nullptr)
        throw std::logic_error("Judgement view is already initialized.");
    for (std::size_t index = 0; index < halfWindows_.size(); ++index)
        halfWindows_[index] = baseRange.Bands()[index].halfWindow;
    displayHalfWindow_ = baseRange.HalfWindow(JudgementGrade::Bad) + RhythmDuration{10'000};
    CreateTimingGuide(canvas.Root(), texts);
    CreateJudgementImages(visuals, judgementCircle);
    OnResize(canvas.LogicalSize().width);
    Reset();
}

void GameplayJudgementView::CreateTimingGuide(Visual2DNode &root, TextCatalog &texts)
{
    guide_ = &root.CreateChild("Hud.JudgementGuide");
    guide_->SetPosition({0.0F, -342.0F});
    guide_->SetZIndex(5);
    CreateTimingBands();
    CreateTimingCaptions(texts);
    CreateTimingMarker();
}

void GameplayJudgementView::CreateTimingBands()
{
    const float halfWidth = static_cast<float>(displayHalfWindow_.count()) * ReferencePixelsPerMicrosecond;
    margin_ = &CreatePanel(*guide_, {-halfWidth, 0.0F, halfWidth * 2.0F, BarHeight}, "Judgement.Margin");
    Tint(*margin_, {0.38F, 0.43F, 0.50F, 1.0F});
    // Wide outer bands first; smaller bands cover the centre without gaps.
    for (std::size_t reverse = bands_.size(); reverse > 0; --reverse)
    {
        const std::size_t index = reverse - 1;
        bands_[index] = &CreatePanel(*guide_, {}, "Judgement.Band." + std::to_string(index));
        bands_[index]->SetZIndex(static_cast<int>(bands_.size() - index));
        Tint(*bands_[index], LaneKeyBeam::GradeColor(static_cast<JudgementGrade>(index)));
    }
}

void GameplayJudgementView::CreateTimingCaptions(TextCatalog &texts)
{
    const auto &text = Gameplay(texts);
    const std::array<std::wstring_view, 3> captions{text.early, L"0", text.late};
    for (std::size_t index = 0; index < labels_.size(); ++index)
    {
        labels_[index] =
            &CreateLabel(*guide_, {}, std::wstring(captions[index]), "Judgement.Caption." + std::to_string(index));
        auto &label = *labels_[index]->GetComponent<TextVisualComponent>();
        texts.ApplyFont(label);
        label.SetFontSize(12.0F);
        label.SetTextColor(Ink);
        label.SetHorizontalAlignment(index == 0   ? TextAlignment::Leading
                                     : index == 1 ? TextAlignment::Center
                                                  : TextAlignment::Trailing);
    }
}

void GameplayJudgementView::CreateTimingMarker()
{
    auto &zero = CreatePanel(*guide_, {-0.5F, -1.0F, 1.0F, BarHeight + 2.0F}, "Judgement.Zero");
    zero.SetZIndex(6);
    Tint(zero, Ink);
    const auto create = [this](TimingMarker &marker, std::string name)
    {
        marker.node = &CreatePanel(*guide_, {-2.0F, -3.0F, 4.0F, BarHeight + 6.0F}, std::move(name));
        marker.node->SetPivot({0.5F, 0.5F});
        marker.node->SetZIndex(7);
        marker.fill = &CreatePanel(*marker.node, {1.0F, 1.0F, 2.0F, BarHeight + 4.0F}, "Judgement.MarkerFill");
        TintMarker(marker, 1);
    };
    for (std::size_t i = 0; i < markers_.size(); ++i)
        create(markers_[i], i == 0 ? "Judgement.Marker" : "Judgement.Marker." + std::to_string(i));
    for (std::size_t i = 0; i < retiringMarkers_.size(); ++i)
        create(retiringMarkers_[i], "Judgement.RetiringMarker." + std::to_string(i));
}

void GameplayJudgementView::CreateJudgementImages(ScreenVisual2DManager &visuals, Visual2DNode &judgementCircle)
{
    const float size = judgementCircle.NodeSize().width * 1.4F;
    for (std::size_t index = 0; index < judgementImages_.size(); ++index)
    {
        const std::string file = "Judgements." + std::string(ImageNames[index]);
        const auto image = visuals.RegisterImage(InGame(file));
        const auto nativeSize = visuals.GetImageSize(image);
        if (nativeSize.width <= 0.0F || nativeSize.height <= 0.0F)
            throw std::logic_error("Judgement skin image must have a non-zero size.");
        const float height = size * nativeSize.height / nativeSize.width;
        auto &node = CreateSprite(judgementCircle, {0.0F, 0.0F, size, height}, image,
                                     "Lane.Judgement." + std::string(ImageNames[index]));
        node.SetPivot({0.5F, 0.5F});
        node.SetPosition({judgementCircle.NodeSize().width * 0.5F, judgementCircle.NodeSize().height * 0.5F});
        // Follow the judgement-circle position/scale but keep the wording
        // upright inside Taiko's clockwise-rotated lane.
        node.Transform().SetRotationRollPitchYaw(0.0F, 0.0F, DirectX::XM_PIDIV2);
        node.SetZIndex(1);
        judgementImages_[index] = &node;
    }
}

void GameplayJudgementView::OnResize(const float logicalWidth)
{
    if (guide_ == nullptr)
        return;
    const float totalMicroseconds = static_cast<float>(displayHalfWindow_.count()) * 2.0F;
    pixelsPerMicrosecond_ =
        std::min(ReferencePixelsPerMicrosecond, std::max(logicalWidth - 40.0F, 1.0F) / totalMicroseconds);
    const float halfWidth = static_cast<float>(displayHalfWindow_.count()) * pixelsPerMicrosecond_;
    margin_->SetBounds({-halfWidth, 0.0F, halfWidth * 2.0F, BarHeight});
    for (std::size_t index = 0; index < bands_.size(); ++index)
    {
        const float width = static_cast<float>(halfWindows_[index].count()) * pixelsPerMicrosecond_ * 2.0F;
        bands_[index]->SetBounds({-width * 0.5F, 0.0F, width, BarHeight});
    }
    labels_[0]->SetBounds({-halfWidth, 12.0F, 80.0F, 16.0F});
    labels_[1]->SetBounds({-20.0F, 12.0F, 40.0F, 16.0F});
    labels_[2]->SetBounds({halfWidth - 80.0F, 12.0F, 80.0F, 16.0F});
    UpdateMarkerPositions();
}

void GameplayJudgementView::PositionMarker(TimingMarker &marker)
{
    const auto error = std::clamp(marker.error, -displayHalfWindow_, displayHalfWindow_);
    marker.node->SetPosition({static_cast<float>(error.count()) * pixelsPerMicrosecond_, BarHeight * 0.5F});
}

void GameplayJudgementView::TintMarker(TimingMarker &marker, const float alpha)
{
    auto outline = Ink;
    outline.alpha = alpha;
    Tint(*marker.node, outline);
    Tint(*marker.fill, {1, 1, 1, alpha});
}

void GameplayJudgementView::UpdateMarkerPositions()
{
    for (auto &marker : markers_)
        PositionMarker(marker);
    for (auto &marker : retiringMarkers_)
        PositionMarker(marker);
}

void GameplayJudgementView::AddMarker(const RhythmDuration error)
{
    auto &marker = markers_[nextMarker_];
    if (markerCount_ == markers_.size())
    {
        auto &retiring = retiringMarkers_[nextRetiring_];
        retiring.error = marker.error;
        retiring.fadeElapsed = 0;
        PositionMarker(retiring);
        TintMarker(retiring, 1);
        retiring.node->SetVisible(true);
        nextRetiring_ = (nextRetiring_ + 1) % retiringMarkers_.size();
    }
    else
        ++markerCount_;
    marker.error = error;
    PositionMarker(marker);
    marker.node->SetVisible(true);
    nextMarker_ = (nextMarker_ + 1) % markers_.size();
    // Draw newer retained hits above older hits even after the ring wraps.
    for (std::size_t i = 0; i < markerCount_; ++i)
        markers_[(nextMarker_ + markers_.size() - markerCount_ + i) % markers_.size()].node->SetZIndex(
            8 + static_cast<int>(i));
}

void GameplayJudgementView::Present(const NoteEvent &event)
{
    if (guide_ == nullptr)
        return;
    const bool hasTiming = HasInputTiming(event);
    const bool poor = LaneKeyBeam::IsWrongInput(event);
    const bool miss = event.type == NoteEventType::Missed;
    if (!hasTiming && !miss)
        return;
    const std::size_t index = poor   ? PoorImageIndex
                              : miss ? static_cast<std::size_t>(JudgementGrade::Miss)
                                     : static_cast<std::size_t>(event.judgement.grade);
    if (index >= judgementImages_.size())
        return;
    ShowImage(index);
    if (hasTiming)
    {
        AddMarker(event.judgement.signedError);
    }
}

void GameplayJudgementView::ShowImage(std::size_t index)
{
    if (activeJudgement_ != nullptr)
        activeJudgement_->SetVisible(false);
    activeJudgement_ = judgementImages_[index];
    judgementElapsedSeconds_ = 0.0;
    Tint(*activeJudgement_, {1.0F, 1.0F, 1.0F, 1.0F});
    activeJudgement_->SetVisible(true);
}

void GameplayJudgementView::PresentCompletion(bool success)
{
    if (guide_ != nullptr)
        ShowImage(success ? OkImageIndex : static_cast<std::size_t>(JudgementGrade::Miss));
}

void GameplayJudgementView::Update(const double deltaSeconds)
{
    if (guide_ == nullptr || !std::isfinite(deltaSeconds))
        return;
    const double delta = std::max(deltaSeconds, 0.0);
    for (auto &marker : retiringMarkers_)
    {
        if (marker.fadeElapsed >= FeedbackSeconds)
            continue;
        marker.fadeElapsed = std::min(FeedbackSeconds, marker.fadeElapsed + delta);
        TintMarker(marker, static_cast<float>(1 - marker.fadeElapsed / FeedbackSeconds));
        marker.node->SetVisible(marker.fadeElapsed < FeedbackSeconds);
    }
    if (activeJudgement_ != nullptr)
    {
        judgementElapsedSeconds_ = std::min(FeedbackSeconds, judgementElapsedSeconds_ + delta);
        Tint(*activeJudgement_,
             {1.0F, 1.0F, 1.0F, static_cast<float>(1.0 - judgementElapsedSeconds_ / FeedbackSeconds)});
        activeJudgement_->SetVisible(judgementElapsedSeconds_ < FeedbackSeconds);
    }
}

void GameplayJudgementView::Reset() noexcept
{
    judgementElapsedSeconds_ = FeedbackSeconds;
    nextMarker_ = markerCount_ = nextRetiring_ = 0;
    const auto reset = [](auto &markers)
    {
        for (auto &marker : markers)
        {
            marker.error = {};
            marker.fadeElapsed = FeedbackSeconds;
            if (marker.node)
                marker.node->SetVisible(false);
        }
    };
    reset(markers_);
    reset(retiringMarkers_);
    for (auto *node : judgementImages_)
        if (node != nullptr)
            node->SetVisible(false);
    activeJudgement_ = nullptr;
}

void GameplayJudgementView::Shutdown() noexcept
{
    Reset();
    if (guide_ != nullptr)
        guide_->SetVisible(false);
    guide_ = nullptr;
    margin_ = nullptr;
    bands_.fill(nullptr);
    labels_.fill(nullptr);
    markers_ = {};
    retiringMarkers_ = {};
    judgementImages_.fill(nullptr);
}
