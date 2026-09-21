#include "MusicSelectView.h"
#include "SongSelectLayout.h"
#include "SongSelectionText.h"

using namespace song_select;

void MusicSelectView::ProcessPointer(const mrg::platform::InputState &input)
{
    const std::optional<mrg::visual2d::Point> canvasPointer =
        input.IsMouseInsideWindow()
            ? mrg::visual2d::MapScreenPointer(
                  {static_cast<float>(input.MousePositionX()),
                   static_cast<float>(input.MousePositionY())},
                  {static_cast<float>(width_), static_cast<float>(height_)}, *canvas_)
            : std::nullopt;
    mrg::visual2d::PointerInput pointer{};
    pointer.available = canvasPointer.has_value();
    pointer.position = canvasPointer.value_or(mrg::visual2d::Point{});
    pointer.leftButtonDown = input.IsMouseButtonDown(mrg::platform::MouseButton::Left);
    pointer.leftButtonPressed = input.WasMouseButtonPressed(mrg::platform::MouseButton::Left);
    pointer.leftButtonReleased = input.WasMouseButtonReleased(mrg::platform::MouseButton::Left);
    pointer.wheelDelta = input.MouseWheelDelta();
    pointer.timestampTicks = LatestPointerTimestamp(input);
    inputRouter_.Process(*canvas_, pointer);

    if (canvasPointer.has_value() && songViewport_ != nullptr &&
        songViewport_->BoundsInCanvas().Contains(*canvasPointer) &&
        std::abs(input.MouseWheelDelta()) > 0.0F)
    {
        scrollOffset_ -= input.MouseWheelDelta() * layout::ScrollStep;
        ApplyScrollOffset();
    }
}

bool MusicSelectView::ProcessActions()
{
    for (const mrg::visual2d::Action &action : canvas_->TakeActions())
    {
        if (action.type == mrg::visual2d::ActionType::SelectionChanged &&
            action.source == sortSelectorId_)
        {
            selection_.sortMode_ = static_cast<SortMode>(std::min<std::size_t>(
                action.selectedIndex, static_cast<std::size_t>(SortMode::Artist)));
            RebuildVisibleSongs();
            return false;
        }
        if (action.type != mrg::visual2d::ActionType::Clicked)
        {
            continue;
        }
        if (action.source == backButtonId_)
        {
            command_ = SongSelectCommand::Back;
            return true;
        }
        if (action.source == goButtonId_)
        {
            command_ = SongSelectCommand::Launch;
            return true;
        }
        if (action.source == searchFieldId_)
        {
            searchFocused_ = true;
            RefreshSearchPresentation();
            return false;
        }
        for (std::size_t position = 0; position < songCards_.size(); ++position)
        {
            if (songCards_[position].button != nullptr &&
                action.source == songCards_[position].button->Id())
            {
                searchFocused_ = false;
                SelectVisibleSong(position);
                RefreshSearchPresentation();
                return false;
            }
        }
        for (const auto [id, patternIndex] : difficultyButtonIds_)
        {
            if (action.source == id)
            {
                searchFocused_ = false;
                SelectPattern(patternIndex);
                RefreshSearchPresentation();
                return false;
            }
        }
    }
    return false;
}

bool MusicSelectView::ProcessKeyboard(const mrg::platform::InputState &input)
{
    if (input.WasKeyPressed(VK_ESCAPE))
    {
        if (searchFocused_)
        {
            searchFocused_ = false;
            RefreshSearchPresentation();
            return false;
        }
        command_ = SongSelectCommand::Back;
        return true;
    }
    if (input.IsKeyDown(VK_CONTROL) && input.WasKeyPressed(static_cast<std::uint16_t>('F')))
    {
        searchFocused_ = true;
        RefreshSearchPresentation();
        return false;
    }
    if (ProcessSearchKeyboard(input))
    {
        return false;
    }
    if (selection_.visibleSongIndices_.empty())
    {
        return false;
    }

    if (input.WasKeyPressed(VK_LEFT))
    {
        MoveSongFocus(-1);
    }
    else if (input.WasKeyPressed(VK_RIGHT))
    {
        MoveSongFocus(1);
    }
    else if (input.WasKeyPressed(VK_UP))
    {
        MoveDifficultyFocus(-1);
    }
    else if (input.WasKeyPressed(VK_DOWN))
    {
        MoveDifficultyFocus(1);
    }

    if (input.WasKeyPressed(VK_RETURN) || input.WasKeyPressed(VK_SPACE))
    {
        command_ = SongSelectCommand::Launch;
        return true;
    }
    return false;
}

bool MusicSelectView::ProcessSearchKeyboard(const mrg::platform::InputState &input)
{
    if (!searchFocused_)
    {
        return false;
    }
    if (input.WasKeyPressed(VK_RETURN))
    {
        searchFocused_ = false;
        RefreshSearchPresentation();
        return true;
    }

    bool changed = false;
    if (input.WasKeyPressed(VK_BACK) && !selection_.searchText_.empty())
    {
        selection_.searchText_.pop_back();
        changed = true;
    }
    if (selection_.searchText_.size() < 48)
    {
        for (std::uint16_t key = 'A'; key <= 'Z'; ++key)
        {
            if (input.WasKeyPressed(key))
            {
                selection_.searchText_.push_back(static_cast<wchar_t>(key));
                changed = true;
            }
        }
        for (std::uint16_t key = '0'; key <= '9'; ++key)
        {
            if (input.WasKeyPressed(key))
            {
                selection_.searchText_.push_back(static_cast<wchar_t>(key));
                changed = true;
            }
        }
        if (input.WasKeyPressed(VK_SPACE))
        {
            selection_.searchText_.push_back(L' ');
            changed = true;
        }
    }
    if (changed)
    {
        RebuildVisibleSongs();
        searchFocused_ = true;
        RefreshSearchPresentation();
    }
    return changed;
}

void MusicSelectView::MoveSongFocus(const int delta)
{
    selection_.MoveSongFocus(delta);
    RefreshSelection();
}

void MusicSelectView::MoveDifficultyFocus(const int delta)
{
    const auto previousSong = selection_.focusedSongPosition_;
    const auto previousPattern = selection_.selectedPatternIndex_;
    selection_.MoveDifficultyFocus(delta);
    if (previousSong != selection_.focusedSongPosition_)
        RefreshSelection();
    else if (previousPattern != selection_.selectedPatternIndex_)
        SelectPattern(selection_.selectedPatternIndex_);
}

void MusicSelectView::SelectVisibleSong(const std::size_t visiblePosition)
{
    selection_.SelectVisibleSong(visiblePosition);
    RefreshSelection();
}

void MusicSelectView::SelectPattern(const std::size_t index)
{
    selection_.SelectPattern(index);
    for (const auto [id, patternIndex] : difficultyButtonIds_)
    {
        if (auto *button = canvas_->FindNode(id))
        {
            const bool selected = patternIndex == selection_.selectedPatternIndex_;
            ApplyButtonStyle(*button, selected ? AccentBlue : PureWhite,
                             selected ? AccentHover : PaleBlue, selected ? PureWhite : DeepBlue);
        }
    }
    RefreshSelectionPresentation();
}
