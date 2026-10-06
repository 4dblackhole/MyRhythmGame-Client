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
    namespace v = mrg::visual2d;
    namespace r = finger_drum::rhythm;
    constexpr float ReferencePixelsPerMicrosecond = 400.0F / 180'000.0F;
    constexpr float BarHeight = 8.0F;
    constexpr std::array<std::string_view, 7> ImageNames{
        "MAX", "PERFECT", "GREAT", "GOOD", "BAD", "MISS", "POOR"};
    constexpr v::Color Ink{0.06F, 0.09F, 0.16F, 1.0F};

    void Tint(v::Visual2DNode &node, const v::Color color)
    {
        node.GetComponent<v::SpriteVisualComponent>()->SetTint(color);
    }

    // Only real timing judgements get a marker. Roll/count-only ticks have no
    // grade; an automatic miss does not represent a player's input timestamp.
    bool HasInputTiming(const r::NoteEvent &event) noexcept
    {
        return (event.type == r::NoteEventType::HitAccepted ||
                event.type == r::NoteEventType::HoldStarted ||
                event.type == r::NoteEventType::TickAccepted ||
                event.type == r::NoteEventType::InputRejected) &&
            r::IsAtLeastAsAccurateAs(event.judgement.grade, r::JudgementGrade::Bad);
    }
}

void GameplayJudgementView::Initialize(v::ScreenVisual2DManager &visuals,
    v::Visual2DCanvas &canvas, v::Visual2DNode &judgementCircle,
    const r::AccuracyRange &baseRange, finger_drum::texts::TextCatalog &texts)
{
    if (guide_ != nullptr)
        throw std::logic_error("Judgement view is already initialized.");
    for (std::size_t index = 0; index < halfWindows_.size(); ++index)
        halfWindows_[index] = baseRange.Bands()[index].halfWindow;
    displayHalfWindow_ = baseRange.HalfWindow(r::JudgementGrade::Bad) + r::RhythmDuration{10'000};
    CreateTimingGuide(canvas.Root(), texts);
    CreateJudgementImages(visuals, judgementCircle);
    OnResize(canvas.LogicalSize().width);
    Reset();
}

void GameplayJudgementView::CreateTimingGuide(v::Visual2DNode &root,
    finger_drum::texts::TextCatalog &texts)
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
    margin_ = &v::CreatePanel(*guide_, {-halfWidth, 0.0F, halfWidth * 2.0F, BarHeight}, "Judgement.Margin");
    Tint(*margin_, {0.38F, 0.43F, 0.50F, 1.0F});
    // Wide outer bands first; smaller bands cover the centre without gaps.
    for (std::size_t reverse = bands_.size(); reverse > 0; --reverse)
    {
        const std::size_t index = reverse - 1;
        bands_[index] = &v::CreatePanel(*guide_, {}, "Judgement.Band." + std::to_string(index));
        bands_[index]->SetZIndex(static_cast<int>(bands_.size() - index));
        Tint(*bands_[index], finger_drum::presentation::LaneKeyBeam::GradeColor(
            static_cast<r::JudgementGrade>(index)));
    }
}

void GameplayJudgementView::CreateTimingCaptions(finger_drum::texts::TextCatalog &texts)
{
    const auto &text = finger_drum::texts::Gameplay(texts.CurrentLanguage());
    const std::array<std::wstring_view, 3> captions{text.early, L"0", text.late};
    for (std::size_t index = 0; index < labels_.size(); ++index)
    {
        labels_[index] = &v::CreateLabel(*guide_, {}, std::wstring(captions[index]),
            "Judgement.Caption." + std::to_string(index));
        auto &label = *labels_[index]->GetComponent<v::TextVisualComponent>();
        texts.ApplyFont(label);
        label.SetFontSize(12.0F);
        label.SetTextColor(Ink);
        label.SetHorizontalAlignment(index == 0 ? v::TextAlignment::Leading :
            index == 1 ? v::TextAlignment::Center : v::TextAlignment::Trailing);
    }
}

void GameplayJudgementView::CreateTimingMarker()
{
    auto &zero = v::CreatePanel(*guide_, {-0.5F, -1.0F, 1.0F, BarHeight + 2.0F}, "Judgement.Zero");
    zero.SetZIndex(6);
    Tint(zero, Ink);
    marker_ = &v::CreatePanel(*guide_, {-2.0F, -3.0F, 4.0F, BarHeight + 6.0F}, "Judgement.Marker");
    marker_->SetPivot({0.5F, 0.5F});
    marker_->SetZIndex(7);
    Tint(*marker_, Ink);
    auto &fill = v::CreatePanel(*marker_, {1.0F, 1.0F, 2.0F, BarHeight + 4.0F}, "Judgement.MarkerFill");
    Tint(fill, {1.0F, 1.0F, 1.0F, 1.0F});
}

