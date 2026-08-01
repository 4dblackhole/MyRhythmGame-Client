#pragma once

#include "MRG_Core.h"

#include <DirectXMath.h>

#include <array>
#include <cstdint>
#include <memory>

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
    void InitializeOptionsUi(const mrg::EngineServices& services);
    void UpdateOptionsUi(const mrg::UpdateContext& context);
    void ApplyUiActions();
    [[nodiscard]] std::int64_t LatestPointerTimestamp(
        const mrg::platform::InputState& input) const noexcept;

    std::uint32_t width_{1280};
    std::uint32_t height_{720};
    mrg::scene::Camera camera_;
    std::array<mrg::scene::MeshInstance, 4> cubes_;
    std::array<DirectX::XMFLOAT3, 4> initialRotations_{};
    std::array<DirectX::XMFLOAT3, 4> angularVelocities_{};
    mrg::graphics::D3D12UiRenderer uiRenderer_;
    std::unique_ptr<mrg::ui::WorldSpaceCanvas> optionsUi_;
    mrg::ui::UiInputRouter uiInput_;
    DirectX::XMFLOAT4X4 uiSurfaceWorld_{};
    mrg::ui::UiElementId rotationToggleId_{};
    mrg::ui::UiElementId speedSliderId_{};
    mrg::ui::UiElementId presentationComboId_{};
    float animationSeconds_{};
    float rotationSpeedScale_{1.0F};
    bool rotationEnabled_{true};
    bool worldSpaceUi_{};
};
