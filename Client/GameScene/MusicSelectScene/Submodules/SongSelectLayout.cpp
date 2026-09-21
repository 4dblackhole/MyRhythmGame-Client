#include "MusicSelectView.h"
#include "SongSelectLayout.h"
#include "SongSelectionText.h"

using namespace song_select;

void MusicSelectView::UpdateResponsiveLayout()
{
    if (canvas_ == nullptr || board_ == nullptr || background_ == nullptr ||
        categoryBar_ == nullptr || (!IsEditorSongSelect() && recordPanel_ == nullptr) ||
        informationPanel_ == nullptr || browserPanel_ == nullptr || optionButton_ == nullptr ||
        backButton_ == nullptr || goButton_ == nullptr)
    {
        return;
    }

    const mrg::visual2d::Size canvasSize = canvas_->LogicalSize();
    const ResponsiveSongSelectLayout responsive =
        CalculateResponsiveLayout(canvasSize.width, IsEditorSongSelect());
    board_->SetSize(canvasSize);
    board_->SetPosition({0.0F, 0.0F});
    background_->SetBounds(responsive.background);

    ResizeBorderedPanel(*categoryBar_, responsive.category, 3.0F);
    if (recordPanel_ != nullptr)
    {
        ResizeBorderedPanel(*recordPanel_, responsive.record, 3.0F);
    }
    ResizeBorderedPanel(*informationPanel_, responsive.information, 3.0F);
    ResizeBorderedPanel(*browserPanel_, responsive.browser, 3.0F);
    categoryBar_->SetClipRect(
        {0.0F, 0.0F, categoryBar_->NodeSize().width, categoryBar_->NodeSize().height});
    if (recordPanel_ != nullptr)
    {
        recordPanel_->SetClipRect(
            {0.0F, 0.0F, recordPanel_->NodeSize().width, recordPanel_->NodeSize().height});
    }
    informationPanel_->SetClipRect(
        {0.0F, 0.0F, informationPanel_->NodeSize().width, informationPanel_->NodeSize().height});
    browserPanel_->SetClipRect(
        {0.0F, 0.0F, browserPanel_->NodeSize().width, browserPanel_->NodeSize().height});

    UpdateRecordPanelLayout();
    UpdateSongInformationLayout();
    UpdateSongBrowserLayout();

    ResizeBorderedPanel(*optionButton_, responsive.option, 3.0F);
    backButton_->SetBounds(responsive.back);
    goButton_->SetBounds(responsive.go);
    UpdateFooterLayout();
}

void MusicSelectView::UpdateRecordPanelLayout()
{
    if (recordPanel_ == nullptr || recordSelector_ == nullptr || emptyRecordMessage_ == nullptr)
    {
        return;
    }
    const float panelWidth = recordPanel_->NodeSize().width / DesignToCanvasScale;
    const float innerWidth = std::max(panelWidth - 44.0F, 1.0F);
    recordSelector_->SetBounds(
        ScaleTopLeftBounds({22.0F, 22.0F, innerWidth, layout::RecordSelector.height},
                           recordPanel_->NodeSize().height));
    emptyRecordMessage_->SetBounds(
        ScaleTopLeftBounds({22.0F, 400.0F, innerWidth, layout::EmptyRecordMessage.height},
                           recordPanel_->NodeSize().height));
}

