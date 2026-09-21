#pragma once

#include "MRG_Core.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

// Presents the FingerDrum title screen. The authored image panels remain
// centered at every viewport aspect ratio, while one Canvas tree owns the
// mouse/keyboard menu and its movable selection cursor.
class LogoView;
class FingerDrumLogoScene final : public mrg::scene::GameScene
{
  public:
    explicit FingerDrumLogoScene(mrg::visual2d::ScreenVisual2DManager &screenVisuals);

    ~FingerDrumLogoScene() override;
    void Initialize(const mrg::EngineServices &services) override;
    void BeginScene() override;
    void EndScene() noexcept override;
    void Update(const mrg::UpdateContext &context, mrg::scene::SceneManager &scenes) override;
    void Render(const mrg::graphics::RenderContext &context) override;
    void OnResize(std::uint32_t width, std::uint32_t height) override;
    void Shutdown() noexcept override;

  private:
    std::unique_ptr<LogoView> view_;
};
