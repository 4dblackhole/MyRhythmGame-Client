#include "MusicSelectView.h"
#include "SongSelectLayout.h"
#include "SongSelectionText.h"
#include "Texts/GameScene/MusicSelectScene/MusicSelectTexts.h"

using namespace song_select;

void MusicSelectView::CreateCategoryBar()
{
    const auto &text = MusicSelect(texts_);
    auto &bar = AddBorderedPanel(*board_, layout::CategoryBar, PanelWhite, BorderBlue, 3.0F, 20.0F,
                                 "Categories");
    categoryBar_ = &bar;
    auto &all = AddPanel(bar, layout::AllCategory, AccentBlue, "AllCategory", 19.0F);
    categoryLabel_ = &AddLabel(
        all, {0.0F, 0.0F, 86.0F, 38.0F}, std::wstring(text.all), 13.0F, PureWhite,
        "AllCategoryText", TextAlignment::Center);
}

void MusicSelectView::CreateRecordPanel()
{
    const auto &text = MusicSelect(texts_);
    auto &panel = AddBorderedPanel(*board_, layout::RecordPanel, PanelWhite, BorderBlue, 3.0F,
                                   24.0F, "RecordPanel");
    recordPanel_ = &panel;
    auto &selector = CreateComboBox(
        panel, ScaleTopLeftBounds(layout::RecordSelector, panel.NodeSize().height),
        {std::wstring(text.personalRecord)}, "RecordSelector");
    recordSelector_ = &selector;
    recordSelectorId_ = selector.Id();
    ApplySpriteStyle(selector, PaleBlue, PaleBlue);
    SetCornerRadius(selector, 10.0F);
    BindSkinImage(selector, screenVisuals_, "MusicSelect", "RecordSelector");
    auto &recordBehavior = RequireComponent<ComboBoxBehaviorComponent>(selector);
    recordBehavior.SetFontSize(CanvasFontSize(10.0F));
    texts_.ApplyFont(recordBehavior);
    recordBehavior.SetTextColor(DeepBlue);
    recordBehavior.SetSelectedTextColor(PureWhite);
    recordBehavior.SetPopupBackgroundColor(PureWhite);
    emptyRecordMessage_ = &AddLabel(
        panel, layout::EmptyRecordMessage, std::wstring(text.noRecords), 28.0F, DeepBlue,
        "NoRecords", TextAlignment::Center);
}

void MusicSelectView::CreateSongInformationPanel()
{
    const auto &text = MusicSelect(texts_);
    auto &panel = AddBorderedPanel(*board_, layout::InformationPanel, PanelWhite, BorderBlue, 3.0F,
                                   24.0F, "SongInformation");
    informationPanel_ = &panel;
    auto &preview = AddPanel(panel, layout::Preview, {0.765F, 0.906F, 0.980F, 1.0F},
                             "BackgroundPreview", 12.0F);
    preview_ = &preview;
    previewImage_ = &preview.CreateChild("PreviewImage");
    previewImage_->AddComponent<SpriteVisualComponent>();
    SetCornerRadius(*previewImage_, 12.0F);
    previewImage_->SetVisible(false);
    previewTitle_ = &AddLabel(preview, layout::PreviewTitle, std::wstring(text.backgroundPreview), 13.0F,
                              MutedBlue, "PreviewTitle", TextAlignment::Center);
    previewEmpty_ = &AddLabel(preview, layout::PreviewEmpty, std::wstring(text.noImage), 10.0F, SoftTextBlue,
                              "PreviewEmpty", TextAlignment::Center);

    selectedSongLabel_ = &AddLabel(panel, layout::SelectedSong, std::wstring(text.noSongSelected), 28.0F,
                                   DeepBlue, "SelectedSong");
    selectedSongLabel_->AddComponent<MarqueeTextComponent>(std::wstring(text.noSongSelected),
                                                                                      0.28);
    selectedArtistLabel_ =
        &AddLabel(panel, layout::SelectedArtist, L"", 16.0F, MutedBlue, "SelectedArtist");
    selectedArtistLabel_->AddComponent<MarqueeTextComponent>(L"", 0.32);

    auto &information = AddBorderedPanel(panel, layout::DifficultyInformation, PaleBlue, PaleBorder,
                                         1.0F, 12.0F, "DifficultyInformation");
    difficultyInformation_ = &information;
    difficultyHeading_ = &AddLabel(information, layout::DifficultyHeading, std::wstring(text.difficulty), 9.0F,
                                   SoftTextBlue, "DifficultyHeading");
    creatorHeading_ = &AddLabel(information, layout::CreatorHeading, std::wstring(text.creator), 9.0F, SoftTextBlue,
                                "CreatorHeading");
    selectedPatternLabel_ = &AddLabel(information, layout::SelectedDifficulty, L"—", 18.0F,
                                      AccentBlue, "SelectedDifficulty");
    selectedCreatorLabel_ =
        &AddLabel(information, layout::SelectedCreator, L"—", 18.0F, DeepBlue, "SelectedCreator");
    informationDivider_ =
        &AddPanel(information, layout::InformationDivider, PaleBorder, "InformationDivider");
    selectedDetailsLabel_ =
        &AddLabel(information, layout::SelectedDetails, std::wstring(text.emptyDetails), 9.0F,
                  MutedBlue, "SelectedDetails", TextAlignment::Center);
}