void MusicSelectView::UpdateSongInformationLayout()
{
    if (informationPanel_ == nullptr || preview_ == nullptr || previewTitle_ == nullptr ||
        previewEmpty_ == nullptr || selectedSongLabel_ == nullptr ||
        selectedArtistLabel_ == nullptr || difficultyInformation_ == nullptr ||
        difficultyHeading_ == nullptr || creatorHeading_ == nullptr ||
        selectedPatternLabel_ == nullptr || selectedCreatorLabel_ == nullptr ||
        informationDivider_ == nullptr || selectedDetailsLabel_ == nullptr)
    {
        return;
    }

    const float panelWidth = informationPanel_->NodeSize().width / DesignToCanvasScale;
    const bool showPreview = panelWidth >= 500.0F;
    preview_->SetVisible(showPreview);

    float detailsX = 24.0F;
    if (showPreview)
    {
        const float previewWidth =
            std::clamp(layout::Preview.width * panelWidth / layout::InformationPanel.width, 160.0F,
                       layout::Preview.width);
        preview_->SetBounds(ScaleTopLeftBounds({24.0F, 24.0F, previewWidth, previewWidth},
                                               informationPanel_->NodeSize().height));
        previewTitle_->SetBounds(ScaleTopLeftBounds(
            {20.0F, layout::PreviewTitle.y * previewWidth / layout::Preview.width,
             std::max(previewWidth - 40.0F, 1.0F), layout::PreviewTitle.height},
            preview_->NodeSize().height));
        previewEmpty_->SetBounds(ScaleTopLeftBounds(
            {20.0F, layout::PreviewEmpty.y * previewWidth / layout::Preview.width,
             std::max(previewWidth - 40.0F, 1.0F), layout::PreviewEmpty.height},
            preview_->NodeSize().height));
        detailsX += previewWidth + 20.0F;
    }

    const float detailsWidth = std::max(panelWidth - detailsX - 24.0F, 1.0F);
    selectedSongLabel_->SetBounds(ScaleTopLeftBounds(
        {detailsX, layout::SelectedSong.y, detailsWidth, layout::SelectedSong.height},
        informationPanel_->NodeSize().height));
    selectedArtistLabel_->SetBounds(ScaleTopLeftBounds(
        {detailsX, layout::SelectedArtist.y, detailsWidth, layout::SelectedArtist.height},
        informationPanel_->NodeSize().height));
    ResizeBorderedPanel(*difficultyInformation_,
                        ScaleTopLeftBounds({detailsX, layout::DifficultyInformation.y, detailsWidth,
                                            layout::DifficultyInformation.height},
                                           informationPanel_->NodeSize().height),
                        1.0F);

    const float headingGap = std::clamp((detailsWidth - 32.0F) * 0.03F, 2.0F, 8.71F);
    const float headingWidth = std::max((detailsWidth - 32.0F - headingGap) * 0.5F, 1.0F);
    const float creatorX = 16.0F + headingWidth + headingGap;
    difficultyHeading_->SetBounds(ScaleTopLeftBounds(
        {16.0F, layout::DifficultyHeading.y, headingWidth, layout::DifficultyHeading.height},
        difficultyInformation_->NodeSize().height));
    creatorHeading_->SetBounds(ScaleTopLeftBounds(
        {creatorX, layout::CreatorHeading.y, headingWidth, layout::CreatorHeading.height},
        difficultyInformation_->NodeSize().height));
    selectedPatternLabel_->SetBounds(ScaleTopLeftBounds(
        {16.0F, layout::SelectedDifficulty.y, headingWidth, layout::SelectedDifficulty.height},
        difficultyInformation_->NodeSize().height));
    selectedCreatorLabel_->SetBounds(ScaleTopLeftBounds(
        {creatorX, layout::SelectedCreator.y, headingWidth, layout::SelectedCreator.height},
        difficultyInformation_->NodeSize().height));
    const float informationInnerWidth = std::max(detailsWidth - 32.0F, 1.0F);
    informationDivider_->SetBounds(
        ScaleTopLeftBounds({16.0F, layout::InformationDivider.y, informationInnerWidth,
                            layout::InformationDivider.height},
                           difficultyInformation_->NodeSize().height));
    selectedDetailsLabel_->SetBounds(ScaleTopLeftBounds(
        {16.0F, layout::SelectedDetails.y, informationInnerWidth, layout::SelectedDetails.height},
        difficultyInformation_->NodeSize().height));
}

