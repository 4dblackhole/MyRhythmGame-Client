#include "GameplayProgressView.h"
#include "App/AssetPaths.h"
#include "Mode/Submodules/PlaySession.h"

#include <algorithm>
#include <format>
#include <stdexcept>

namespace
{
    namespace v = mrg::visual2d;
    constexpr float AssetScale = 2.0F / 3.0F;
    constexpr float TextInset = 12.0F;

    template <typename Component> Component &RequireComponent(v::Visual2DNode &node)
    {
        auto *component = node.GetComponent<Component>();
        if (!component)
            throw std::logic_error("A progress node is missing its presentation component.");
        return *component;
    }

    std::wstring FormatSeconds(const std::int64_t seconds)
    {
        return std::format(L"{}:{:02}", seconds / 60, seconds % 60);
    }
} // namespace

void GameplayProgressView::CacheLastNoteTime(const finger_drum::mode::PlaySession &session)
{
    endTime_ = {};
    hasNotes_ = false;
    // Use surviving playable notes, not authored measure count, expiry windows
    // or processing state. A long note may end after the latest normal head.
    for (const auto &lane : session.Gear().Lanes())
        for (const auto &note : lane->Notes())
        {
            endTime_ = hasNotes_ ? std::max(endTime_, note->Timing()) : note->Timing();
            hasNotes_ = true;
            const auto *presentation = session.FindNotePresentation(note->Id());
            if (presentation && presentation->hasEndTime)
                endTime_ = std::max(endTime_, presentation->endTime);
        }
}

void GameplayProgressView::Initialize(mrg::visual2d::ScreenVisual2DManager &visuals,
                                      mrg::visual2d::Visual2DNode &parent,
                                      const finger_drum::mode::PlaySession &session,
                                      finger_drum::texts::TextCatalog &texts)
{
    CacheLastNoteTime(session);
    displayedSeconds_ = -1;
    fraction_ = 0;
    const auto image = [&visuals](const wchar_t *name)
    { return visuals.RegisterImage(mrg_client::asset_paths::skin::InGame(std::wstring{L"Progress."} + name)); };
    const auto background = image(L"Background");
    const auto track = image(L"Track");
    const auto fill = image(L"Fill");
    const auto marker = image(L"Marker");
    height_ = visuals.GetImageSize(background).height * AssetScale;
    trackHeight_ = visuals.GetImageSize(track).height * AssetScale;
    const auto markerNative = visuals.GetImageSize(marker);
    markerSize_ = {markerNative.width * AssetScale, markerNative.height * AssetScale};
    if (height_ <= 0 || trackHeight_ <= 0 || markerSize_.width <= 0 || markerSize_.height <= 0)
        throw std::logic_error("Progress skin images must have a non-zero size.");

    root_ = &v::CreateSprite(parent, {}, background, "Hud.GameProgress");
    root_->SetZIndex(5);
    track_ = &v::CreateSprite(*root_, {}, track, "Progress.Track");
    fill_ = &v::CreateSprite(*root_, {}, fill, "Progress.Fill");
    fill_->SetZIndex(1);
    marker_ = &v::CreateSprite(*root_, {}, marker, "Progress.Marker");
    marker_->SetZIndex(2);
    CreateTimeLabels(texts);
    Update(finger_drum::rhythm::RhythmTime::min());
}

void GameplayProgressView::CreateTimeLabels(finger_drum::texts::TextCatalog &texts)
{
    const auto total = FormatSeconds(std::max(endTime_.count(), std::int64_t{}) / 1'000'000);
    labelWidth_ = std::max(40.0F, static_cast<float>(total.size()) * 7.0F);
    currentLabel_ = &mrg::visual2d::CreateLabel(*root_, {}, L"", "Progress.CurrentTime");
    totalLabel_ = &mrg::visual2d::CreateLabel(*root_, {}, total, "Progress.TotalTime");
    for (auto *label : {currentLabel_, totalLabel_})
    {
        label->SetZIndex(3);
        auto &text = RequireComponent<mrg::visual2d::TextVisualComponent>(*label);
        texts.ApplyFont(text);
        text.SetFontSize(16.0F * AssetScale);
        text.SetTextColor({0.94F, 0.97F, 1.0F, 1.0F});
    }
    RequireComponent<mrg::visual2d::TextVisualComponent>(*totalLabel_)
        .SetHorizontalAlignment(mrg::visual2d::TextAlignment::Trailing);
}

void GameplayProgressView::SetLayout(const mrg::visual2d::Rect bounds)
{
    if (!root_)
        return;
    root_->SetBounds(bounds);
    root_->SetClipRect({0, 0, bounds.width, bounds.height});
    const float inset = std::min(TextInset, bounds.width * 0.05F);
    const float labelWidth = std::min(labelWidth_, bounds.width * 0.25F);
    currentLabel_->SetBounds({inset, 0, labelWidth, bounds.height});
    totalLabel_->SetBounds({bounds.width - inset - labelWidth, 0, labelWidth, bounds.height});
    // Reserve room for the marker at both ends of the track so it cannot
    // overlap the time labels when the viewport becomes narrow.
    const float markerWidth = std::min(markerSize_.width, bounds.width * 0.1F);
    const float trackLeft = inset + labelWidth + markerWidth * 0.5F;
    const float trackWidth = std::max(bounds.width - 2.0F * trackLeft, 0.0F);
    const float trackHeight = std::min(trackHeight_, bounds.height);
    track_->SetBounds({trackLeft, (bounds.height - trackHeight) * 0.5F, trackWidth, trackHeight});
    UpdateFillLayout();
}

void GameplayProgressView::UpdateFillLayout()
{
    const auto track = track_->Bounds();
    fill_->SetVisible(fraction_ > 0 && track.width > 0);
    fill_->SetBounds({track.x, track.y, track.width * fraction_, track.height});
    // Crop instead of compressing the texture as progress changes.
    RequireComponent<mrg::visual2d::SpriteVisualComponent>(*fill_).SetUvTransform({fraction_, 1.0F}, {});
    const auto size = root_->NodeSize();
    const float markerWidth = std::min(markerSize_.width, size.width * 0.1F);
    const float markerHeight = markerSize_.height * markerWidth / markerSize_.width;
    marker_->SetBounds({track.x + track.width * fraction_ - markerWidth * 0.5F, (size.height - markerHeight) * 0.5F,
                        markerWidth, markerHeight});
}

void GameplayProgressView::Update(const finger_drum::rhythm::RhythmTime time)
{
    if (!root_)
        return;
    const auto elapsed =
        std::clamp(time, finger_drum::rhythm::RhythmTime{}, std::max(endTime_, finger_drum::rhythm::RhythmTime{}));
    if (endTime_.count() > 0)
        fraction_ = static_cast<float>(static_cast<double>(elapsed.count()) / static_cast<double>(endTime_.count()));
    else
        fraction_ = hasNotes_ && time >= endTime_ ? 1.0F : 0.0F;
    UpdateFillLayout();
    const auto seconds = elapsed.count() / 1'000'000;
    if (seconds != displayedSeconds_)
    {
        RequireComponent<mrg::visual2d::TextVisualComponent>(*currentLabel_).SetText(FormatSeconds(seconds));
        displayedSeconds_ = seconds;
    }
}

void GameplayProgressView::Shutdown() noexcept
{
    root_ = track_ = fill_ = marker_ = currentLabel_ = totalLabel_ = nullptr;
    endTime_ = {};
    displayedSeconds_ = -1;
    height_ = trackHeight_ = labelWidth_ = fraction_ = 0;
    markerSize_ = {};
    hasNotes_ = false;
}
