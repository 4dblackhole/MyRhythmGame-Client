#include "MusicSelectView.h"
#include "SongSelectLayout.h"
#include "SongSelectionText.h"

using namespace song_select;

void MusicSelectView::CreateCategoryBar()
{
    auto &bar = AddBorderedPanel(*board_, layout::CategoryBar, PanelWhite, BorderBlue, 3.0F, 20.0F,
                                 "Categories");
    categoryBar_ = &bar;
    auto &all = AddPanel(bar, layout::AllCategory, AccentBlue, "AllCategory", 19.0F);
    AddLabel(all, {0.0F, 0.0F, 86.0F, 38.0F}, L"ALL", 13.0F, PureWhite, "AllCategoryText",
             mrg::visual2d::TextAlignment::Center);
}

void MusicSelectView::CreateRecordPanel()
{
    auto &panel = AddBorderedPanel(*board_, layout::RecordPanel, PanelWhite, BorderBlue, 3.0F,
                                   24.0F, "RecordPanel");
    recordPanel_ = &panel;
    auto &selector = mrg::visual2d::CreateComboBox(
        panel, ScaleTopLeftBounds(layout::RecordSelector, panel.NodeSize().height),
        {L"PERSONAL RECORD"}, "RecordSelector");
    recordSelector_ = &selector;
    recordSelectorId_ = selector.Id();
    ApplySpriteStyle(selector, PaleBlue, PaleBlue);
    SetCornerRadius(selector, 10.0F);
    auto &recordBehavior = RequireComponent<mrg::visual2d::ComboBoxBehaviorComponent>(selector);
    recordBehavior.SetFontSize(CanvasFontSize(10.0F));
    recordBehavior.SetTextColor(DeepBlue);
    recordBehavior.SetSelectedTextColor(PureWhite);
    recordBehavior.SetPopupBackgroundColor(PureWhite);
    emptyRecordMessage_ = &AddLabel(panel, layout::EmptyRecordMessage, L"NO RECORDS", 28.0F,
                                    DeepBlue, "NoRecords", mrg::visual2d::TextAlignment::Center);
}

void MusicSelectView::CreateSongInformationPanel()
{
    auto &panel = AddBorderedPanel(*board_, layout::InformationPanel, PanelWhite, BorderBlue, 3.0F,
                                   24.0F, "SongInformation");
    informationPanel_ = &panel;
    auto &preview = AddPanel(panel, layout::Preview, {0.765F, 0.906F, 0.980F, 1.0F},
                             "BackgroundPreview", 12.0F);
    preview_ = &preview;
    previewTitle_ = &AddLabel(preview, layout::PreviewTitle, L"BACKGROUND PREVIEW", 13.0F,
                              MutedBlue, "PreviewTitle", mrg::visual2d::TextAlignment::Center);
    previewEmpty_ = &AddLabel(preview, layout::PreviewEmpty, L"NO IMAGE", 10.0F, SoftTextBlue,
                              "PreviewEmpty", mrg::visual2d::TextAlignment::Center);

    selectedSongLabel_ = &AddLabel(panel, layout::SelectedSong, L"NO SONG SELECTED", 28.0F,
                                   DeepBlue, "SelectedSong");
    selectedSongLabel_->AddComponent<finger_drum::presentation::MarqueeTextComponent>(
        L"NO SONG SELECTED", 24, 0.28);
    selectedArtistLabel_ =
        &AddLabel(panel, layout::SelectedArtist, L"", 16.0F, MutedBlue, "SelectedArtist");
    selectedArtistLabel_->AddComponent<finger_drum::presentation::MarqueeTextComponent>(L"", 34,
                                                                                        0.32);

    auto &information = AddBorderedPanel(panel, layout::DifficultyInformation, PaleBlue, PaleBorder,
                                         1.0F, 12.0F, "DifficultyInformation");
    difficultyInformation_ = &information;
    difficultyHeading_ = &AddLabel(information, layout::DifficultyHeading, L"DIFFICULTY", 9.0F,
                                   SoftTextBlue, "DifficultyHeading");
    creatorHeading_ = &AddLabel(information, layout::CreatorHeading, L"CREATOR", 9.0F, SoftTextBlue,
                                "CreatorHeading");
    selectedPatternLabel_ = &AddLabel(information, layout::SelectedDifficulty, L"—", 18.0F,
                                      AccentBlue, "SelectedDifficulty");
    selectedCreatorLabel_ =
        &AddLabel(information, layout::SelectedCreator, L"—", 18.0F, DeepBlue, "SelectedCreator");
    informationDivider_ =
        &AddPanel(information, layout::InformationDivider, PaleBorder, "InformationDivider");
    selectedDetailsLabel_ =
        &AddLabel(information, layout::SelectedDetails, L"LEVEL —   BPM —   NOTES —   MODE —", 9.0F,
                  MutedBlue, "SelectedDetails", mrg::visual2d::TextAlignment::Center);
}

