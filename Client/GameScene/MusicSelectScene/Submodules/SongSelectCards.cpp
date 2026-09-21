#include "MusicSelectView.h"
#include "SongSelectLayout.h"
#include "SongSelectionText.h"

using namespace song_select;

void MusicSelectView::RebuildSongCards()
{
    if (songContent_ == nullptr || canvas_ == nullptr)
    {
        return;
    }
    inputRouter_.Reset(*canvas_);
    std::vector<mrg::visual2d::NodeId> childIds;
    childIds.reserve(songContent_->Children().size());
    for (const auto &child : songContent_->Children())
    {
        childIds.push_back(child->Id());
    }
    for (const mrg::visual2d::NodeId id : childIds)
    {
        static_cast<void>(songContent_->RemoveChild(id));
    }
    songCards_.clear();
    difficultyButtonIds_.clear();

    if (selection_.visibleSongIndices_.empty())
    {
        AddLabel(*songContent_,
                 {layout::EmptySongMessage.x, layout::EmptySongMessage.y,
                  std::max(songContentWidth_ - 32.0F, 1.0F), layout::EmptySongMessage.height},
                 L"NO SONGS AVAILABLE", 20.0F, DeepBlue, "NoSongs",
                 mrg::visual2d::TextAlignment::Center);
        contentHeight_ = layout::ContentBottom;
        ApplyScrollOffset();
        return;
    }

    float top = layout::ContentTop;
    for (std::size_t position = 0; position < selection_.visibleSongIndices_.size(); ++position)
    {
        const std::size_t catalogIndex = selection_.visibleSongIndices_[position];
        const bool focused = position == selection_.focusedSongPosition_;
        const std::size_t difficultyCount = selection_.catalog_.songs[catalogIndex].patterns.size();
        const float height =
            focused ? layout::ExpandedBaseHeight +
                          layout::DifficultyRowStep * static_cast<float>(difficultyCount)
                    : layout::NormalSongHeight;
        CreateSongCard(position, catalogIndex, top, height, focused);
        top += height + layout::SongGap;
    }
    contentHeight_ = top - layout::SongGap + layout::ContentTop;
    ApplyScrollOffset();
    inputRouter_.InvalidateHitTest();
}

void MusicSelectView::CreateSongCard(const std::size_t visiblePosition,
                                     const std::size_t catalogIndex, const float top,
                                     const float height, const bool focused)
{
    const auto &song = selection_.catalog_.songs[catalogIndex];
    auto &button = mrg::visual2d::CreateButton(
        *songContent_,
        ScaleTopLeftBounds({layout::ContentLeft, top, songContentWidth_, height},
                           songContent_->NodeSize().height),
        L"", std::format("Song.{}", visiblePosition));
    ApplyButtonStyle(button, focused ? BorderBlue : PaleBorder, BorderBlue, DeepBlue);
    SetCornerRadius(button, 14.0F);
    AddPanel(button, {2.0F, 2.0F, std::max(songContentWidth_ - 4.0F, 1.0F), height - 4.0F},
             focused ? FocusBlue : RowBlue, "Surface", 12.0F);

    const float titleWidth = std::max(songContentWidth_ - 28.0F, 1.0F);
    auto &title =
        AddLabel(button, {14.0F, focused ? 16.0F : 10.0F, titleWidth, focused ? 32.0F : 30.0F},
                 SongTitle(song), focused ? 20.0F : 16.0F, DeepBlue, "Title");
    title.AddComponent<finger_drum::presentation::MarqueeTextComponent>(SongTitle(song),
                                                                        focused ? 31 : 37, 0.24);
    auto &artist = AddLabel(button, {14.0F, focused ? 50.0F : 40.0F, titleWidth, 20.0F},
                            SongArtist(song), focused ? 12.0F : 11.0F, MutedBlue, "Artist");

    if (focused)
    {
        AddPanel(button, {14.0F, 80.0F, titleWidth, 1.0F}, PaleBorder, "Divider");
        if (song.patterns.empty())
        {
            AddLabel(button, {14.0F, 92.0F, titleWidth, 24.0F}, L"NO DIFFICULTIES", 12.0F,
                     MutedBlue, "NoDifficulties", mrg::visual2d::TextAlignment::Center);
        }
        for (std::size_t index = 0; index < song.patterns.size(); ++index)
        {
            auto &difficulty = mrg::visual2d::CreateButton(
                button,
                ScaleTopLeftBounds({14.0F,
                                    92.0F + layout::DifficultyRowStep * static_cast<float>(index),
                                    titleWidth, layout::DifficultyRowHeight},
                                   button.NodeSize().height),
                PatternName(song.patterns[index]), std::format("Difficulty.{}", index));
            const bool selected = index == selection_.selectedPatternIndex_;
            ApplyButtonStyle(difficulty, selected ? AccentBlue : PureWhite,
                             selected ? AccentHover : PaleBlue, selected ? PureWhite : DeepBlue);
            SetCornerRadius(difficulty, 8.0F);
            auto &text = RequireComponent<mrg::visual2d::TextVisualComponent>(difficulty);
            text.SetFontSize(CanvasFontSize(12.0F));
            text.SetHorizontalAlignment(mrg::visual2d::TextAlignment::Leading);
            text.SetContentBounds(
                ScaleTopLeftBounds({12.0F, 0.0F, titleWidth - 24.0F, layout::DifficultyRowHeight},
                                   difficulty.NodeSize().height));
            difficultyButtonIds_.push_back({difficulty.Id(), index});
        }
    }
    songCards_.push_back({catalogIndex, top, height, &button, &title, &artist});
}

