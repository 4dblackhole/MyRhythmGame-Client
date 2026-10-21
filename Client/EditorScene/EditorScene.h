#pragma once
#include "GameFlow/GameplayLaunchStore.h"
#include "PhantomCore.h"
#include "Texts/TextCatalog.h"
#include <memory>

class EditorWorkspace;
class EditorView;

class EditorScene final : public GameScene
{
  public:
    EditorScene(ScreenVisual2DManager &visuals,
                std::shared_ptr<GameplayLaunchStore> request,
                TextCatalog &texts);
    ~EditorScene() override;
    void Initialize(const EngineServices &services) override;
    void Update(const UpdateContext &context, SceneManager &scenes) override;
    void Render(const RenderContext &) override;
    void OnResize(std::uint32_t width, std::uint32_t height) override;
    void Shutdown() noexcept override;

  private:
    std::unique_ptr<EditorWorkspace> workspace_;
    std::unique_ptr<EditorView> view_;
};
