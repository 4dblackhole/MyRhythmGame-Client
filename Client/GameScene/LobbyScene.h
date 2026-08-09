#pragma once

#include "MRG_Core.h"

#include "Catalog/SongCatalog.h"
#include "GameFlow/GameplayLaunchRequest.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// Song selection owns only catalog presentation state. The selected runtime
// paths are copied to GameplayLaunchRequest immediately before entering the
// transient gameplay route.
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
    struct SongListRow
    {
        mrg::visual2d::Visual2DNode* button{};
        mrg::visual2d::Visual2DNode* title{};
        mrg::visual2d::Visual2DNode* artist{};
    };

    void CreateHeader(const mrg::EngineServices& services);
    void CreateCategoryBar();
    void CreateRecordPanel();
    void CreateSongInformationPanel();
    void CreatePatternPanel();
    void CreateSongList();
    void CreateSongListRow(
        mrg::visual2d::Visual2DNode& parent,
        std::size_t index,
        const finger_drum::chart::SongCatalogEntry& song);
    void ApplySongListRowStyle(SongListRow& row, bool selected);
    void CreateFooter();
    void RebuildPatternButtons();
    void RefreshSelectionPresentation();
    void ProcessPointer(const mrg::platform::InputState& input);
    [[nodiscard]] bool ProcessActions(mrg::scene::SceneManager& scenes);
    [[nodiscard]] bool ProcessKeyboard(
        const mrg::platform::InputState& input,
        mrg::scene::SceneManager& scenes);
    void SelectSong(std::size_t index);
    void SelectPattern(std::size_t index);
    [[nodiscard]] bool StartSelectedPattern(
        mrg::scene::SceneManager& scenes);
    [[nodiscard]] bool ReturnToLogo(mrg::scene::SceneManager& scenes) const;

    mrg::visual2d::Visual2DNode& AddPanel(
        mrg::visual2d::Visual2DNode& parent,
        mrg::visual2d::Rect penpotBounds,
        mrg::visual2d::Color color,
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
    std::size_t selectedSongIndex_{};
    std::size_t selectedPatternIndex_{};
    std::uint32_t width_{1280};
    std::uint32_t height_{720};
    std::unique_ptr<mrg::visual2d::Visual2DCanvas> canvas_;
    mrg::visual2d::Visual2DInputRouter inputRouter_;
    mrg::visual2d::Visual2DNode* board_{};
    mrg::visual2d::Visual2DNode* selectedSongLabel_{};
    mrg::visual2d::Visual2DNode* selectedArtistLabel_{};
    mrg::visual2d::Visual2DNode* selectedPatternLabel_{};
    mrg::visual2d::Visual2DNode* patternList_{};
    mrg::visual2d::NodeId backButtonId_{};
    mrg::visual2d::NodeId playButtonId_{};
    std::vector<SongListRow> songRows_;
    std::vector<mrg::visual2d::Visual2DNode*> patternButtons_;
    std::vector<mrg::visual2d::NodeId> patternButtonIds_;
};