void MusicSelectView::UpdateSongBrowserLayout()
{
    if (browserPanel_ == nullptr || searchField_ == nullptr || searchCountLabel_ == nullptr ||
        sortSelector_ == nullptr || songViewport_ == nullptr || songContent_ == nullptr ||
        scrollbarTrack_ == nullptr || scrollbarHandle_ == nullptr || browserHint_ == nullptr)
    {
        return;
    }

    const float panelWidth = browserPanel_->NodeSize().width / DesignToCanvasScale;
    const float innerWidth = std::max(panelWidth - 28.0F, 1.0F);
    searchField_->SetBounds(
        ScaleTopLeftBounds({14.0F, layout::SearchField.y, innerWidth, layout::SearchField.height},
                           browserPanel_->NodeSize().height));
    auto &searchText = RequireComponent<mrg::visual2d::TextVisualComponent>(*searchField_);
    const float countWidth = std::min(96.0F, innerWidth);
    const float searchTextWidth = std::max(innerWidth - countWidth - 18.0F, 1.0F);
    searchText.SetContentBounds(
        ScaleTopLeftBounds({14.0F, 0.0F, searchTextWidth, layout::SearchField.height},
                           searchField_->NodeSize().height));
    searchCountLabel_->SetBounds(
        ScaleTopLeftBounds({std::max(innerWidth - countWidth - 17.0F, 0.0F), 0.0F, countWidth,
                            layout::SearchCount.height},
                           searchField_->NodeSize().height));

    sortSelector_->SetBounds(
        ScaleTopLeftBounds({14.0F, layout::SortSelector.y, innerWidth, layout::SortSelector.height},
                           browserPanel_->NodeSize().height));
    ResizeBorderedPanel(
        *songViewport_,
        ScaleTopLeftBounds({14.0F, layout::SongViewport.y, innerWidth, layout::SongViewport.height},
                           browserPanel_->NodeSize().height),
        1.5F);
    songViewport_->SetClipRect(
        {0.0F, 0.0F, songViewport_->NodeSize().width, songViewport_->NodeSize().height});
    songContent_->SetSize(songViewport_->NodeSize());
    songContentWidth_ = std::max(innerWidth - 24.0F, 1.0F);

    const float trackX = std::max(innerWidth - 9.0F, 0.0F);
    scrollbarTrack_->SetBounds(
        ScaleTopLeftBounds({trackX, layout::ScrollbarTrack.y, layout::ScrollbarTrack.width,
                            layout::ScrollbarTrack.height},
                           songViewport_->NodeSize().height));
    browserHint_->SetBounds(
        ScaleTopLeftBounds({18.0F, layout::BrowserHint.y, std::max(panelWidth - 36.0F, 1.0F),
                            layout::BrowserHint.height},
                           browserPanel_->NodeSize().height));
    UpdateScrollbar();
}

void MusicSelectView::UpdateFooterLayout()
{
    if (optionButton_ == nullptr || optionLabel_ == nullptr || backButton_ == nullptr ||
        backFill_ == nullptr || backLabel_ == nullptr || goButton_ == nullptr ||
        goFill_ == nullptr || goLabel_ == nullptr)
    {
        return;
    }
    optionLabel_->SetBounds(
        {0.0F, 0.0F, optionButton_->NodeSize().width, optionButton_->NodeSize().height});

    const auto resizeEdgeButton = [](mrg::visual2d::Visual2DNode &button,
                                     mrg::visual2d::Visual2DNode &fill,
                                     mrg::visual2d::Visual2DNode &label) {
        const mrg::visual2d::Size buttonSize = button.NodeSize();
        const float padding = std::min(
            {4.0F * DesignToCanvasScale, buttonSize.width * 0.1F, buttonSize.height * 0.1F});
        fill.SetBounds({padding, padding, std::max(buttonSize.width - padding * 2.0F, 1.0F),
                        std::max(buttonSize.height - padding * 2.0F, 1.0F)});
        label.SetBounds({0.0F, 0.0F, buttonSize.width, buttonSize.height});
    };
    resizeEdgeButton(*backButton_, *backFill_, *backLabel_);
    resizeEdgeButton(*goButton_, *goFill_, *goLabel_);
}

void MusicSelectView::ResizeBorderedPanel(mrg::visual2d::Visual2DNode &panel,
                                          const mrg::visual2d::Rect bounds,
                                          const float penpotBorderWidth)
{
    panel.SetBounds(bounds);
    const float borderWidth = penpotBorderWidth * DesignToCanvasScale;
    for (const auto &child : panel.Children())
    {
        if (child->Name() == "Surface")
        {
            child->SetBounds({borderWidth, borderWidth,
                              std::max(panel.NodeSize().width - borderWidth * 2.0F, 1.0F),
                              std::max(panel.NodeSize().height - borderWidth * 2.0F, 1.0F)});
            break;
        }
    }
}