void MusicSelectView::SetBackgroundPreview(const std::filesystem::path &path)
{
    if (previewPath_ == path)
        return;
    previewPath_ = path;
    previewImageSize_ = {};
    previewImage_->SetVisible(false);
    RequireComponent<SpriteVisualComponent>(*previewImage_).SetImage({});
    previewTitle_->SetVisible(true);
    previewEmpty_->SetVisible(true);
    if (path.empty())
        return;
    try
    {
        const auto image = screenVisuals_.RegisterImage(previewPath_);
        const auto size = screenVisuals_.GetImageSize(image);
        if (image.value == 0 || size.width <= 0 || size.height <= 0)
            return;
        previewImageSize_ = size;
        auto &sprite = RequireComponent<SpriteVisualComponent>(*previewImage_);
        sprite.SetImage(image);
        sprite.SetTint(PureWhite);
        UpdateBackgroundPreviewLayout();
        previewImage_->SetVisible(true);
        previewTitle_->SetVisible(false);
        previewEmpty_->SetVisible(false);
    }
    catch (const std::exception &)
    {
        // Missing/corrupt artwork keeps NO IMAGE until the selected path changes.
    }
}

void MusicSelectView::UpdateBackgroundPreviewLayout()
{
    if (previewImage_ == nullptr)
        return;
    const auto size = preview_->NodeSize();
    previewImage_->SetBounds({0, 0, size.width, size.height});
    if (previewImageSize_.width <= 0 || previewImageSize_.height <= 0)
        return;
    const auto scale = std::max(size.width / previewImageSize_.width,
                                size.height / previewImageSize_.height);
    const DirectX::XMFLOAT2 uvScale{size.width / (previewImageSize_.width * scale),
                                   size.height / (previewImageSize_.height * scale)};
    RequireComponent<SpriteVisualComponent>(*previewImage_).SetUvTransform(
        uvScale, {(1 - uvScale.x) * .5F, (1 - uvScale.y) * .5F});
}