void MusicSelectView::CreateSongBrowser()
{
    auto &panel = AddBorderedPanel(*board_, layout::BrowserPanel, PanelWhite, BorderBlue, 3.0F,
                                   24.0F, "SongBrowser");
    browserPanel_ = &panel;

    auto &search = mrg::visual2d::CreateButton(
        panel, ScaleTopLeftBounds(layout::SearchField, panel.NodeSize().height), L"SEARCH SONGS",
        "SearchField");
    searchField_ = &search;
    searchFieldId_ = search.Id();
    ApplyButtonStyle(search, PureWhite, PaleBlue, SoftTextBlue);
    SetCornerRadius(search, 12.0F);
    auto &searchText = RequireComponent<mrg::visual2d::TextVisualComponent>(search);
    searchText.SetFontSize(CanvasFontSize(11.0F));
    searchText.SetHorizontalAlignment(mrg::visual2d::TextAlignment::Leading);
    searchText.SetContentBounds(
        ScaleTopLeftBounds({14.0F, 0.0F, 340.0F, 54.0F}, search.NodeSize().height));
    searchCountLabel_ = &AddLabel(search, layout::SearchCount, L"0 SONGS", 9.0F, SoftTextBlue,
                                  "SearchCount", mrg::visual2d::TextAlignment::Trailing);

    auto &sort = mrg::visual2d::CreateComboBox(
        panel, ScaleTopLeftBounds(layout::SortSelector, panel.NodeSize().height),
        {L"난이도순", L"곡 이름순", L"아티스트 이름순"}, "SortSelector");
    sortSelector_ = &sort;
    sortSelectorId_ = sort.Id();
    sort.SetZIndex(10);
    ApplySpriteStyle(sort, PaleBlue, FocusBlue);
    SetCornerRadius(sort, 10.0F);
    auto &sortBehavior = RequireComponent<mrg::visual2d::ComboBoxBehaviorComponent>(sort);
    sortBehavior.SetItemHeight(38.0F * DesignToCanvasScale);
    sortBehavior.SetMaxVisibleItems(3);
    sortBehavior.SetFontSize(CanvasFontSize(12.0F));
    sortBehavior.SetTextColor(DeepBlue);
    sortBehavior.SetSelectedTextColor(PureWhite);
    sortBehavior.SetPopupBackgroundColor(PureWhite);

    auto &viewport = AddBorderedPanel(panel, layout::SongViewport, ViewportBlue, SubtleBorder, 1.5F,
                                      14.0F, "SongViewport");
    songViewport_ = &viewport;
    viewport.SetClipRect({0.0F, 0.0F, viewport.NodeSize().width, viewport.NodeSize().height});
    songContent_ = &viewport.CreateChild("SongContent");
    songContent_->SetSize(viewport.NodeSize());
    songContent_->SetPosition({0.0F, 0.0F});
    songContent_->SetZIndex(1);
    scrollbarTrack_ = &AddPanel(viewport, layout::ScrollbarTrack, {0.835F, 0.910F, 0.957F, 1.0F},
                                "ScrollbarTrack", 3.0F);
    scrollbarTrack_->SetZIndex(2);
    scrollbarHandle_ =
        &AddPanel(viewport, layout::ScrollbarTrack, AccentBlue, "ScrollbarHandle", 3.0F);
    scrollbarHandle_->SetZIndex(3);

    browserHint_ = &AddLabel(panel, layout::BrowserHint,
                             L"← / →  MOVE SONG     ↑ / ↓  DIFFICULTY     ENTER  GO", 9.0F,
                             SoftTextBlue, "BrowserHint", mrg::visual2d::TextAlignment::Center);
}

