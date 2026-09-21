#pragma once
#include "Catalog/SongCatalog.h"
#include <string>
#include <vector>

class SongSelectionState final
{
  public:
    enum class SortMode : std::size_t
    {
        Difficulty,
        Title,
        Artist,
    };

    finger_drum::chart::SongCatalogLoadResult catalog_;
    std::vector<std::size_t> visibleSongIndices_;
    std::size_t focusedSongPosition_{};
    std::size_t selectedPatternIndex_{};
    SortMode sortMode_{SortMode::Difficulty};
    std::wstring searchText_;
    std::wstring launchError_;

    void RebuildVisibleSongs();
    void MoveSongFocus(const int delta);
    void MoveDifficultyFocus(const int delta);
    void SelectVisibleSong(const std::size_t visiblePosition);
    void SelectPattern(std::size_t index);
};
