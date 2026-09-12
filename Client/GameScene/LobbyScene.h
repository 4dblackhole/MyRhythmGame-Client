#pragma once

#include "MRG_Core.h"

#include "Catalog/SongCatalog.h"
#include "GameFlow/GameplayLaunchRequest.h"

#include <cstddef>
#include <cstdint>
#include <array>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

// Presents the Penpot song browser. Only the focused song expands, and the
// selected catalog paths are copied to GameplayLaunchRequest before entering
// the transient gameplay Scene.
class LobbyScene final : public mrg::scene::GameScene
{
public:
    explicit LobbyScene(
        mrg::visual2d::ScreenVisual2DManager& screenVisuals,
        mrg::audio::AudioPlaybackManager& audioPlayback,
        std::shared_ptr<finger_drum::GameplayLaunchRequest> launchRequest);

    void Initialize(const mrg::EngineServices& services) override;
    void BeginScene() override;
    void EndScene() noexcept override;
    void Update(
        const mrg::UpdateContext& context,
        mrg::scene::SceneManager& scenes) override;
    void Render(const mrg::graphics::RenderContext& context) override;
    void OnResize(std::uint32_t width, std::uint32_t height) override;
    void Shutdown() noexcept override;

private:
    enum class SortMode : std::size_t
    {
        Difficulty,
        Title,
        Artist,
    };

    struct SongCard
    {
        std::size_t catalogIndex{};
        float top{};
        float height{};
        mrg::visual2d::Visual2DNode* button{};
        mrg::visual2d::Visual2DNode* title{};
        mrg::visual2d::Visual2DNode* artist{};
    };

    struct PreviewSlot
    {
        std::shared_ptr<mrg::audio::AudioClip> clip;
        mrg::audio::AudioPlaybackId playbackId{
            mrg::audio::InvalidAudioPlaybackId};
        std::optional<std::size_t> catalogIndex;
        float volume{};
        float targetVolume{};
    };

    void CreateCategoryBar();
    void CreateRecordPanel();
    void CreateSongInformationPanel();
    void CreateSongBrowser();
    void CreateFooter();
    void UpdateResponsiveLayout();
    void UpdateRecordPanelLayout();
    void UpdateSongInformationLayout();
    void UpdateSongBrowserLayout();
    void UpdateFooterLayout();
    void ResizeBorderedPanel(
        mrg::visual2d::Visual2DNode& panel,
        mrg::visual2d::Rect bounds,
        float penpotBorderWidth);
    void RebuildVisibleSongs();
    void RebuildSongCards();
    void CreateSongCard(
        std::size_t visiblePosition,
        std::size_t catalogIndex,
        float top,
        float height,
        bool focused);
    void RefreshSelectionPresentation();
    void RefreshSearchPresentation();
    void EnsureFocusedCardVisible();
    void ApplyScrollOffset();
    void UpdateScrollbar();
    void ProcessPointer(const mrg::platform::InputState& input);
    [[nodiscard]] bool ProcessActions(mrg::scene::SceneManager& scenes);
    [[nodiscard]] bool ProcessKeyboard(
        const mrg::platform::InputState& input,
        mrg::scene::SceneManager& scenes);
    [[nodiscard]] bool ProcessSearchKeyboard(
        const mrg::platform::InputState& input);
    void MoveSongFocus(int delta);
    void MoveDifficultyFocus(int delta);
    void SelectVisibleSong(std::size_t visiblePosition);
    void SelectPattern(std::size_t index);
    void SyncPreviewToFocusedSong();
    void StartSongPreview(std::size_t catalogIndex);
    void UpdatePreviewAudio(double deltaSeconds);
    void StopPreviewAudio() noexcept;
    void StopPreviewSlot(PreviewSlot& slot) noexcept;
    [[nodiscard]] bool StartSelectedPattern(
        mrg::scene::SceneManager& scenes);
    [[nodiscard]] bool ReturnToLogo(mrg::scene::SceneManager& scenes) const;

    mrg::visual2d::Visual2DNode& AddPanel(
        mrg::visual2d::Visual2DNode& parent,
        mrg::visual2d::Rect penpotBounds,
        mrg::visual2d::Color color,
        std::string name,
        float penpotCornerRadius = 0.0F);
    mrg::visual2d::Visual2DNode& AddBorderedPanel(
        mrg::visual2d::Visual2DNode& parent,
        mrg::visual2d::Rect penpotBounds,
        mrg::visual2d::Color fill,
        mrg::visual2d::Color border,
        float penpotBorderWidth,
        float penpotCornerRadius,
        std::string name);
    mrg::visual2d::Visual2DNode& AddLabel(
        mrg::visual2d::Visual2DNode& parent,
        mrg::visual2d::Rect penpotBounds,
        std::wstring text,
        float penpotFontSize,
        mrg::visual2d::Color color,
        std::string name,
        mrg::visual2d::TextAlignment alignment =
            mrg::visual2d::TextAlignment::Leading);

