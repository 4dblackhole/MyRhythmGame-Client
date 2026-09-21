#pragma once
#include "GameFlow/GameplayLaunchStore.h"
#include "MRG_Core.h"
#include <memory>

class EditorWorkspace;
class EditorView;

class EditorScene final : public mrg::scene::GameScene
{
  public:
    EditorScene(mrg::visual2d::ScreenVisual2DManager &visuals,
                std::shared_ptr<finger_drum::GameplayLaunchStore> request);
    ~EditorScene() override;
    void Initialize(const mrg::EngineServices &services) override;
    void Update(const mrg::UpdateContext &context, mrg::scene::SceneManager &scenes) override;
    void Render(const mrg::graphics::RenderContext &) override;
    void OnResize(std::uint32_t width, std::uint32_t height) override;
    void Shutdown() noexcept override;

  private:
    std::unique_ptr<EditorWorkspace> workspace_;
    std::unique_ptr<EditorView> view_;
};
