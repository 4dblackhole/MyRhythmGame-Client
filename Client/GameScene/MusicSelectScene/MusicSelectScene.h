#pragma once
#include "MRG_Core.h"
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

class MusicSelectScene final : public mrg::scene::GameScene
{
  public:
    explicit MusicSelectScene(mrg::visual2d::ScreenVisual2DManager &screenVisuals,
                              mrg::audio::AudioPlaybackManager &audioPlayback,
                              std::shared_ptr<finger_drum::GameplayLaunchStore> launchRequest,
                              SongSelectPurpose purpose,
                              finger_drum::texts::TextCatalog &texts,
                              mrg_client::OptionSettings &options);

    ~MusicSelectScene() override;

    void Initialize(const mrg::EngineServices &services) override;
    void BeginScene() override;
    void EndScene() noexcept override;
    void Update(const mrg::UpdateContext &context, mrg::scene::SceneManager &scenes) override;
    void Render(const mrg::graphics::RenderContext &context) override;
    void OnResize(std::uint32_t width, std::uint32_t height) override;
    void Shutdown() noexcept override;

  private:
    bool StartSelectedPattern(mrg::scene::SceneManager &scenes);
    void CreateDifficulty(mrg::scene::SceneManager &scenes);
    void ConvertOsu();
    void PollOsuImport();
    std::shared_ptr<finger_drum::GameplayLaunchStore> launchRequest_;
    SongSelectPurpose purpose_;
    mrg_client::OptionSettings &options_;
    finger_drum::texts::TextCatalog &texts_;
    std::unique_ptr<SongSelectionState> selection_;
    std::unique_ptr<MusicSelectView> view_;
    std::unique_ptr<SongPreviewController> preview_;
    bool active_{};
    bool firstEntry_{true};
    SongCatalogLoader catalogLoader_;
    EditorOsuImport osuImport_;
};
