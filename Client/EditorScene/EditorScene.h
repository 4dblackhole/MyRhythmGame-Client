#pragma once
#include "GameFlow/GameplayLaunchRequest.h"
#include "MRG_Core.h"
#include <memory>

class EditorScene final : public mrg::scene::GameScene
{
  public:
    EditorScene(mrg::visual2d::ScreenVisual2DManager &visuals,
                std::shared_ptr<finger_drum::GameplayLaunchRequest> request);
    ~EditorScene() override;
    void Initialize(const mrg::EngineServices &services) override;
    void Update(const mrg::UpdateContext &context, mrg::scene::SceneManager &scenes) override;
    void Render(const mrg::graphics::RenderContext &) override;
    void OnResize(std::uint32_t width, std::uint32_t height) override;
    void Shutdown() noexcept override;

  private:
    struct State;
    std::unique_ptr<State> state_;
};