void MusicSelectView::CreateFooter()
{
    auto &option = AddBorderedPanel(*board_, layout::OptionButton, PureWhite, BorderBlue, 3.0F,
                                    12.0F, "OptionSelect");
    optionButton_ = &option;
    optionLabel_ = &AddLabel(option, {0.0F, 0.0F, 696.0F, 60.0F}, L"OPTION SELECT", 14.0F,
                             MutedBlue, "OptionLabel", mrg::visual2d::TextAlignment::Center);
    optionButtonId_ = option.Id();

    struct EdgeButtonNodes
    {
        mrg::visual2d::NodeId id{};
        mrg::visual2d::Visual2DNode *button{};
        mrg::visual2d::Visual2DNode *fill{};
        mrg::visual2d::Visual2DNode *label{};
    };
    auto createEdgeButton = [this](const mrg::visual2d::Rect bounds,
                                   const mrg::visual2d::Rect labelBounds, const std::wstring &text,
                                   const std::string &name) -> EdgeButtonNodes {
        auto &button = mrg::visual2d::CreateButton(
            *board_, ScaleTopLeftBounds(bounds, board_->NodeSize().height), L"", name);
        ApplyButtonStyle(button, PureWhite, PureWhite, PureWhite);
        SetCornerRadius(button, 28.0F);
        auto &fill = AddPanel(button, {4.0F, 4.0F, bounds.width - 8.0F, bounds.height - 8.0F},
                              AccentBlue, name + ".Fill", 24.0F);
        auto &label = AddLabel(button, labelBounds, text, 27.0F, PureWhite, name + ".Label",
                               mrg::visual2d::TextAlignment::Center);
        return {button.Id(), &button, &fill, &label};
    };
    const EdgeButtonNodes back =
        createEdgeButton(layout::BackButton, layout::BackLabel, L"BACK", "Back");
    backButtonId_ = back.id;
    backButton_ = back.button;
    backFill_ = back.fill;
    backLabel_ = back.label;
    const EdgeButtonNodes go = createEdgeButton(layout::GoButton, layout::GoLabel, L"GO", "Go");
    goButtonId_ = go.id;
    goButton_ = go.button;
    goFill_ = go.fill;
    goLabel_ = go.label;
}

mrg::visual2d::Visual2DNode &MusicSelectView::AddPanel(mrg::visual2d::Visual2DNode &parent,
                                                       const mrg::visual2d::Rect penpotBounds,
                                                       const mrg::visual2d::Color color,
                                                       std::string name,
                                                       const float penpotCornerRadius)
{
    auto &panel = mrg::visual2d::CreatePanel(
        parent, ScaleTopLeftBounds(penpotBounds, parent.NodeSize().height), std::move(name));
    ApplyFlatStyle(panel, color);
    SetCornerRadius(panel, penpotCornerRadius);
    return panel;
}

mrg::visual2d::Visual2DNode &MusicSelectView::AddBorderedPanel(
    mrg::visual2d::Visual2DNode &parent, const mrg::visual2d::Rect penpotBounds,
    const mrg::visual2d::Color fill, const mrg::visual2d::Color border,
    const float penpotBorderWidth, const float penpotCornerRadius, std::string name)
{
    auto &outer = AddPanel(parent, penpotBounds, border, std::move(name), penpotCornerRadius);
    AddPanel(outer,
             {penpotBorderWidth, penpotBorderWidth,
              std::max(penpotBounds.width - penpotBorderWidth * 2.0F, 0.0F),
              std::max(penpotBounds.height - penpotBorderWidth * 2.0F, 0.0F)},
             fill, "Surface", std::max(penpotCornerRadius - penpotBorderWidth, 0.0F));
    return outer;
}

mrg::visual2d::Visual2DNode &MusicSelectView::AddLabel(
    mrg::visual2d::Visual2DNode &parent, const mrg::visual2d::Rect penpotBounds, std::wstring text,
    const float penpotFontSize, const mrg::visual2d::Color color, std::string name,
    const mrg::visual2d::TextAlignment alignment)
{
    auto &label = mrg::visual2d::CreateLabel(
        parent, ScaleTopLeftBounds(penpotBounds, parent.NodeSize().height), std::move(text),
        std::move(name));
    auto &textComponent = RequireComponent<mrg::visual2d::TextVisualComponent>(label);
    textComponent.SetFontSize(CanvasFontSize(penpotFontSize));
    textComponent.SetTextColor(color);
    textComponent.SetHorizontalAlignment(alignment);
    return label;
}