void MusicSelectView::CreateSongBrowser()
{
    const auto &text = MusicSelect(texts_);
    auto &panel = AddBorderedPanel(*board_, layout::BrowserPanel, PanelWhite, BorderBlue, 3.0F,
                                   24.0F, "SongBrowser");
    browserPanel_ = &panel;

    auto &search = CreateButton(
        panel, ScaleTopLeftBounds(layout::SearchField, panel.NodeSize().height),
        std::wstring(text.searchSongs),
        "SearchField");
    searchField_ = &search;
    searchFieldId_ = search.Id();
    ApplyButtonStyle(search, PureWhite, PaleBlue, SoftTextBlue);
    SetCornerRadius(search, 12.0F);
    BindSkinImage(search, screenVisuals_, "MusicSelect", "SearchField");
    auto &searchText = RequireComponent<TextVisualComponent>(search);
    searchText.SetFontSize(CanvasFontSize(11.0F));
    texts_.ApplyFont(searchText);
    searchText.SetHorizontalAlignment(TextAlignment::Leading);
    searchText.SetContentBounds(
        ScaleTopLeftBounds({14.0F, 0.0F, 340.0F, 54.0F}, search.NodeSize().height));
    searchCountLabel_ = &AddLabel(search, layout::SearchCount, L"", 9.0F, SoftTextBlue,
                                  "SearchCount", TextAlignment::Trailing);

    auto &sort = CreateComboBox(
        panel, ScaleTopLeftBounds(layout::SortSelector, panel.NodeSize().height),
        {std::wstring(text.sortModes[0]), std::wstring(text.sortModes[1]),
         std::wstring(text.sortModes[2])}, "SortSelector");
    sortSelector_ = &sort;
    sortSelectorId_ = sort.Id();
    sort.SetZIndex(10);
    ApplySpriteStyle(sort, PaleBlue, FocusBlue);
    SetCornerRadius(sort, 10.0F);
    BindSkinImage(sort, screenVisuals_, "MusicSelect", "SortSelector");
    auto &sortBehavior = RequireComponent<ComboBoxBehaviorComponent>(sort);
    sortBehavior.SetItemHeight(38.0F * DesignToCanvasScale);
    sortBehavior.SetMaxVisibleItems(3);
    sortBehavior.SetFontSize(CanvasFontSize(12.0F));
    texts_.ApplyFont(sortBehavior);
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

    browserHint_ = &AddLabel(panel, layout::BrowserHint, std::wstring(text.browserHint), 9.0F,
                             SoftTextBlue, "BrowserHint", TextAlignment::Center);
}

void MusicSelectView::CreateFooter()
{
    const auto &text = MusicSelect(texts_);
    auto &option = AddBorderedPanel(*board_, layout::OptionButton, PureWhite, BorderBlue, 3.0F,
                                    12.0F, "OptionSelect");
    optionButton_ = &option;
    option.AddComponent<RectangleCollider2DComponent>();
    option.AddComponent<PointerReceiverComponent>();
    option.AddComponent<ButtonBehaviorComponent>();
    optionLabel_ = &AddLabel(option, {0.0F, 0.0F, 696.0F, 60.0F},
                             std::wstring(text.optionSelect), 14.0F,
                             MutedBlue, "OptionLabel", TextAlignment::Center);
    optionButtonId_ = option.Id();

    struct EdgeButtonNodes
    {
        NodeId id{};
        Visual2DNode *button{};
        Visual2DNode *fill{};
        Visual2DNode *label{};
    };
    auto createEdgeButton = [this](const Rect bounds,
                                   const Rect labelBounds, const std::wstring &text,
                                   const std::string &name) -> EdgeButtonNodes {
        auto &button = CreateButton(
            *board_, ScaleTopLeftBounds(bounds, board_->NodeSize().height), L"", name);
        ApplyButtonStyle(button, PureWhite, PureWhite, PureWhite);
        SetCornerRadius(button, 28.0F);
        auto &fill = AddPanel(button, {4.0F, 4.0F, bounds.width - 8.0F, bounds.height - 8.0F},
                              AccentBlue, name + ".Fill", 24.0F);
        auto &label = AddLabel(button, labelBounds, text, 27.0F, PureWhite, name + ".Label",
                               TextAlignment::Center);
        BindSkinImage(button, screenVisuals_, "MusicSelect", name, name + ".Fill");
        return {button.Id(), &button, &fill, &label};
    };
    const EdgeButtonNodes back =
        createEdgeButton(layout::BackButton, layout::BackLabel, std::wstring(text.back), "Back");
    backButtonId_ = back.id;
    backButton_ = back.button;
    backFill_ = back.fill;
    backLabel_ = back.label;
    const EdgeButtonNodes go =
        createEdgeButton(layout::GoButton, layout::GoLabel, std::wstring(text.go), "Go");
    goButtonId_ = go.id;
    goButton_ = go.button;
    goFill_ = go.fill;
    goLabel_ = go.label;
}