void GameplayJudgementView::CreateJudgementImages(v::ScreenVisual2DManager &visuals,
    v::Visual2DNode &judgementCircle)
{
    const float size = judgementCircle.NodeSize().width * 1.4F;
    for (std::size_t index = 0; index < judgementImages_.size(); ++index)
    {
        const std::string file = "Judgements/" + std::string(ImageNames[index]) + ".png";
        const auto image = visuals.RegisterImage(mrg_client::asset_paths::skin::InGame(file));
        const auto nativeSize = visuals.GetImageSize(image);
        if (nativeSize.width <= 0.0F || nativeSize.height <= 0.0F)
            throw std::logic_error("Judgement skin image must have a non-zero size.");
        const float height = size * nativeSize.height / nativeSize.width;
        auto &node = v::CreateSprite(judgementCircle, {0.0F, 0.0F, size, height},
            image, "Lane.Judgement." + std::string(ImageNames[index]));
        node.SetPivot({0.5F, 0.5F});
        node.SetPosition({judgementCircle.NodeSize().width * 0.5F,
            judgementCircle.NodeSize().height * 0.5F});
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
    pixelsPerMicrosecond_ = std::min(ReferencePixelsPerMicrosecond,
        std::max(logicalWidth - 40.0F, 1.0F) / totalMicroseconds);
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
    UpdateMarkerPosition();
}

void GameplayJudgementView::UpdateMarkerPosition()
{
    const auto error = std::clamp(markerError_, -displayHalfWindow_, displayHalfWindow_);
    marker_->SetPosition({static_cast<float>(error.count()) * pixelsPerMicrosecond_, BarHeight * 0.5F});
}

void GameplayJudgementView::Present(const r::NoteEvent &event)
{
    if (guide_ == nullptr)
        return;
    const bool hasTiming = HasInputTiming(event);
    const bool poor = finger_drum::presentation::LaneKeyBeam::IsWrongInput(event);
    const bool miss = event.type == r::NoteEventType::Missed;
    if (!hasTiming && !miss)
        return;
    const std::size_t index = poor ? ImageNames.size() - 1 : miss
        ? static_cast<std::size_t>(r::JudgementGrade::Miss) : static_cast<std::size_t>(event.judgement.grade);
    if (index >= judgementImages_.size())
        return;
    if (activeJudgement_ != nullptr)
        activeJudgement_->SetVisible(false);
    activeJudgement_ = judgementImages_[index];
    judgementElapsedSeconds_ = 0.0;
    Tint(*activeJudgement_, {1.0F, 1.0F, 1.0F, 1.0F});
    activeJudgement_->SetVisible(true);
    if (hasTiming)
    {
        markerError_ = event.judgement.signedError;
        markerElapsedSeconds_ = 0.0;
        UpdateMarkerPosition();
        marker_->SetVisible(true);
    }
}

void GameplayJudgementView::Update(const double deltaSeconds)
{
    if (guide_ == nullptr || !std::isfinite(deltaSeconds))
        return;
    const double delta = std::max(deltaSeconds, 0.0);
    markerElapsedSeconds_ = std::min(FeedbackSeconds, markerElapsedSeconds_ + delta);
    marker_->SetVisible(markerElapsedSeconds_ < FeedbackSeconds);
    if (activeJudgement_ != nullptr)
    {
        judgementElapsedSeconds_ = std::min(FeedbackSeconds, judgementElapsedSeconds_ + delta);
        Tint(*activeJudgement_, {1.0F, 1.0F, 1.0F,
            static_cast<float>(1.0 - judgementElapsedSeconds_ / FeedbackSeconds)});
        activeJudgement_->SetVisible(judgementElapsedSeconds_ < FeedbackSeconds);
    }
}

void GameplayJudgementView::Reset() noexcept
{
    markerElapsedSeconds_ = FeedbackSeconds;
    judgementElapsedSeconds_ = FeedbackSeconds;
    markerError_ = {};
    if (marker_ != nullptr)
        marker_->SetVisible(false);
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
    marker_ = nullptr;
    judgementImages_.fill(nullptr);
}
