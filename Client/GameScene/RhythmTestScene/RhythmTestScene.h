#pragma once
#include "MRG_Core.h"
#include "GameFlow/GameplayLaunchStore.h"
#include "Texts/TextCatalog.h"
#include <memory>

class GameplaySessionController;
class GameplayPresenter;
class RhythmTestScene final : public mrg::scene::GameScene
{
  public:
    explicit RhythmTestScene(std::shared_ptr<finger_drum::GameplayLaunchStore> launchRequest,
                             mrg::audio::AudioPlaybackManager &playback,
                             mrg::visual2d::ScreenVisual2DManager &screenVisuals,
                             bool debugMode,
                             finger_drum::texts::TextCatalog &texts);

    ~RhythmTestScene() override;

    void Initialize(const mrg::EngineServices &services) override;
    void BeginScene() override;
    void EndScene() noexcept override;
    void Update(const mrg::UpdateContext &context, mrg::scene::SceneManager &scenes) override;
    void Render(const mrg::graphics::RenderContext &context) override;
    void OnResize(std::uint32_t width, std::uint32_t height) override;
    void Shutdown() noexcept override;

  private:
    std::unique_ptr<GameplaySessionController> controller_;
    std::unique_ptr<GameplayPresenter> view_;
};
