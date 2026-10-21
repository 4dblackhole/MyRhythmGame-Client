#include "ResultScene.h"
#include "GameFlow/YOUMUSceneIds.h"
#include "Submodules/ResultViewFactory.h"
#include <stdexcept>
ResultScene::ResultScene(std::unique_ptr<IPlayResult> result,
                         ScreenVisual2DManager &visuals, TextCatalog &texts,
                         ResultViewContext context, RetryHandler retry)
    : result_(std::move(result)), retry_(std::move(retry))
{
    if (!result_)
        throw std::invalid_argument("Result Scene requires a play result.");
    context.canRetry = context.canRetry && static_cast<bool>(retry_);
    view_ = CreateResultView(*result_, visuals, texts, std::move(context));
}
ResultScene::~ResultScene() = default;
void ResultScene::Initialize(const EngineServices &services)
{
    view_->Initialize(services);
}
void ResultScene::BeginScene()
{
    view_->SetVisible(true);
}
void ResultScene::EndScene() noexcept
{
    view_->SetVisible(false);
}
void ResultScene::Update(const UpdateContext &context, SceneManager &scenes)
{
    const auto action = view_->Update(context.input);
    if (action == ResultAction::Retry && (!retry_ || !retry_(scenes)))
        throw std::runtime_error("Failed to retry the completed pattern.");
    if (action == ResultAction::Back && !scenes.ChangeScene(LobbySceneId))
        throw std::runtime_error("Failed to return from Result to song select.");
}
void ResultScene::Render(const RenderContext &)
{
}
void ResultScene::OnResize(std::uint32_t width, std::uint32_t height)
{
    view_->OnResize(width, height);
}
void ResultScene::Shutdown() noexcept
{
    view_->Shutdown();
}
