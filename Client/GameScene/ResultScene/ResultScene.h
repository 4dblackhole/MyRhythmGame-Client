#pragma once
#include "MRG_Core.h"
#include "Mode/Results/IPlayResult.h"
#include "Submodules/IResultView.h"
#include "Texts/TextCatalog.h"
#include <functional>
#include <memory>
class IResultView;
class ResultScene final : public mrg::scene::GameScene
{
  public:
    using RetryHandler = std::function<bool(mrg::scene::SceneManager &)>;
    ResultScene(std::unique_ptr<finger_drum::mode::IPlayResult> result, mrg::visual2d::ScreenVisual2DManager &visuals,
                finger_drum::texts::TextCatalog &texts, ResultViewContext context = {}, RetryHandler retry = {});
    ~ResultScene() override;
    void Initialize(const mrg::EngineServices &services) override;
    void BeginScene() override;
    void EndScene() noexcept override;
    void Update(const mrg::UpdateContext &context, mrg::scene::SceneManager &scenes) override;
    void Render(const mrg::graphics::RenderContext &context) override;
    void OnResize(std::uint32_t width, std::uint32_t height) override;
    void Shutdown() noexcept override;

  private:
    // The view observes immutable data owned by this Scene. It is destroyed first.
    std::unique_ptr<finger_drum::mode::IPlayResult> result_;
    std::unique_ptr<IResultView> view_;
    RetryHandler retry_;
};
