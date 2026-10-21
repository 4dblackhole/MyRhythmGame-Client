#pragma once

#include "PhantomCore.h"

#include "GameFlow/GameplayLaunchStore.h"
#include "Texts/TextCatalog.h"
#include "App/OptionSettings.h"
#include "Mode/Results/IPlayResult.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

// The Client root for YOUMU.  The Engine owns the platform loop while
// this class selects the game window policy and registers game-owned Scenes.
class YOUMUGame final : public SceneGameClient
{
  public:
    YOUMUGame(bool smokeTest, std::string initialSceneId,
                   bool rhythmDebugMode = false);

    [[nodiscard]] EngineConfig GetEngineConfig() const override;

  protected:
    void RegisterScenes(SceneManager &scenes) override;
    [[nodiscard]] std::string_view InitialSceneId() const noexcept override;
    void OnClientInitialized(const EngineServices &services) override;
    void OnClientUpdated(const UpdateContext &context) override;
    void OnClientRendered(const RenderContext &context) override;
    void OnClientShuttingDown() noexcept override;

  private:
    void RefreshPerformanceText(const PerformanceStatistics &performance);
    std::unique_ptr<GameScene> CreateResultScene();
    void RestoreSavedAudioDriver(AudioSystem &audio);
    static void SubmitPerformanceLine(TextRenderSystem &textRendering,
                                      std::wstring_view text, const FontHandle &font,
                                      float layoutY, float viewportWidth);

    bool smokeTest_{};
    bool showPerformanceOverlay_{};
    bool rhythmDebugMode_{};
    std::string initialSceneId_;
    std::shared_ptr<GameplayLaunchStore> launchRequest_;
    std::unique_ptr<IPlayResult> pendingResult_;
    TextCatalog texts_;
    OptionSettings optionSettings_;
    FontHandle performanceFont_;
    std::uint64_t lastPerformanceMeasurementIndex_{};
    std::wstring framesPerSecondText_;
    std::wstring updatesPerSecondText_;
};
