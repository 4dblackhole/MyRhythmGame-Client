#pragma once
#include "PhantomCore.h"
#include "Presentation/OptionsPanel.h"
#include "SongSelectionState.h"
#include "SongSelectPurpose.h"
#include "Texts/TextCatalog.h"
#include <filesystem>
#include <vector>
#include <future>
#include "Records/PlayRecordStore.h"

enum class SongSelectCommand
{
    None,
    Back,
    Launch,
    CreateDifficulty,
    ConvertOsu
};
class MusicSelectView final
{
  public:
    MusicSelectView(ScreenVisual2DManager &visuals, SongSelectionState &state,
                    SongSelectPurpose purpose, TextCatalog &texts,
                    OptionSettings &options, std::filesystem::path recordDatabase = {})
        : selection_(state), purpose_(purpose), screenVisuals_(visuals), texts_(texts),
          settings_(options), options_(texts, options), recordDatabase_(std::move(recordDatabase))
    {
    }
    void SetVisible(bool visible)
    {
        if (!visible) CloseAdditionMenu();
        else ++recordRevision_;
        static_cast<void>(canvasHandle_.SetVisible(visible));
    }
    void Initialize(const EngineServices &services);
    SongSelectCommand Update(const InputState &input, double deltaSeconds);
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
        Visual2DNode *button{};
        Visual2DNode *title{};
        Visual2DNode *artist{};
        std::size_t visiblePosition{};
        NodeId createDifficultyId{};
    };

    void CreateCategoryBar();
    void CreateRecordPanel();
    void UpdateRecords();
    void ShowRecords(const std::vector<PlayRecord> &records);
    void EditRecordQuery();
    void LayoutRecordControls();
    void CreateSongInformationPanel();
    void SetBackgroundPreview(const std::filesystem::path &path);
    void UpdateBackgroundPreviewLayout();
    void CreateSongBrowser();
    void CreateFooter();
    void CreateAdditionMenu();
    void OpenAdditionMenu(Rect anchor);
    void CloseAdditionMenu();
    bool ProcessAdditionMenu(const InputState &input);
    void ApplyTexts();
    void UpdateResponsiveLayout();
    void UpdateRecordPanelLayout();
    void UpdateSongInformationLayout();
    void UpdateSongBrowserLayout();
    void UpdateFooterLayout();
    void ResizeBorderedPanel(Visual2DNode &panel, const Rect bounds,
                             const float penpotBorderWidth);
    void RebuildSongCards();
    void UpdateVisibleSongCards();
    void CreateSongCard(const std::size_t visiblePosition, const std::size_t catalogIndex,
                        const float top, const float height, const bool focused);
    NodeId CreateDifficultyButton(Visual2DNode &card, std::size_t visiblePosition);
    void RefreshSearchPresentation();
    void EnsureFocusedCardVisible();
    void ApplyScrollOffset();
    void UpdateScrollbar();
    void ProcessPointer(const InputState &input);
    bool ProcessActions();
    bool ProcessKeyboard(const InputState &input);
    bool ProcessSearchKeyboard(const InputState &input);
    void MoveSongFocus(const int delta);
    void MoveDifficultyFocus(const int delta);
    void SelectVisibleSong(const std::size_t visiblePosition);
    void SelectPattern(const std::size_t index);
    bool IsEditorSongSelect() const noexcept;
    Visual2DNode &AddPanel(Visual2DNode &parent,
                                          const Rect penpotBounds,
                                          const Color color, std::string name,
                                          const float penpotCornerRadius = 0.0F);
    Visual2DNode &AddBorderedPanel(Visual2DNode &parent,
                                                  const Rect penpotBounds,
                                                  const Color fill,
                                                  const Color border,
                                                  const float penpotBorderWidth,
                                                  const float penpotCornerRadius, std::string name);
    Visual2DNode &AddLabel(
        Visual2DNode &parent, const Rect penpotBounds,
        std::wstring text, const float penpotFontSize, const Color color,
        std::string name,
        const TextAlignment alignment = TextAlignment::Leading);
    void RefreshSelection();
    SongSelectionState &selection_;
    SongSelectPurpose purpose_;
    SongSelectCommand command_{};
    ScreenVisual2DManager &screenVisuals_;
    TextCatalog &texts_;
    OptionSettings &settings_;
    OptionsPanel options_;
    std::filesystem::path recordDatabase_;
    std::filesystem::path previewPath_;
    Size previewImageSize_{};
    std::uint64_t textRevision_{};
    std::vector<SongCard> songCards_;
    std::vector<SongCard> songRows_;
    std::size_t firstVisibleRow_{}, lastVisibleRow_{};
    bool songCardsDirty_{true};
    std::vector<std::pair<NodeId, std::size_t>> difficultyButtonIds_;
    float scrollOffset_{};
    float contentHeight_{};
    float songContentWidth_{454.29F};
    std::uint32_t width_{1280};
    std::uint32_t height_{720};
    bool searchFocused_{};
    Visual2DNode *additionMenu_{}, *newDifficultyItem_{}, *convertOsuItem_{};
    ScreenCanvasHandle canvasHandle_;
    Visual2DCanvas *canvas_{};
    Visual2DInputRouter inputRouter_;
    std::optional<Point> canvasPointer_;
    Visual2DNode *board_{};
    Visual2DNode *background_{};
    Visual2DNode *categoryBar_{};
    Visual2DNode *categoryLabel_{};
    std::future<std::vector<PlayRecord>> recordJob_;
    std::filesystem::path recordPattern_;
    std::string recordMode_, recordQuery_;
    std::size_t recordOffset_{};
    std::uint64_t recordRevision_{1}, recordJobRevision_{}, recordDisplayRevision_{};
    bool recordNext_{};
    Visual2DNode *recordQueryButton_{}, *recordPreviousButton_{}, *recordNextButton_{};
    Visual2DNode *recordRows_{};
    Visual2DNode *recordPanel_{};
    Visual2DNode *recordSelector_{};
    Visual2DNode *emptyRecordMessage_{};
    Visual2DNode *informationPanel_{};
    Visual2DNode *preview_{};
    Visual2DNode *previewImage_{};
    Visual2DNode *previewTitle_{};
    Visual2DNode *previewEmpty_{};
    Visual2DNode *difficultyInformation_{};
    Visual2DNode *difficultyHeading_{};
    Visual2DNode *creatorHeading_{};
    Visual2DNode *informationDivider_{};
    Visual2DNode *browserPanel_{};
    Visual2DNode *songViewport_{};
    Visual2DNode *songContent_{};
    Visual2DNode *scrollbarTrack_{};
    Visual2DNode *scrollbarHandle_{};
    Visual2DNode *searchField_{};
    Visual2DNode *searchCountLabel_{};
    Visual2DNode *sortSelector_{};
    Visual2DNode *browserHint_{};
    Visual2DNode *selectedSongLabel_{};
    Visual2DNode *selectedArtistLabel_{};
    Visual2DNode *selectedPatternLabel_{};
    Visual2DNode *selectedCreatorLabel_{};
    Visual2DNode *selectedDetailsLabel_{};
    Visual2DNode *modeSelector_{};
    NodeId modeSelectorId_{};
    Visual2DNode *optionButton_{};
    Visual2DNode *optionLabel_{};
    Visual2DNode *backButton_{};
    Visual2DNode *backFill_{};
    Visual2DNode *backLabel_{};
    Visual2DNode *goButton_{};
    Visual2DNode *goFill_{};
    Visual2DNode *goLabel_{};
    NodeId recordSelectorId_{};
    NodeId sortSelectorId_{};
    NodeId searchFieldId_{};
    NodeId backButtonId_{};
    NodeId optionButtonId_{};
    NodeId goButtonId_{};
};
