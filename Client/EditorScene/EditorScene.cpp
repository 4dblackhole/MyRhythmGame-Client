#include "EditorScene.h"
#include "Submodules/EditorWorkspace.h"
#include "Submodules/EditorView.h"
#include "GameFlow/FingerDrumSceneIds.h"
#include <stdexcept>

EditorScene::EditorScene(mrg::visual2d::ScreenVisual2DManager &visuals,
                         std::shared_ptr<finger_drum::GameplayLaunchStore> request)
    : workspace_(std::make_unique<EditorWorkspace>(
          request ? request->Snapshot()
                  : throw std::invalid_argument("Editor requires a launch store."))),
      view_(std::make_unique<EditorView>(visuals, *workspace_))
{
}
EditorScene::~EditorScene() = default;
void EditorScene::Initialize(const mrg::EngineServices &services)
{
    workspace_->Initialize();
    view_->Initialize(services);
}
void EditorScene::Update(const mrg::UpdateContext &context, mrg::scene::SceneManager &scenes)
{
    try
    {
        workspace_->UpdateAnalysis();
    }
    catch (const std::exception &error)
    {
        workspace_->status = error.what();
        workspace_->rebuild = true;
    }
    if (view_->Update(context))
        static_cast<void>(scenes.ChangeScene(finger_drum::scene_ids::EditorSongSelect));
}
void EditorScene::OnResize(std::uint32_t width, std::uint32_t height)
{
    view_->Resize(width, height);
}
void EditorScene::Shutdown() noexcept
{
    workspace_->analysis.Stop();
    view_->Shutdown();
}
void EditorScene::Render(const mrg::graphics::RenderContext &)
{
}
