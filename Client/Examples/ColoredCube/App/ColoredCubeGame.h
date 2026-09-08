#pragma once


#include "MRG_Core.h"

#include <cstdint>
#include <string>
#include <string_view>

class ColoredCubeGame final : public mrg::scene::SceneGameClient
{
public:
    explicit ColoredCubeGame(
        bool smokeTest,
        bool showPerformanceOverlay,
        std::string initialSceneId) noexcept;

    [[nodiscard]] mrg::EngineConfig GetEngineConfig() const override;

protected:
    void RegisterScenes(mrg::scene::SceneManager& scenes) override;
    [[nodiscard]] std::string_view InitialSceneId() const noexcept override;
    void OnClientInitialized(const mrg::EngineServices& services) override;
    void OnClientUpdated(const mrg::UpdateContext& context) override;
    void OnClientRendered(
        const mrg::graphics::RenderContext& context) override;
    void OnClientShuttingDown() noexcept override;

private:
    void BeginAudioPlaybackSmokeCheck(const mrg::EngineServices& services);
    void CompleteAudioPlaybackSmokeCheck();
    void RefreshPerformanceText(
        const mrg::PerformanceStatistics& performance);
    static void SubmitPerformanceLine(
        mrg::graphics::TextRenderSystem& textRendering,
        std::wstring_view text,
        const mrg::graphics::FontHandle& font,
        float fontSizePixels,
        const DirectX::XMFLOAT4& color,
        float layoutX,
        float layoutY,
        float layoutWidth,
        float lineHeight);

    bool smokeTest_{};
    mrg::audio::AudioPlaybackId audioSmokePlayback_{};
    bool audioSmokeSourceRendered_{};
    bool audioSmokeTransitionRequested_{};
    bool showPerformanceOverlay_{};
    std::string initialSceneId_;
    mrg::graphics::FontHandle framesPerSecondFont_;
    mrg::graphics::FontHandle updatesPerSecondFont_;
    std::uint64_t lastPerformanceMeasurementIndex_{};
    std::wstring framesPerSecondText_{L"FPS: measuring..."};
    std::wstring updatesPerSecondText_{L"UPS: measuring..."};
};
