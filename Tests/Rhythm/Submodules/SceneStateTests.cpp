#include "EditorScene/Submodules/Modes/Taiko/TaikoEditorTool.h"
#include "GameFlow/GameplayLaunchStore.h"
#include "GameScene/MusicSelectScene/Submodules/SongSelectionState.h"
#include "TestSupport.h"

namespace tests
{
    void TestSceneStateBoundaries()
    {
        SongSelectionState state;
        state.RebuildVisibleSongs();
        state.MoveDifficultyFocus(1);
        state.MoveSongFocus(-1);
        Require(state.visibleSongIndices_.empty(), "Empty catalogs must remain navigable without a selection.");

        SongCatalogEntry first, second;
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
        Require(state.visibleSongIndices_ == std::vector<std::size_t>{1, 0} && state.focusedSongPosition_ == 1,
                "Sorting changes display order but keeps focus on the same catalog entry.");
        state.searchText_ = L"artist b";
        state.RebuildVisibleSongs();
        Require(state.visibleSongIndices_ == std::vector<std::size_t>{1} && state.selectedPatternIndex_ == 0,
                "Search is case-insensitive and resets difficulty safely.");
        state.MoveSongFocus(20);
        Require(state.focusedSongPosition_ == 0, "Song focus clamps to the filtered list.");

        GameplayLaunchStore store;
        auto prepared = std::make_unique<PlaySession>();
        auto *identity = prepared.get();
        store.SetValidated({"validated.ymp", {}, "music.mp3", "Taiko"}, std::move(prepared));
        auto transferred = store.TakeValidatedSession();
        Require(transferred.get() == identity && !store.TakeValidatedSession(),
                "A validated play session must transfer ownership once without reparsing.");
        GameplayLaunchRequest firstLaunch{"first.ymp", {}, "first.mp3", "Taiko", L"Played title", L"Played creator"};
        firstLaunch.songArtist = L"Played artist";
        store.Set(firstLaunch);
        auto playing = store.Snapshot();
        store.Set({"second.ymp", "second.yme", "second.mp3", "Taiko"});
        playing.musicPath = "local.mp3";
        Require(playing.patternPath == "first.ymp" && store.Snapshot().musicPath == "second.mp3",
                "An active session and a later selection must own independent launch values.");
        Require(playing.songTitle == L"Played title" && playing.patternCreator == L"Played creator" &&
                    playing.songArtist == L"Played artist" && store.Snapshot().songTitle.empty() &&
                    store.Snapshot().songArtist.empty(),
                "Result display metadata must belong to the played selection snapshot.");
        playing.Clear();
        Require(!playing.IsValid() && playing.songTitle.empty() && playing.patternCreator.empty() &&
                    playing.songArtist.empty(),
                "Clearing a launch must also clear result display metadata.");

        for (const auto &tool : Tools)
            Require(FindTaikoNote(static_cast<int>(tool.note)) != nullptr,
                    "Every editor note tool must refer to a known persisted note definition.");
        const auto *katBuzz = Find(18);
        Require(katBuzz && katBuzz->note == TaikoNoteType::Buzz &&
                    ExtraData(*katBuzz) == std::vector<std::string>{"Action=Kat", "TickDivision=16"},
                "Kat Buzz's editor ID must serialize as Buzz plus its action, not as a new YMP ID.");
        Require(TaikoVisual(TaikoNoteType::BigTickRoll) == NoteVisualKind::BigRoll &&
                    TaikoVisual(TaikoNoteType::Buzz, TaikoAction::Kat) ==
                        NoteVisualKind::KatBuzz,
                "Typed presentation must preserve the large roll and Kat Buzz appearances.");
    }
} // namespace youmu::tests
