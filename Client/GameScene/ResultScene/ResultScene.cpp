#include "ResultScene.h"
#include "GameFlow/FingerDrumSceneIds.h"
#include "Submodules/ResultViewFactory.h"
#include <stdexcept>
ResultScene::ResultScene(std::unique_ptr<finger_drum::mode::IPlayResult> result,
                         mrg::visual2d::ScreenVisual2DManager &visuals, finger_drum::texts::TextCatalog &texts)
    : result_(std::move(result))
{
    if (!result_)
        throw std::invalid_argument("Result Scene requires a play result.");
    view_ = CreateResultView(*result_, visuals, texts);
}
ResultScene::~ResultScene() = default;
void ResultScene::Initialize(const mrg::EngineServices &services)
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
void ResultScene::Update(const mrg::UpdateContext &context, mrg::scene::SceneManager &scenes)
{
    if (view_->Update(context.input) && !scenes.ChangeScene(finger_drum::scene_ids::Lobby))
        throw std::runtime_error("Failed to return from Result to song select.");
}
void ResultScene::Render(const mrg::graphics::RenderContext &)
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
