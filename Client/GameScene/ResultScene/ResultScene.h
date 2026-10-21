#pragma once
#include "PhantomCore.h"
#include "Mode/Results/IPlayResult.h"
#include "Submodules/IResultView.h"
#include "Texts/TextCatalog.h"
#include <functional>
#include <memory>
class IResultView;
class ResultScene final : public GameScene
{
  public:
    using RetryHandler = std::function<bool(SceneManager &)>;
    ResultScene(std::unique_ptr<IPlayResult> result, ScreenVisual2DManager &visuals,
                TextCatalog &texts, ResultViewContext context = {}, RetryHandler retry = {});
    ~ResultScene() override;
    void Initialize(const EngineServices &services) override;
    void BeginScene() override;
    void EndScene() noexcept override;
    void Update(const UpdateContext &context, SceneManager &scenes) override;
    void Render(const RenderContext &context) override;
    void OnResize(std::uint32_t width, std::uint32_t height) override;
    void Shutdown() noexcept override;

  private:
    // The view observes immutable data owned by this Scene. It is destroyed first.
    std::unique_ptr<IPlayResult> result_;
    std::unique_ptr<IResultView> view_;
    RetryHandler retry_;
};
