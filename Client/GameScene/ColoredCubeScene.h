#pragma once

#include "MRG_Core.h"

#include <DirectXMath.h>

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class ColoredCubeScene final : public mrg::scene::GameScene
{
public:
    explicit ColoredCubeScene(bool startWithWorldSpaceUi = false) noexcept;

    void Initialize(const mrg::EngineServices& services) override;
    void Update(
        const mrg::UpdateContext& context,
        mrg::scene::SceneManager& scenes) override;
    void Render(const mrg::graphics::RenderContext& context) override;
    void OnResize(std::uint32_t width, std::uint32_t height) override;
    void Shutdown() noexcept override;

private:
    void UpdateCamera(
        const mrg::UpdateContext& context,
        bool suppressMouseWheel);
    void InitializeOptionsUi(const mrg::EngineServices& services);
    void UpdateOptionsUi(
        const mrg::UpdateContext& context,
        bool allowPointerInput);
    void ApplyUiActions();
    void InitializeAudioOptionsUi(const mrg::EngineServices& services);
    [[nodiscard]] bool UpdateAudioOptionsUi(
        const mrg::UpdateContext& context);
    void UpdateAudioPanelMotion(const mrg::UpdateContext& context);
    void TryPlayPopSound(mrg::audio::AudioSystem& audio);
    [[nodiscard]] std::optional<mrg::visual2d::Point> MapAudioPanelPointer(
        const mrg::platform::InputState& input) const noexcept;
    void RefreshAudioDeviceChoices();
    void RefreshAudioBufferLengthChoice();
    void SelectAudioBackend(std::size_t backendIndex);
    void ApplyAudioDeviceSelection(std::size_t audioDeviceIndex);
    void ApplyAudioBufferLengthSelection(std::size_t bufferLengthIndex);
    [[nodiscard]] bool ReloadPopSound(std::string& errorMessage);
    void ApplyAudioUiActions();
    void SetAudioStatus(std::wstring text);
    [[nodiscard]] std::int64_t LatestPointerTimestamp(
        const mrg::platform::InputState& input) const noexcept;

    std::uint32_t width_{1280};
    std::uint32_t height_{720};
    mrg::scene::Camera camera_;
    std::array<mrg::scene::MeshInstance, 4> cubes_;
    std::array<DirectX::XMFLOAT3, 4> initialRotations_{};
    std::array<DirectX::XMFLOAT3, 4> angularVelocities_{};
    std::unique_ptr<mrg::visual2d::WorldSpaceVisual2DCanvas> optionsUi_;
    mrg::visual2d::Visual2DInputRouter uiInput_;
    mrg::graphics::RenderTargetTextureHandle optionsCanvasTexture_;
    // Hidden smoke tests render the same Canvas into a second target to
    // validate multiple Visual2D texture passes in one engine frame.
    mrg::graphics::RenderTargetTextureHandle visual2DValidationTexture_;
    mrg::scene::MeshInstance curvedOptionsSurface_;
    DirectX::XMFLOAT4X4 uiSurfaceWorld_{};
    mrg::visual2d::NodeId rotationToggleId_{};
    mrg::visual2d::NodeId speedSliderId_{};
    mrg::visual2d::NodeId presentationComboId_{};
    float animationSeconds_{};
    float rotationSpeedScale_{1.0F};
    bool rotationEnabled_{true};
    bool worldSpaceUi_{};
    bool validateMultipleVisual2DPasses_{};

    std::unique_ptr<mrg::visual2d::Visual2DCanvas> audioOptionsUi_;
    mrg::visual2d::Visual2DInputRouter audioUiInput_;
    std::vector<mrg::audio::AudioDeviceInfo> audioDevices_;
    mrg::audio::AudioSystem* audioSystem_{};
    std::unique_ptr<mrg::audio::AudioClip> popSound_;
    mrg::visual2d::NodeId audioBackendComboId_{};
    mrg::visual2d::NodeId audioDeviceComboId_{};
    mrg::visual2d::NodeId audioBufferLengthComboId_{};
    mrg::visual2d::NodeId audioStatusLabelId_{};
    mrg::visual2d::NodeId audioPanelId_{};
    mrg::audio::AudioOutputBackend selectedAudioBackend_{
        mrg::audio::AudioOutputBackend::Automatic};
    float audioPanelX_{-490.0F};
    bool audioPanelOpen_{};
};