    std::shared_ptr<finger_drum::GameplayLaunchRequest> launchRequest_;
    finger_drum::chart::SongCatalogLoadResult catalog_;
    std::vector<std::size_t> visibleSongIndices_;
    std::vector<SongCard> songCards_;
    std::vector<std::pair<mrg::visual2d::NodeId, std::size_t>>
        difficultyButtonIds_;
    std::size_t focusedSongPosition_{};
    std::size_t selectedPatternIndex_{};
    SortMode sortMode_{SortMode::Difficulty};
    std::wstring searchText_;
    std::wstring launchError_;
    float scrollOffset_{};
    float contentHeight_{};
    float songContentWidth_{454.29F};
    std::uint32_t width_{1280};
    std::uint32_t height_{720};
    bool searchFocused_{};
    bool sceneActive_{};

    mrg::visual2d::ScreenVisual2DManager& screenVisuals_;
    mrg::audio::AudioPlaybackManager& audioPlayback_;
    mrg::audio::AudioSystem* audioSystem_{};
    std::array<PreviewSlot, 2> previewSlots_{};
    std::optional<std::size_t> currentPreviewSlot_;
    mrg::visual2d::ScreenCanvasHandle canvasHandle_;
    mrg::visual2d::Visual2DCanvas* canvas_{};
    mrg::visual2d::Visual2DInputRouter inputRouter_;
    mrg::visual2d::Visual2DNode* board_{};
    mrg::visual2d::Visual2DNode* background_{};
    mrg::visual2d::Visual2DNode* categoryBar_{};
    mrg::visual2d::Visual2DNode* recordPanel_{};
    mrg::visual2d::Visual2DNode* recordSelector_{};
    mrg::visual2d::Visual2DNode* emptyRecordMessage_{};
    mrg::visual2d::Visual2DNode* informationPanel_{};
    mrg::visual2d::Visual2DNode* preview_{};
    mrg::visual2d::Visual2DNode* previewTitle_{};
    mrg::visual2d::Visual2DNode* previewEmpty_{};
    mrg::visual2d::Visual2DNode* difficultyInformation_{};
    mrg::visual2d::Visual2DNode* difficultyHeading_{};
    mrg::visual2d::Visual2DNode* creatorHeading_{};
    mrg::visual2d::Visual2DNode* informationDivider_{};
    mrg::visual2d::Visual2DNode* browserPanel_{};
    mrg::visual2d::Visual2DNode* songViewport_{};
    mrg::visual2d::Visual2DNode* songContent_{};
    mrg::visual2d::Visual2DNode* scrollbarTrack_{};
    mrg::visual2d::Visual2DNode* scrollbarHandle_{};
    mrg::visual2d::Visual2DNode* searchField_{};
    mrg::visual2d::Visual2DNode* searchCountLabel_{};
    mrg::visual2d::Visual2DNode* sortSelector_{};
    mrg::visual2d::Visual2DNode* browserHint_{};
    mrg::visual2d::Visual2DNode* selectedSongLabel_{};
    mrg::visual2d::Visual2DNode* selectedArtistLabel_{};
    mrg::visual2d::Visual2DNode* selectedPatternLabel_{};
    mrg::visual2d::Visual2DNode* selectedCreatorLabel_{};
    mrg::visual2d::Visual2DNode* selectedDetailsLabel_{};
    mrg::visual2d::Visual2DNode* optionButton_{};
    mrg::visual2d::Visual2DNode* optionLabel_{};
    mrg::visual2d::Visual2DNode* backButton_{};
    mrg::visual2d::Visual2DNode* backFill_{};
    mrg::visual2d::Visual2DNode* backLabel_{};
    mrg::visual2d::Visual2DNode* goButton_{};
    mrg::visual2d::Visual2DNode* goFill_{};
    mrg::visual2d::Visual2DNode* goLabel_{};
    mrg::visual2d::NodeId recordSelectorId_{};
    mrg::visual2d::NodeId sortSelectorId_{};
    mrg::visual2d::NodeId searchFieldId_{};
    mrg::visual2d::NodeId backButtonId_{};
    mrg::visual2d::NodeId optionButtonId_{};
    mrg::visual2d::NodeId goButtonId_{};
};
