#include "GameplayPresenter.h"
#include "GameplaySupport.h"
#include <limits>
#include <numeric>
using namespace gameplay;

void GameplayPresenter::Initialize(const mrg::EngineServices &services,
                                   const finger_drum::mode::PlaySession &session)
{
    session_ = &session;
    width_ = services.windowWidth;
    height_ = services.windowHeight;
    canvasHandle_ = screenVisuals_.CreateOwnedCanvas(
        {{1280.0F, 720.0F}, mrg::visual2d::CanvasScaleMode::FixedHeight});
    canvas_ = canvasHandle_.Get();
    if (!canvas_)
        throw std::runtime_error("Failed to create the gameplay screen Canvas.");
    CreatePresentation();
    CreateNoteVisuals();
    UpdateNoteTravelLayout();
}

void GameplayPresenter::OnResize(const std::uint32_t width, const std::uint32_t height)
{
    width_ = width;
    height_ = height;
    if (canvas_ != nullptr)
    {
        UpdatePresentationLayout();
        judgementView_.OnResize(canvas_->LogicalSize().width);
    }
}

finger_drum::rhythm::RhythmTime GameplayPresenter::InitialTimelineTime() const
{
    using Time = finger_drum::rhythm::RhythmTime;
    const Time latest{-2'000'000};
    if (session_->ScrollDistance(latest, {}) <= firstVisibilityDistance_)
        return latest;
    // Invert the monotone cumulative distance only at entry. A slow future
    // object must not lengthen the lead-in of every ordinary note.
    const long double bound = std::floor(firstVisibilityDistance_ / session_->MinimumScrollMultiplier());
    auto low =
        static_cast<Time::rep>(std::max(bound, static_cast<long double>(std::numeric_limits<Time::rep>::lowest())));
    auto high = latest.count();
    while (low < high - 1)
    {
        const auto middle = std::midpoint(low, high);
        if (session_->ScrollDistance(Time{middle}, {}) <= firstVisibilityDistance_)
            low = middle;
        else
            high = middle;
    }
    return Time{low};
}

float GameplayPresenter::TravelPixels(const finger_drum::rhythm::RhythmDuration duration,
                                      const float multiplier) const noexcept
{
    constexpr long double MicrosecondsPerWholeNoteAtBpmOne = 240'000'000.0L;
    return static_cast<float>(static_cast<long double>(duration.count()) *
                              session_->Timeline().BaseBpm() / MicrosecondsPerWholeNoteAtBpmOne *
                              pixelsPerWholeNote_ * multiplier);
}

float GameplayPresenter::TravelPixelsAt(finger_drum::rhythm::RhythmTime target, finger_drum::rhythm::RhythmTime current, float multiplier) const
{
    return static_cast<float>(session_->ScrollDistance(target, current) *
        session_->Timeline().BaseBpm() / 240'000'000.0L * pixelsPerWholeNote_ * multiplier);
}

mrg::visual2d::Point GameplayPresenter::LaneObjectPosition(const finger_drum::rhythm::RhythmTime target,
                                                           const finger_drum::rhythm::RhythmTime current,
                                                           const float multiplier) const
{
    // Notes and measure lines are centred lane objects on the same time axis.
    // Whole integrates the shared distance; Separate scales that distance per object.
    return {laneWidth_ * 0.5F, judgementLocalY_ + TravelPixelsAt(target, current, multiplier)};
}

void GameplayPresenter::Shutdown() noexcept
{
    keyBeam_.Shutdown();
    judgementView_.Shutdown();
    progressView_.Shutdown();
    noteVisuals_.clear();
    noteModels_.clear();
    noteImages_.clear();
    preparingImages_.clear();
    visibleNoteModels_.clear();
    noteVisibility_.Assign({});
    measureVisibility_.Assign({});
    visibilityRevision_ = 0;
    presentationGeneration_ = 0;
    firstVisibilityDistance_ = 0;
    presentedNoteIds_.clear();
    completionEffects_.clear();
    measureLineVisuals_.clear();
    keyIndicators_.fill(nullptr);
    keyGlows_.fill(nullptr);
    keyPressFlashes_.fill(nullptr);
    keyPressFlashRemainingSeconds_.fill(0.0);
    background_ = nullptr;
    scrollGearRoot_ = nullptr;
    scrollGearBorder_ = nullptr;
    scrollGearSurface_ = nullptr;
    inputPresentationRoot_ = nullptr;
    inputPanel_ = nullptr;
    laneRoot_ = nullptr;
    noteRoot_ = nullptr;
    noteTravelDistance_ = 1.0F;
    pixelsPerWholeNote_ = 1.0F;
    laneTiles_.clear();
    laneImage_ = {};
    strongKeyLightImage_ = {};
    weakKeyLightImage_ = {};
    keyPressFlashImage_ = {};
    accuracyIndicator_ = nullptr;
#if defined(_DEBUG)
    noteDebugLabel_ = nullptr;
#endif
    errorLabel_ = nullptr;
    canvas_ = nullptr;
    canvasHandle_.Reset();
    session_ = nullptr;
}

void GameplayPresenter::ApplyFeedback(const GameplayFeedback &feedback)
{
    if (feedback.reset)
    {
        keyBeam_.Reset();
        judgementView_.Reset();
        completionEffects_.clear();
    }
    if (feedback.keyPressed)
        keyBeam_.OnKeyPressed(feedback.result);
    PresentJudgementFeedback(feedback);
    for (const finger_drum::rhythm::NoteEvent &event : feedback.result.events)
    {
        if (event.type != finger_drum::rhythm::NoteEventType::Completed)
        {
            continue;
        }
        if (!noteVisuals_.contains(event.noteId))
            if (const auto model = noteModels_.find(event.noteId); model != noteModels_.end())
                CreateNoteVisual(*model->second);
        const auto visual = noteVisuals_.find(event.noteId);
        if (visual != noteVisuals_.end() && visual->second.focusType != FocusNoteType::None)
        {
            completionEffects_[event.noteId] = event.eventTime;
        }
    }
}