void MusicSelectView::RefreshSelectionPresentation()
{
    if (selectedSongLabel_ == nullptr || selectedArtistLabel_ == nullptr ||
        selectedPatternLabel_ == nullptr || selectedCreatorLabel_ == nullptr ||
        selectedDetailsLabel_ == nullptr)
    {
        return;
    }

    auto &songMarquee =
        RequireComponent<finger_drum::presentation::MarqueeTextComponent>(*selectedSongLabel_);
    auto &artistMarquee =
        RequireComponent<finger_drum::presentation::MarqueeTextComponent>(*selectedArtistLabel_);
    if (selection_.visibleSongIndices_.empty())
    {
        songMarquee.SetText(L"NO SONG SELECTED");
        artistMarquee.SetText(L"");
        RequireComponent<mrg::visual2d::TextVisualComponent>(*selectedPatternLabel_).SetText(L"—");
        RequireComponent<mrg::visual2d::TextVisualComponent>(*selectedCreatorLabel_).SetText(L"—");
        RequireComponent<mrg::visual2d::TextVisualComponent>(*selectedDetailsLabel_)
            .SetText(selection_.launchError_.empty() ? L"LEVEL —   BPM —   NOTES —   MODE —"
                                                     : selection_.launchError_);
        return;
    }

    selection_.focusedSongPosition_ =
        std::min(selection_.focusedSongPosition_, selection_.visibleSongIndices_.size() - 1);
    const auto &song =
        selection_.catalog_.songs[selection_.visibleSongIndices_[selection_.focusedSongPosition_]];
    songMarquee.SetText(SongTitle(song));
    artistMarquee.SetText(SongArtist(song));
    if (song.patterns.empty())
    {
        RequireComponent<mrg::visual2d::TextVisualComponent>(*selectedPatternLabel_)
            .SetText(L"NO DIFFICULTY");
        RequireComponent<mrg::visual2d::TextVisualComponent>(*selectedCreatorLabel_).SetText(L"—");
        RequireComponent<mrg::visual2d::TextVisualComponent>(*selectedDetailsLabel_)
            .SetText(L"LEVEL —   BPM —   NOTES —   MODE —");
        return;
    }

    selection_.selectedPatternIndex_ =
        std::min(selection_.selectedPatternIndex_, song.patterns.size() - 1);
    const auto &selected = song.patterns[selection_.selectedPatternIndex_];
    RequireComponent<mrg::visual2d::TextVisualComponent>(*selectedPatternLabel_)
        .SetText(PatternName(selected));
    RequireComponent<mrg::visual2d::TextVisualComponent>(*selectedCreatorLabel_)
        .SetText(selected.pattern.makers.empty()
                     ? L"—"
                     : DecodeDisplayText(selected.pattern.makers.front()));
    std::wstring mode =
        selected.pattern.mode.empty() ? L"—" : DecodeDisplayText(selected.pattern.mode);
    std::ranges::transform(mode, mode.begin(), [](const wchar_t character) {
        return static_cast<wchar_t>(std::towupper(character));
    });
    if (!selection_.launchError_.empty())
    {
        RequireComponent<mrg::visual2d::TextVisualComponent>(*selectedDetailsLabel_)
            .SetText(selection_.launchError_);
        return;
    }
    RequireComponent<mrg::visual2d::TextVisualComponent>(*selectedDetailsLabel_)
        .SetText(std::format(L"LEVEL —   BPM {}   NOTES {}   MODE {}",
                             BpmText(selected.pattern.baseBpm), selected.pattern.notes.size(),
                             mode));
}

