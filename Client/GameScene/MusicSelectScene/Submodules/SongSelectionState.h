#pragma once
#include "Catalog/SongCatalog.h"
#include <cstdint>
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

    enum class LaunchErrorKind : std::uint8_t
    {
        None,
        Catalog,
        UnsupportedMode,
        Pattern,
    };

    finger_drum::chart::SongCatalogLoadResult catalog_;
    std::vector<std::size_t> visibleSongIndices_;
    std::size_t focusedSongPosition_{};
    std::size_t selectedPatternIndex_{};
    SortMode sortMode_{SortMode::Difficulty};
    std::wstring searchText_;
    LaunchErrorKind launchErrorKind_{LaunchErrorKind::None};
    std::string launchErrorDetail_;

    void RebuildVisibleSongs();
    void MoveSongFocus(const int delta);
    void MoveDifficultyFocus(const int delta);
    void SelectVisibleSong(const std::size_t visiblePosition);
    void SelectPattern(std::size_t index);
    void SetLaunchError(LaunchErrorKind kind, std::string detail);
    void ClearLaunchError() noexcept;
};
