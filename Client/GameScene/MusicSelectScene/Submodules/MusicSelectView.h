#pragma once
#include "MRG_Core.h"
#include "SongSelectionState.h"
#include "SongSelectPurpose.h"
#include <vector>

enum class SongSelectCommand
{
    None,
    Back,
    Launch
};
class MusicSelectView final
{
  public:
    MusicSelectView(mrg::visual2d::ScreenVisual2DManager &visuals, SongSelectionState &state,
                    SongSelectPurpose purpose)
        : selection_(state), purpose_(purpose), screenVisuals_(visuals)
    {
    }
    void SetVisible(bool visible)
    {
        static_cast<void>(canvasHandle_.SetVisible(visible));
    }
    void Initialize(const mrg::EngineServices &services);
    SongSelectCommand Update(const mrg::platform::InputState &input);
    void OnResize(const std::uint32_t width, const std::uint32_t height);
    void Shutdown() noexcept;
    void RebuildVisibleSongs();
    void RefreshSelectionPresentation();

  private:
    using SortMode = SongSelectionState::SortMode;
    struct SongCard
    {
        std::size_t catalogIndex{};
        float top{};
        float height{};
        mrg::visual2d::Visual2DNode *button{};
        mrg::visual2d::Visual2DNode *title{};
        mrg::visual2d::Visual2DNode *artist{};
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
    void ResizeBorderedPanel(mrg::visual2d::Visual2DNode &panel, const mrg::visual2d::Rect bounds,
                             const float penpotBorderWidth);
    void RebuildSongCards();
    void CreateSongCard(const std::size_t visiblePosition, const std::size_t catalogIndex,
                        const float top, const float height, const bool focused);
    void RefreshSearchPresentation();
    void EnsureFocusedCardVisible();
    void ApplyScrollOffset();
    void UpdateScrollbar();
    void ProcessPointer(const mrg::platform::InputState &input);
    bool ProcessActions();
    bool ProcessKeyboard(const mrg::platform::InputState &input);
    bool ProcessSearchKeyboard(const mrg::platform::InputState &input);
    void MoveSongFocus(const int delta);
    void MoveDifficultyFocus(const int delta);
    void SelectVisibleSong(const std::size_t visiblePosition);
    void SelectPattern(const std::size_t index);
    bool IsEditorSongSelect() const noexcept;
    mrg::visual2d::Visual2DNode &AddPanel(mrg::visual2d::Visual2DNode &parent,
                                          const mrg::visual2d::Rect penpotBounds,
                                          const mrg::visual2d::Color color, std::string name,
                                          const float penpotCornerRadius = 0.0F);
    mrg::visual2d::Visual2DNode &AddBorderedPanel(mrg::visual2d::Visual2DNode &parent,
                                                  const mrg::visual2d::Rect penpotBounds,
                                                  const mrg::visual2d::Color fill,
                                                  const mrg::visual2d::Color border,
                                                  const float penpotBorderWidth,
                                                  const float penpotCornerRadius, std::string name);
    mrg::visual2d::Visual2DNode &AddLabel(
        mrg::visual2d::Visual2DNode &parent, const mrg::visual2d::Rect penpotBounds,
        std::wstring text, const float penpotFontSize, const mrg::visual2d::Color color,
        std::string name,
        const mrg::visual2d::TextAlignment alignment = mrg::visual2d::TextAlignment::Leading);
    void RefreshSelection();
    SongSelectionState &selection_;
    SongSelectPurpose purpose_;
    SongSelectCommand command_{};
    mrg::visual2d::ScreenVisual2DManager &screenVisuals_;
    std::vector<SongCard> songCards_;
    std::vector<std::pair<mrg::visual2d::NodeId, std::size_t>> difficultyButtonIds_;
    float scrollOffset_{};
    float contentHeight_{};
    float songContentWidth_{454.29F};
    std::uint32_t width_{1280};
    std::uint32_t height_{720};
    bool searchFocused_{};
    mrg::visual2d::ScreenCanvasHandle canvasHandle_;
    mrg::visual2d::Visual2DCanvas *canvas_{};
    mrg::visual2d::Visual2DInputRouter inputRouter_;
    mrg::visual2d::Visual2DNode *board_{};
    mrg::visual2d::Visual2DNode *background_{};
    mrg::visual2d::Visual2DNode *categoryBar_{};
    mrg::visual2d::Visual2DNode *recordPanel_{};
    mrg::visual2d::Visual2DNode *recordSelector_{};
    mrg::visual2d::Visual2DNode *emptyRecordMessage_{};
    mrg::visual2d::Visual2DNode *informationPanel_{};
    mrg::visual2d::Visual2DNode *preview_{};
    mrg::visual2d::Visual2DNode *previewTitle_{};
    mrg::visual2d::Visual2DNode *previewEmpty_{};
    mrg::visual2d::Visual2DNode *difficultyInformation_{};
    mrg::visual2d::Visual2DNode *difficultyHeading_{};
    mrg::visual2d::Visual2DNode *creatorHeading_{};
    mrg::visual2d::Visual2DNode *informationDivider_{};
    mrg::visual2d::Visual2DNode *browserPanel_{};
    mrg::visual2d::Visual2DNode *songViewport_{};
    mrg::visual2d::Visual2DNode *songContent_{};
    mrg::visual2d::Visual2DNode *scrollbarTrack_{};
    mrg::visual2d::Visual2DNode *scrollbarHandle_{};
    mrg::visual2d::Visual2DNode *searchField_{};
    mrg::visual2d::Visual2DNode *searchCountLabel_{};
    mrg::visual2d::Visual2DNode *sortSelector_{};
    mrg::visual2d::Visual2DNode *browserHint_{};
    mrg::visual2d::Visual2DNode *selectedSongLabel_{};
    mrg::visual2d::Visual2DNode *selectedArtistLabel_{};
    mrg::visual2d::Visual2DNode *selectedPatternLabel_{};
    mrg::visual2d::Visual2DNode *selectedCreatorLabel_{};
    mrg::visual2d::Visual2DNode *selectedDetailsLabel_{};
    mrg::visual2d::Visual2DNode *optionButton_{};
    mrg::visual2d::Visual2DNode *optionLabel_{};
    mrg::visual2d::Visual2DNode *backButton_{};
    mrg::visual2d::Visual2DNode *backFill_{};
    mrg::visual2d::Visual2DNode *backLabel_{};
    mrg::visual2d::Visual2DNode *goButton_{};
    mrg::visual2d::Visual2DNode *goFill_{};
    mrg::visual2d::Visual2DNode *goLabel_{};
    mrg::visual2d::NodeId recordSelectorId_{};
    mrg::visual2d::NodeId sortSelectorId_{};
    mrg::visual2d::NodeId searchFieldId_{};
    mrg::visual2d::NodeId backButtonId_{};
    mrg::visual2d::NodeId optionButtonId_{};
    mrg::visual2d::NodeId goButtonId_{};
};
