#pragma once

#include "MRG_Core.h"

#include <DirectXMath.h>

#include <array>
#include <cstdint>

class ColoredCubeScene final : public mrg::scene::GameScene
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
    void UpdateCamera(const mrg::UpdateContext& context);

    std::uint32_t width_{1280};
    std::uint32_t height_{720};
    mrg::scene::Camera camera_;
    std::array<mrg::scene::MeshInstance, 4> cubes_;
    std::array<DirectX::XMFLOAT3, 4> initialRotations_{};
    std::array<DirectX::XMFLOAT3, 4> angularVelocities_{};
};
