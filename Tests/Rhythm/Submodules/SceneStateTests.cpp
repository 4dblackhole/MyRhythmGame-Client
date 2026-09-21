#include "TestSupport.h"
#include "GameFlow/GameplayLaunchStore.h"
#include "GameScene/MusicSelectScene/Submodules/SongSelectionState.h"
#include "EditorScene/Submodules/EditorTool.h"

namespace finger_drum::tests
{
    void TestSceneStateBoundaries()
    {
        SongSelectionState state;
        state.RebuildVisibleSongs();
        state.MoveDifficultyFocus(1);
        state.MoveSongFocus(-1);
        Require(state.visibleSongIndices_.empty(),
                "Empty catalogs must remain navigable without a selection.");

        chart::SongCatalogEntry first, second;
        first.music.names = {"Zulu"};
        first.music.artists = {"Artist A"};
        first.patterns.resize(2);
        second.music.names = {"Alpha"};
        second.music.artists = {"Artist B"};
        second.patterns.resize(1);
        state.catalog_.songs = {first, second};
        state.RebuildVisibleSongs();
        state.MoveDifficultyFocus(1);
        Require(state.focusedSongPosition_ == 0 && state.selectedPatternIndex_ == 1,
                "Down first moves inside the expanded song.");
        state.MoveDifficultyFocus(1);
        Require(state.focusedSongPosition_ == 1 && state.selectedPatternIndex_ == 0,
                "Down at the final difficulty selects the next song's first difficulty.");
        state.MoveDifficultyFocus(-1);
        Require(state.focusedSongPosition_ == 0 && state.selectedPatternIndex_ == 0,
                "Up at the first difficulty preserves the existing previous-song policy.");
        state.sortMode_ = SongSelectionState::SortMode::Title;
        state.RebuildVisibleSongs();
        Require(state.visibleSongIndices_ == std::vector<std::size_t>{1, 0} &&
                    state.focusedSongPosition_ == 1,
                "Sorting changes display order but keeps focus on the same catalog entry.");
        state.searchText_ = L"artist b";
        state.RebuildVisibleSongs();
        Require(state.visibleSongIndices_ == std::vector<std::size_t>{1} &&
                    state.selectedPatternIndex_ == 0,
                "Search is case-insensitive and resets difficulty safely.");
        state.MoveSongFocus(20);
        Require(state.focusedSongPosition_ == 0, "Song focus clamps to the filtered list.");

        GameplayLaunchStore store;
        store.Set({"first.ymp", {}, "first.mp3", "Taiko"});
        auto playing = store.Snapshot();
        store.Set({"second.ymp", "second.yme", "second.mp3", "Taiko"});
        playing.musicPath = "local.mp3";
        Require(playing.patternPath == "first.ymp" && store.Snapshot().musicPath == "second.mp3",
                "An active session and a later selection must own independent launch values.");

        for (const auto &tool : editor_tools::Tools)
            Require(mode::FindTaikoNote(static_cast<int>(tool.note)) != nullptr,
                    "Every editor note tool must refer to a known persisted note definition.");
        const auto *katBuzz = editor_tools::Find(18);
        Require(
            katBuzz && katBuzz->note == mode::TaikoNoteType::Buzz &&
                editor_tools::ExtraData(*katBuzz) ==
                    std::vector<std::string>{"Action=Kat", "TickDivision=16"},
            "Kat Buzz's editor ID must serialize as Buzz plus its action, not as a new YMP ID.");
        Require(mode::TaikoVisual(mode::TaikoNoteType::BigTickRoll) ==
                        mode::NoteVisualKind::BigRoll &&
                    mode::TaikoVisual(mode::TaikoNoteType::Buzz, mode::TaikoAction::Kat) ==
                        mode::NoteVisualKind::KatBuzz,
                "Typed presentation must preserve the large roll and Kat Buzz appearances.");
    }
} // namespace finger_drum::tests
