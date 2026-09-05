#pragma once

#include "MRG_Core.h"

#include "Catalog/SongCatalog.h"
#include "GameFlow/GameplayLaunchRequest.h"

#include <cstddef>
#include <cstdint>
#include <memory>
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
        std::shared_ptr<finger_drum::GameplayLaunchRequest> launchRequest);

    void Initialize(const mrg::EngineServices& services) override;
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

    void CreateCategoryBar();
    void CreateRecordPanel();
    void CreateSongInformationPanel();
    void CreateSongBrowser();
    void CreateFooter();
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
    float scrollOffset_{};
    float contentHeight_{};
    std::uint32_t width_{1280};
    std::uint32_t height_{720};
    bool searchFocused_{};

    std::unique_ptr<mrg::visual2d::Visual2DCanvas> canvas_;
    mrg::visual2d::Visual2DInputRouter inputRouter_;
    mrg::visual2d::Visual2DNode* board_{};
    mrg::visual2d::Visual2DNode* songViewport_{};
    mrg::visual2d::Visual2DNode* songContent_{};
    mrg::visual2d::Visual2DNode* scrollbarHandle_{};
    mrg::visual2d::Visual2DNode* searchField_{};
    mrg::visual2d::Visual2DNode* searchCountLabel_{};
    mrg::visual2d::Visual2DNode* selectedSongLabel_{};
    mrg::visual2d::Visual2DNode* selectedArtistLabel_{};
    mrg::visual2d::Visual2DNode* selectedPatternLabel_{};
    mrg::visual2d::Visual2DNode* selectedCreatorLabel_{};
    mrg::visual2d::Visual2DNode* selectedDetailsLabel_{};
    mrg::visual2d::NodeId recordSelectorId_{};
    mrg::visual2d::NodeId sortSelectorId_{};
    mrg::visual2d::NodeId searchFieldId_{};
    mrg::visual2d::NodeId backButtonId_{};
    mrg::visual2d::NodeId optionButtonId_{};
    mrg::visual2d::NodeId goButtonId_{};
};
