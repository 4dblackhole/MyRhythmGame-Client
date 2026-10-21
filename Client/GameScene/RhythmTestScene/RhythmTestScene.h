#pragma once
#include "PhantomCore.h"
#include "GameFlow/GameplayLaunchStore.h"
#include "Texts/TextCatalog.h"
#include "Mode/Results/IPlayResult.h"
#include <memory>

class IPlaySessionController;
class GameplayPresenter;
class GameplayPauseMenu;
class RhythmTestScene final : public GameScene
{
  public:
    explicit RhythmTestScene(std::shared_ptr<GameplayLaunchStore> launchRequest,
                             AudioPlaybackManager &playback,
                             ScreenVisual2DManager &screenVisuals,
                             bool debugMode,
                             TextCatalog &texts,
                             std::unique_ptr<IPlayResult> &pendingResult);

    ~RhythmTestScene() override;

    void Initialize(const EngineServices &services) override;
    void BeginScene() override;
    void EndScene() noexcept override;
    void Update(const UpdateContext &context, SceneManager &scenes) override;
    void Render(const RenderContext &context) override;
    void OnResize(std::uint32_t width, std::uint32_t height) override;
    void Shutdown() noexcept override;

  private:
    void ShowResult(SceneManager &scenes);
    void UpdatePauseMenu(const UpdateContext &context, SceneManager &scenes);
    bool PreparePresentation();
    std::unique_ptr<IPlayResult> &pendingResult_;
    std::unique_ptr<IPlaySessionController> controller_;
    std::unique_ptr<GameplayPresenter> view_;
    std::unique_ptr<GameplayPauseMenu> pauseMenu_;
    std::optional<EngineServices> services_;
    bool presentationReady_{};
    double nextPreparationPoll_{};
};
