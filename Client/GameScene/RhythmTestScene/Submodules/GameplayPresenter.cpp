#include "GameplayPresenter.h"
#include "GameplaySupport.h"
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
}

void GameplayPresenter::OnResize(const std::uint32_t width, const std::uint32_t height)
{
    width_ = width;
    height_ = height;
    if (canvas_ != nullptr)
    {
        UpdatePresentationLayout();
    }
}

void GameplayPresenter::Shutdown() noexcept
{
    keyBeam_.Shutdown();
    noteVisuals_.clear();
    presentedNoteIds_.clear();
    completionEffects_.clear();
    measureLineVisuals_.clear();
    keyIndicators_.fill(nullptr);
    keyGlows_.fill(nullptr);
    keyPressFlashes_.fill(nullptr);
    keyPressFlashRemainingSeconds_.fill(0.0);
    background_ = nullptr;
    scrollGearBorder_ = nullptr;
    scrollGearSurface_ = nullptr;
    inputPresentationRoot_ = nullptr;
    inputPanel_ = nullptr;
    laneRoot_ = nullptr;
    laneTiles_.clear();
    laneImage_ = {};
    strongKeyLightImage_ = {};
    weakKeyLightImage_ = {};
    keyPressFlashImage_ = {};
    gameProgressBar_ = nullptr;
    accuracyIndicator_ = nullptr;
#if defined(_DEBUG)
    noteDebugLabel_ = nullptr;
#endif
    judgementIndicator_ = nullptr;
    audioErrorLabel_ = nullptr;
    canvas_ = nullptr;
    canvasHandle_.Reset();
    session_ = nullptr;
}

void GameplayPresenter::ApplyFeedback(const GameplayFeedback &feedback)
{
    if (feedback.reset)
    {
        keyBeam_.Reset();
        completionEffects_.clear();
    }
    if (feedback.keyPressed)
        keyBeam_.OnKeyPressed(feedback.result);
    for (const finger_drum::rhythm::NoteEvent &event : feedback.result.events)
    {
        if (event.type != finger_drum::rhythm::NoteEventType::Completed)
        {
            continue;
        }
        const auto visual = noteVisuals_.find(event.noteId);
        if (visual != noteVisuals_.end() && visual->second.focusType != FocusNoteType::None)
        {
            completionEffects_[event.noteId] = event.eventTime;
        }
    }
}
