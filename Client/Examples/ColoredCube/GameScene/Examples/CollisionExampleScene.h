#pragma once

#include "MRG_Core.h"

#include <cstdint>

// Keeps rendered transforms and backend-neutral collision primitives in the
// same Scene state. The colors provide immediate feedback from Intersects.
class CollisionExampleScene final : public mrg::scene::GameScene
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
    void UpdateCollision(double totalSeconds);
    void SubmitHelpText() const;

    std::uint32_t width_{1280};
    std::uint32_t height_{720};
    float manualOffset_{};
    bool colliding_{};
    mrg::scene::Camera camera_;
    mrg::scene::MeshInstance movingSphere_;
    mrg::scene::MeshInstance fixedBox_;
    DirectX::XMFLOAT4 fixedBoxOrientation_{0.0F, 0.0F, 0.0F, 1.0F};
    mrg::graphics::TextRenderSystem* textRendering_{};
    mrg::graphics::FontHandle helpFont_;
};
