#include "RhythmTestScene.h"
#include "GameFlow/FingerDrumSceneIds.h"
#include "Submodules/GameplayPauseMenu.h"
#include "Submodules/GameplayPresenter.h"
#include "Submodules/GameplaySessionController.h"
#include <stdexcept>

RhythmTestScene::RhythmTestScene(std::shared_ptr<finger_drum::GameplayLaunchStore> request,
                                 mrg::audio::AudioPlaybackManager &playback,
                                 mrg::visual2d::ScreenVisual2DManager &visuals, bool debug,
                                 finger_drum::texts::TextCatalog &texts,
                                 std::unique_ptr<finger_drum::mode::IPlayResult> &pendingResult)
    : pendingResult_(pendingResult),
      controller_(std::make_unique<GameplaySessionController>(
          request ? request->Snapshot() : throw std::invalid_argument("Gameplay requires a launch store."), playback,
          debug, request ? request->TakeValidatedSession() : nullptr)),
      view_(std::make_unique<GameplayPresenter>(visuals, texts)),
      pauseMenu_(std::make_unique<GameplayPauseMenu>(visuals, texts))
{
}
RhythmTestScene::~RhythmTestScene() = default;
void RhythmTestScene::Initialize(const mrg::EngineServices &services)
{
    controller_->InitializeSession();
    services_.emplace(services);
    static_cast<void>(PreparePresentation());
}
bool RhythmTestScene::PreparePresentation()
{
    if (presentationReady_)
        return true;
    if (!view_->PrepareAssets(*services_))
        return false;
    view_->Initialize(*services_, controller_->Session());
    controller_->Start(*services_, view_->InitialTimelineTime());
    view_->PresentErrorMessage(controller_->ErrorMessage());
    view_->SetVisible(true);
    presentationReady_ = true;
    return true;
}
void RhythmTestScene::BeginScene()
{
    view_->SetVisible(true);
}
void RhythmTestScene::EndScene() noexcept
{
    view_->SetVisible(false);
}
void RhythmTestScene::Update(const mrg::UpdateContext &context, mrg::scene::SceneManager &scenes)
{
    if (!presentationReady_ && (context.totalSeconds < nextPreparationPoll_ || !PreparePresentation()))
    {
        if (context.totalSeconds >= nextPreparationPoll_)
            nextPreparationPoll_ = context.totalSeconds + 0.002;
        if (context.input.WasKeyPressed(VK_ESCAPE))
            controller_->OpenPauseMenu(context);
        UpdatePauseMenu(context, scenes);
        return;
    }
    controller_->Update(context);
    UpdatePauseMenu(context, scenes);
    if (!controller_->PauseMenuOpen())
        view_->AdvanceEffects(context.deltaSeconds);
    for (const auto &feedback : controller_->Feedback())
        view_->ApplyFeedback(feedback);
    if (!controller_->PauseMenuOpen())
        view_->UpdateInputPresentation(context.input, context.deltaSeconds);
    view_->PresentErrorMessage(controller_->ErrorMessage());
    if (controller_->ShouldShowResult())
        ShowResult(scenes);
}
void RhythmTestScene::UpdatePauseMenu(const mrg::UpdateContext &context, mrg::scene::SceneManager &scenes)
{
    if (!controller_->PauseMenuOpen())
    {
        pauseMenu_->Hide();
        return;
    }
    pauseMenu_->Show(services_->windowWidth, services_->windowHeight, controller_->TimelineReady(),
                     controller_->Session().Timeline().MeasureSpans().size());
    const auto choice = pauseMenu_->Update(context.input);
    switch (choice.action)
    {
    case GameplayPauseAction::Resume:
        if (controller_->ResumeFromPause(context, choice.leadIn))
            pauseMenu_->Hide();
        break;
    case GameplayPauseAction::Rewind:
        if (controller_->RewindFromPause(context, choice.measures))
            pauseMenu_->Hide();
        break;
    case GameplayPauseAction::Retry:
        controller_->RetryFromPause(context);
        pauseMenu_->Hide();
        break;
    case GameplayPauseAction::Retire:
        controller_->RetireFromPause();
        pauseMenu_->Hide();
        ShowResult(scenes);
        break;
    default:
        break;
    }
}
void RhythmTestScene::ShowResult(mrg::scene::SceneManager &scenes)
{
    pendingResult_ = controller_->TakeResult();
    if (!scenes.ChangeScene(finger_drum::scene_ids::Result))
        throw std::runtime_error("Failed to enter the Result Scene.");
}
void RhythmTestScene::Render(const mrg::graphics::RenderContext &)
{
    if (presentationReady_)
        view_->UpdatePresentation(controller_->Time());
}
void RhythmTestScene::OnResize(std::uint32_t width, std::uint32_t height)
{
    if (services_)
    {
        services_->windowWidth = width;
        services_->windowHeight = height;
    }
    view_->OnResize(width, height);
    pauseMenu_->OnResize(width, height);
}
void RhythmTestScene::Shutdown() noexcept
{
    pauseMenu_->Shutdown();
    view_->Shutdown(); // Release session observers before the session.
    controller_->Shutdown();
    services_.reset();
    presentationReady_ = false;
    nextPreparationPoll_ = 0;
}
