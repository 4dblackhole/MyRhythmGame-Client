#pragma once

#include "MRG_Core.h"

#include <cstdint>
#include <memory>

// Presents the first FingerDrum screen. The three authored image panels are
// laid out as a single centered strip and uniformly scaled to remain visible
// at every supported viewport aspect ratio.
class FingerDrumLogoScene final : public mrg::scene::GameScene
{
public:
    void Initialize(const mrg::EngineServices& services) override;
    void Update(
        const mrg::UpdateContext& context,
        mrg::scene::SceneManager& scenes) override;
    void Render(const mrg::graphics::RenderContext& context) override;
    void OnResize(std::uint32_t width, std::uint32_t height) override;
    void Shutdown() noexcept override;

private:
    void CreateLogoStrip(const mrg::EngineServices& services);
    void UpdateLogoStripLayout();

    std::uint32_t width_{1280};
    std::uint32_t height_{720};
    std::unique_ptr<mrg::visual2d::Visual2DCanvas> canvas_;
    mrg::visual2d::Visual2DNode* logoStrip_{};
    mrg::visual2d::Visual2DNode* leftFade_{};
    mrg::visual2d::Visual2DNode* centerLogo_{};
    mrg::visual2d::Visual2DNode* rightFade_{};
};
