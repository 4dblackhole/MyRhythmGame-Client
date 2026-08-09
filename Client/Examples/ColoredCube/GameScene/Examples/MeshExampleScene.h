#pragma once

#include "MRG_Core.h"

#include <array>
#include <cstdint>

// Demonstrates the renderer-neutral Shape -> GPU mesh -> MeshInstance flow.
// The Scene is registered as DestroyOnExit, so its GPU handles are recreated
// for every entry and released when Space returns to the main Scene.
class MeshExampleScene final : public mrg::scene::GameScene
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
    void SubmitHelpText() const;

    std::uint32_t width_{1280};
    std::uint32_t height_{720};
    float animationSeconds_{};
    mrg::scene::Camera camera_;
    std::array<mrg::scene::MeshInstance, 3> meshes_;
    mrg::graphics::TextRenderSystem* textRendering_{};
    mrg::graphics::FontHandle helpFont_;
};