void MusicSelectView::RefreshSearchPresentation()
{
    if (searchField_ != nullptr)
    {
        std::wstring display =
            selection_.searchText_.empty() ? L"SEARCH SONGS" : selection_.searchText_;
        if (searchFocused_)
        {
            display += L"_";
        }
        RequireComponent<mrg::visual2d::TextVisualComponent>(*searchField_)
            .SetText(std::move(display));
    }
    if (searchCountLabel_ != nullptr)
    {
        RequireComponent<mrg::visual2d::TextVisualComponent>(*searchCountLabel_)
            .SetText(std::format(L"{} SONGS", selection_.visibleSongIndices_.size()));
    }
}

void MusicSelectView::EnsureFocusedCardVisible()
{
    if (selection_.focusedSongPosition_ >= songCards_.size())
    {
        scrollOffset_ = 0.0F;
        ApplyScrollOffset();
        return;
    }
    const SongCard &card = songCards_[selection_.focusedSongPosition_];
    const float visibleTop = layout::ContentTop + scrollOffset_;
    const float visibleBottom = layout::ContentBottom + scrollOffset_;
    if (card.top < visibleTop)
    {
        scrollOffset_ = card.top - layout::ContentTop;
    }
    else if (card.top + card.height > visibleBottom)
    {
        scrollOffset_ = card.top + card.height - layout::ContentBottom;
    }
    ApplyScrollOffset();
}

void MusicSelectView::ApplyScrollOffset()
{
    const float maximum = std::max(contentHeight_ - layout::ContentBottom, 0.0F);
    scrollOffset_ = std::clamp(scrollOffset_, 0.0F, maximum);
    if (songContent_ != nullptr)
    {
        songContent_->SetPosition({0.0F, scrollOffset_ * DesignToCanvasScale});
    }
    UpdateScrollbar();
    inputRouter_.InvalidateHitTest();
}

void MusicSelectView::UpdateScrollbar()
{
    if (scrollbarHandle_ == nullptr || songViewport_ == nullptr)
    {
        return;
    }
    const float maximum = std::max(contentHeight_ - layout::ContentBottom, 0.0F);
    const float handleHeight =
        maximum <= 0.0F
            ? layout::ScrollbarTrack.height
            : std::max(48.0F, layout::ScrollbarTrack.height * layout::ViewportVisibleHeight /
                                  std::max(contentHeight_ - layout::ContentTop,
                                           layout::ViewportVisibleHeight));
    const float travel = layout::ScrollbarTrack.height - handleHeight;
    const float handleTop =
        layout::ScrollbarTrack.y + (maximum <= 0.0F ? 0.0F : travel * scrollOffset_ / maximum);
    const float trackX =
        std::max(songViewport_->NodeSize().width / DesignToCanvasScale - 9.0F, 0.0F);
    scrollbarHandle_->SetBounds(
        ScaleTopLeftBounds({trackX, handleTop, layout::ScrollbarTrack.width, handleHeight},
                           songViewport_->NodeSize().height));
}
