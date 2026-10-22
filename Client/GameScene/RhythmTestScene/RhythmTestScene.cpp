#include "RhythmTestScene.h"
#include "GameFlow/YOUMUSceneIds.h"
#include "GameFlow/GameplaySessionController.h"
#include "Submodules/GameplayPauseMenu.h"
#include "Submodules/GameplayPresenter.h"
#include <stdexcept>

RhythmTestScene::RhythmTestScene(std::shared_ptr<GameplayLaunchStore> request,
                                 AudioPlaybackManager &playback,
                                 ScreenVisual2DManager &visuals, bool debug,
                                 TextCatalog &texts,
                                 std::unique_ptr<IPlayResult> &pendingResult)
    : pendingResult_(pendingResult),
      controller_(std::make_unique<GameplaySessionController>(
          request ? request->Snapshot() : throw std::invalid_argument("Gameplay requires a launch store."), playback,
          debug, request ? request->TakeValidatedSession() : nullptr)),
      view_(std::make_unique<GameplayPresenter>(visuals, texts)),
      pauseMenu_(std::make_unique<GameplayPauseMenu>(visuals, texts))
{
    const auto launch = request->Snapshot();
    view_->ConfigureBackground(launch.backgroundPath, launch.backgroundOffsetX, launch.backgroundOffsetY);
}
RhythmTestScene::~RhythmTestScene() = default;
void RhythmTestScene::Initialize(const EngineServices &services)
{
    services_.emplace(services);
    static_cast<void>(PreparePresentation());
}
bool RhythmTestScene::PreparePresentation()
{
    if (presentationReady_)
        return true;
    if (!controller_->PrepareSession())
        return false;
    // Start both preparations before polling either. The playable screen must
    // never appear while cold hit samples are still being decoded.
    const auto audio = controller_->PrepareAudioResources(services_->audio);
    const bool images = view_->PrepareAssets(*services_);
    if (!images || audio == AudioPreparationState::Loading)
        return false;
    view_->Initialize(*services_, controller_->Session());
    controller_->Start(*services_, view_->InitialTimelineTime());
    view_->PresentErrorMessage(controller_->ErrorMessage());
    view_->SetVisible(false);
    presentationReady_ = true;
    return true;
}
void RhythmTestScene::BeginScene()
{
    view_->SetVisible(presentationReady_ && controller_->CurrentState() != PlayState::Loading);
}
void RhythmTestScene::EndScene() noexcept
{
    view_->SetVisible(false);
}
void RhythmTestScene::Update(const UpdateContext &context, SceneManager &scenes)
{
    if (!presentationReady_ && (context.totalSeconds < nextPreparationPoll_ || !PreparePresentation()))
    {
        if (context.totalSeconds >= nextPreparationPoll_)
            nextPreparationPoll_ = context.totalSeconds + 0.002;
        if (controller_->SessionReady() && context.input.WasKeyPressed(VK_ESCAPE))
            controller_->OpenPauseMenu(context);
        UpdatePauseMenu(context, scenes);
        return;
    }
    controller_->Update(context);
    view_->SetVisible(controller_->CurrentState() != PlayState::Loading);
    UpdatePauseMenu(context, scenes);
    if (controller_->CurrentState() == PlayState::Gameplay)
        view_->AdvanceEffects(context.deltaSeconds);
    for (const auto &feedback : controller_->Feedback())
        view_->ApplyFeedback(feedback);
    if (controller_->CurrentState() == PlayState::Gameplay)
        view_->UpdateInputPresentation(context.input, context.deltaSeconds);
    view_->PresentErrorMessage(controller_->ErrorMessage());
    if (controller_->ShouldShowResult())
        ShowResult(scenes);
}
void RhythmTestScene::UpdatePauseMenu(const UpdateContext &context, SceneManager &scenes)
{
    if (!controller_->PauseMenuOpen())
    {
        pauseMenu_->Hide();
        return;
    }
    pauseMenu_->Show(services_->windowWidth, services_->windowHeight, controller_->CanContinue(),
                     controller_->Session().Timeline().MeasureSpans().size());
    const auto choice = pauseMenu_->Update(context.input);
    switch (choice.action)
    {
    case GameplayPauseAction::Resume:
        if (controller_->ResumeFromPause(context, choice.leadIn) && !controller_->PauseMenuOpen())
            pauseMenu_->Hide();
        break;
    case GameplayPauseAction::Rewind:
        if (controller_->RewindFromPause(context, choice.measures) && !controller_->PauseMenuOpen())
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
void RhythmTestScene::ShowResult(SceneManager &scenes)
{
    pendingResult_ = controller_->TakeResult();
    if (!scenes.ChangeScene(ResultSceneId))
        throw std::runtime_error("Failed to enter the Result Scene.");
}
void RhythmTestScene::Render(const RenderContext &)
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
