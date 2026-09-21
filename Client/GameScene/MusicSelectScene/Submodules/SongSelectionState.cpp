#include "SongSelectionState.h"
#include "SongSelectionText.h"
#include <algorithm>
#include <ranges>

using namespace song_select;

void SongSelectionState::RebuildVisibleSongs()
{
    std::optional<std::size_t> previousCatalogIndex;
    if (focusedSongPosition_ < visibleSongIndices_.size())
    {
        previousCatalogIndex = visibleSongIndices_[focusedSongPosition_];
    }

    visibleSongIndices_.clear();
    for (std::size_t index = 0; index < catalog_.songs.size(); ++index)
    {
        if (MatchesSearch(catalog_.songs[index], searchText_))
        {
            visibleSongIndices_.push_back(index);
        }
    }

    const auto compareText = [this](const std::size_t leftIndex, const std::size_t rightIndex) {
        const auto &left = catalog_.songs[leftIndex];
        const auto &right = catalog_.songs[rightIndex];
        if (sortMode_ == SortMode::Artist)
        {
            return Lowercase(SongArtist(left)) < Lowercase(SongArtist(right));
        }
        return Lowercase(SongTitle(left)) < Lowercase(SongTitle(right));
    };
    // YMM/YMP currently has no chart LEVEL field. "Difficulty" therefore
    // preserves catalog order instead of inventing a score from note count or
    // judgement timing strictness.
    if (sortMode_ != SortMode::Difficulty)
    {
        std::stable_sort(visibleSongIndices_.begin(), visibleSongIndices_.end(), compareText);
    }

    focusedSongPosition_ = 0;
    if (previousCatalogIndex.has_value())
    {
        const auto found = std::ranges::find(visibleSongIndices_, *previousCatalogIndex);
        if (found != visibleSongIndices_.end())
        {
            focusedSongPosition_ =
                static_cast<std::size_t>(std::distance(visibleSongIndices_.begin(), found));
        }
    }
    selectedPatternIndex_ = 0;
}

void SongSelectionState::MoveSongFocus(const int delta)
{
    if (visibleSongIndices_.empty())
    {
        return;
    }
    const std::ptrdiff_t target =
        std::clamp<std::ptrdiff_t>(static_cast<std::ptrdiff_t>(focusedSongPosition_) + delta, 0,
                                   static_cast<std::ptrdiff_t>(visibleSongIndices_.size() - 1));
    SelectVisibleSong(static_cast<std::size_t>(target));
}

void SongSelectionState::MoveDifficultyFocus(const int delta)
{
    if (visibleSongIndices_.empty())
    {
        return;
    }
    const auto &patterns = catalog_.songs[visibleSongIndices_[focusedSongPosition_]].patterns;
    if (delta < 0)
    {
        if (!patterns.empty() && selectedPatternIndex_ > 0)
        {
            SelectPattern(selectedPatternIndex_ - 1);
            return;
        }
        if (focusedSongPosition_ > 0)
        {
            SelectVisibleSong(focusedSongPosition_ - 1);
        }
        return;
    }
    if (!patterns.empty() && selectedPatternIndex_ + 1 < patterns.size())
    {
        SelectPattern(selectedPatternIndex_ + 1);
        return;
    }
    if (focusedSongPosition_ + 1 < visibleSongIndices_.size())
    {
        SelectVisibleSong(focusedSongPosition_ + 1);
    }
}

void SongSelectionState::SelectVisibleSong(const std::size_t visiblePosition)
{
    if (visiblePosition >= visibleSongIndices_.size())
    {
        return;
    }
    focusedSongPosition_ = visiblePosition;
    selectedPatternIndex_ = 0;
    ClearLaunchError();
}

void SongSelectionState::SelectPattern(std::size_t index)
{
    if (focusedSongPosition_ >= visibleSongIndices_.size())
        return;
    const auto &patterns = catalog_.songs[visibleSongIndices_[focusedSongPosition_]].patterns;
    if (index >= patterns.size())
        return;
    selectedPatternIndex_ = index;
    ClearLaunchError();
}

void SongSelectionState::SetLaunchError(const LaunchErrorKind kind, std::string detail)
{
    launchErrorKind_ = kind;
    launchErrorDetail_ = std::move(detail);
}

void SongSelectionState::ClearLaunchError() noexcept
{
    launchErrorKind_ = LaunchErrorKind::None;
    launchErrorDetail_.clear();
}
