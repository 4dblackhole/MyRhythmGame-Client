#include "RhythmTestScene.h"
#include "Submodules/GameplayPresenter.h"
#include "Submodules/GameplaySessionController.h"
#include "GameFlow/FingerDrumSceneIds.h"
#include <stdexcept>

RhythmTestScene::RhythmTestScene(std::shared_ptr<finger_drum::GameplayLaunchStore> request,
                                 mrg::audio::AudioPlaybackManager &playback,
                                 mrg::visual2d::ScreenVisual2DManager &visuals, bool debug,
                                 finger_drum::texts::TextCatalog &texts)
    : controller_(
          std::make_unique<GameplaySessionController>(
              request ? request->Snapshot()
                      : throw std::invalid_argument("Gameplay requires a launch store."),
              playback, debug)),
      view_(std::make_unique<GameplayPresenter>(visuals, texts))
{
}
RhythmTestScene::~RhythmTestScene() = default;
void RhythmTestScene::Initialize(const mrg::EngineServices &services)
{
    controller_->InitializeSession();
    view_->Initialize(services, controller_->Session());
    controller_->Start(services);
    view_->PresentAudioError(controller_->AudioError());
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
    if (context.input.WasKeyPressed(VK_ESCAPE))
    {
        if (!scenes.ChangeScene(finger_drum::scene_ids::Lobby))
            throw std::runtime_error("Failed to return to song select.");
        return;
    }
    view_->AdvanceEffects(context.deltaSeconds);
    controller_->Update(context);
    for (const auto &feedback : controller_->Feedback())
        view_->ApplyFeedback(feedback);
    view_->UpdatePresentation(controller_->Time());
    view_->UpdateInputPresentation(context.input, context.deltaSeconds);
    view_->PresentAudioError(controller_->AudioError());
    if (controller_->ShouldReturnToLobby() && !scenes.ChangeScene(finger_drum::scene_ids::Lobby))
        throw std::runtime_error("Failed to return to song select.");
}
void RhythmTestScene::Render(const mrg::graphics::RenderContext &)
{
}
void RhythmTestScene::OnResize(std::uint32_t width, std::uint32_t height)
{
    view_->OnResize(width, height);
}
void RhythmTestScene::Shutdown() noexcept
{
    view_->Shutdown(); // Release session observers before the session.
    controller_->Shutdown();
}