Visual2DNode &MusicSelectView::AddPanel(Visual2DNode &parent,
                                                       const Rect penpotBounds,
                                                       const Color color,
                                                       std::string name,
                                                       const float penpotCornerRadius)
{
    auto &panel = CreatePanel(
        parent, ScaleTopLeftBounds(penpotBounds, parent.NodeSize().height), std::move(name));
    ApplyFlatStyle(panel, color);
    SetCornerRadius(panel, penpotCornerRadius);
    if (panel.Name() != "Surface" && !panel.Name().ends_with(".Fill"))
        BindSkinImage(panel, screenVisuals_, "MusicSelect", panel.Name());
    return panel;
}

Visual2DNode &MusicSelectView::AddBorderedPanel(
    Visual2DNode &parent, const Rect penpotBounds,
    const Color fill, const Color border,
    const float penpotBorderWidth, const float penpotCornerRadius, std::string name)
{
    auto &outer = AddPanel(parent, penpotBounds, border, std::move(name), penpotCornerRadius);
    AddPanel(outer,
             {penpotBorderWidth, penpotBorderWidth,
              std::max(penpotBounds.width - penpotBorderWidth * 2.0F, 0.0F),
              std::max(penpotBounds.height - penpotBorderWidth * 2.0F, 0.0F)},
             fill, "Surface", std::max(penpotCornerRadius - penpotBorderWidth, 0.0F));
    BindSkinImage(outer, screenVisuals_, "MusicSelect", outer.Name(), "Surface");
    return outer;
}

Visual2DNode &MusicSelectView::AddLabel(
    Visual2DNode &parent, const Rect penpotBounds, std::wstring text,
    const float penpotFontSize, const Color color, std::string name,
    const TextAlignment alignment)
{
    auto &label = CreateLabel(
        parent, ScaleTopLeftBounds(penpotBounds, parent.NodeSize().height), std::move(text),
        std::move(name));
    auto &textComponent = RequireComponent<TextVisualComponent>(label);
    textComponent.SetFontSize(CanvasFontSize(penpotFontSize));
    texts_.ApplyFont(textComponent);
    textComponent.SetTextColor(color);
    textComponent.SetHorizontalAlignment(alignment);
    return label;
}

void MusicSelectView::ApplyTexts()
{
    const auto &text = MusicSelect(texts_);
    const auto setLabel = [this](Visual2DNode *node,
                                 const std::wstring_view value) {
        if (node == nullptr)
            return;
        auto &component = RequireComponent<TextVisualComponent>(*node);
        component.SetText(std::wstring(value));
        texts_.ApplyFont(component);
    };

    setLabel(categoryLabel_, text.all);
    setLabel(emptyRecordMessage_, text.noRecords);
    setLabel(previewTitle_, text.backgroundPreview);
    setLabel(previewEmpty_, text.noImage);
    setLabel(difficultyHeading_, text.difficulty);
    setLabel(creatorHeading_, text.creator);
    setLabel(browserHint_, text.browserHint);
    setLabel(optionLabel_, text.optionSelect);
    setLabel(backLabel_, text.back);
    setLabel(goLabel_, text.go);
    setLabel(newDifficultyItem_, text.newDifficulty);
    setLabel(convertOsuItem_, text.convertOsu);

    if (recordSelector_ != nullptr)
    {
        auto &combo =
            RequireComponent<ComboBoxBehaviorComponent>(*recordSelector_);
        combo.SetItems({std::wstring(text.personalRecord)});
        texts_.ApplyFont(combo);
    }
    if (sortSelector_ != nullptr)
    {
        auto &combo = RequireComponent<ComboBoxBehaviorComponent>(*sortSelector_);
        const std::size_t selected = combo.SelectedIndex();
        combo.SetItems({std::wstring(text.sortModes[0]), std::wstring(text.sortModes[1]),
                        std::wstring(text.sortModes[2])});
        combo.SetSelectedIndex(selected);
        texts_.ApplyFont(combo);
    }
    if (searchField_ != nullptr)
    {
        texts_.ApplyFont(
            RequireComponent<TextVisualComponent>(*searchField_));
    }
    options_.RefreshTexts();
    RefreshSelectionPresentation();
    RefreshSearchPresentation();
    textRevision_ = texts_.Revision();
}
