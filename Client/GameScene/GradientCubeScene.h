#pragma once

#include "MRG_Core.h"

#include <DirectXMath.h>

#include <array>
#include <cstddef>
#include <cstdint>

enum class GradientCubeTheme : std::uint8_t
{
    Blue,
    Red,
    Green,
};

// One-shot sample registered with SceneRetention::DestroyOnExit. It creates
// its mesh resources on entry and releases them after returning to the
// KeepAlive ColoredCubeScene.
class GradientCubeScene final : public mrg::scene::GameScene
{
public:
    GradientCubeScene(
        GradientCubeTheme theme,
        std::size_t cubeCount);

    void Initialize(const mrg::EngineServices& services) override;
    void Update(
        const mrg::UpdateContext& context,
        mrg::scene::SceneManager& scenes) override;
    void Render(const mrg::graphics::RenderContext& context) override;
    void OnResize(std::uint32_t width, std::uint32_t height) override;
    void Shutdown() noexcept override;

private:
    GradientCubeTheme theme_{GradientCubeTheme::Blue};
    std::size_t cubeCount_{};
    std::uint32_t width_{1280};
    std::uint32_t height_{720};
    mrg::scene::Camera camera_;
    std::array<mrg::scene::MeshInstance, 3> cubes_;
};
