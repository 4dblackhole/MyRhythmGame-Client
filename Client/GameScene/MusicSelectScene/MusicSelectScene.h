#pragma once
#include "PhantomCore.h"
#include "GameFlow/GameplayLaunchStore.h"
#include "Submodules/SongSelectPurpose.h"
#include "Texts/TextCatalog.h"
#include "App/OptionSettings.h"
#include "Submodules/SongCatalogLoader.h"
#include "Submodules/EditorOsuImport.h"
#include <memory>

class SongSelectionState;
class MusicSelectView;
class SongPreviewController;

class MusicSelectScene final : public GameScene
{
  public:
    explicit MusicSelectScene(ScreenVisual2DManager &screenVisuals,
                              AudioPlaybackManager &audioPlayback,
                              std::shared_ptr<GameplayLaunchStore> launchRequest,
                              SongSelectPurpose purpose,
                              TextCatalog &texts,
                              OptionSettings &options);

    ~MusicSelectScene() override;

    void Initialize(const EngineServices &services) override;
    void BeginScene() override;
    void EndScene() noexcept override;
    void Update(const UpdateContext &context, SceneManager &scenes) override;
    void Render(const RenderContext &context) override;
    void OnResize(std::uint32_t width, std::uint32_t height) override;
    void Shutdown() noexcept override;

  private:
    bool StartSelectedPattern(SceneManager &scenes);
    void CreateDifficulty(SceneManager &scenes);
    void ConvertOsu();
    void PollOsuImport();
    void RefreshCatalog();
    std::shared_ptr<GameplayLaunchStore> launchRequest_;
    SongSelectPurpose purpose_;
    OptionSettings &options_;
    TextCatalog &texts_;
    std::unique_ptr<SongSelectionState> selection_;
    std::unique_ptr<MusicSelectView> view_;
    std::unique_ptr<SongPreviewController> preview_;
    bool active_{};
    bool firstEntry_{true};
    SongCatalogLoader catalogLoader_;
    std::filesystem::path catalogRoot_;
    EditorOsuImport osuImport_;
};
