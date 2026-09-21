#pragma once

#include "MRG_Core.h"

#include "GameFlow/GameplayLaunchStore.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

// The Client root for FingerDrum.  The Engine owns the platform loop while
// this class selects the game window policy and registers game-owned Scenes.
class FingerDrumGame final : public mrg::scene::SceneGameClient
{
  public:
    FingerDrumGame(bool smokeTest, std::string initialSceneId,
                   bool rhythmDebugMode = false) noexcept;

    [[nodiscard]] mrg::EngineConfig GetEngineConfig() const override;

  protected:
    void RegisterScenes(mrg::scene::SceneManager &scenes) override;
    [[nodiscard]] std::string_view InitialSceneId() const noexcept override;
    void OnClientInitialized(const mrg::EngineServices &services) override;
    void OnClientUpdated(const mrg::UpdateContext &context) override;
    void OnClientRendered(const mrg::graphics::RenderContext &context) override;
    void OnClientShuttingDown() noexcept override;

  private:
    void RefreshPerformanceText(const mrg::PerformanceStatistics &performance);
    static void SubmitPerformanceLine(mrg::graphics::TextRenderSystem &textRendering,
                                      std::wstring_view text, const mrg::graphics::FontHandle &font,
                                      float layoutY, float viewportWidth);

    bool smokeTest_{};
    bool showPerformanceOverlay_{};
    bool rhythmDebugMode_{};
    std::string initialSceneId_;
    std::shared_ptr<finger_drum::GameplayLaunchStore> launchRequest_;
    mrg::graphics::FontHandle performanceFont_;
    std::uint64_t lastPerformanceMeasurementIndex_{};
    std::wstring framesPerSecondText_{L"FPS: measuring..."};
    std::wstring updatesPerSecondText_{L"UPS: measuring..."};
};
