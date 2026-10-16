#include "Editing/ChartEditor.h"
#include "GameScene/MusicSelectScene/Submodules/EditorPatternCreation.h"
#include "GameScene/MusicSelectScene/Submodules/NewDifficultyDialog.h"
#include "Parsing/ChartParser.h"
#include <Windows.h>
#include <fstream>
#include <iostream>

namespace
{
    namespace fs = std::filesystem;
    namespace chart = finger_drum::chart;
    void Check(bool value, const char *message)
    {
        if (!value)
            throw std::runtime_error(message);
    }
    void Write(const fs::path &path, std::string_view text)
    {
        fs::create_directories(path.parent_path());
        std::ofstream stream(path, std::ios::binary);
        stream << text;
        Check(static_cast<bool>(stream), "Fixture write failed.");
    }
    std::string Read(const fs::path &path)
    {
        std::ifstream stream(path, std::ios::binary);
        return {std::istreambuf_iterator<char>{stream}, {}};
    }
    struct DialogProbe
    {
        int phase{};
        bool cancel{}, passed{};
        ULONGLONG started{GetTickCount64()};
    };
    DialogProbe *probe{};
    void CALLBACK DriveDialog(HWND, UINT, UINT_PTR timer, DWORD)
    {
        const auto dialog = GetActiveWindow();
        if (!dialog || !GetDlgItem(dialog, 100))
            return;
        if (GetTickCount64() - probe->started > 5000)
        {
            KillTimer(nullptr, timer);
            SendMessageW(dialog, WM_COMMAND, IDCANCEL, 0);
            return;
        }
        if (probe->cancel)
        {
            probe->passed = GetWindowTextLengthW(GetDlgItem(dialog, 100)) == 0;
            KillTimer(nullptr, timer);
            SendMessageW(dialog, WM_COMMAND, IDCANCEL, 0);
            return;
        }
        const auto phase = probe->phase++;
        if (phase == 0)
        {
            SetDlgItemTextW(dialog, 100, L"새 난이도");
            SetDlgItemTextW(dialog, 101, L"Tester\r\n테스터");
            SetDlgItemTextW(dialog, 102, L"tag1\r\ntag2");
            SetDlgItemTextW(dialog, 103, L"invalid bpm");
            SendMessageW(dialog, WM_COMMAND, IDOK, 0);
            return;
        }
        if (phase < 3)
        {
            SetDlgItemTextW(dialog, 103, L"120");
            SetDlgItemTextW(dialog, 105, phase == 1 ? L"0" : L"101");
            SendMessageW(dialog, WM_COMMAND, IDOK, 0);
            return;
        }
        probe->passed = IsWindow(dialog) && GetWindowTextLengthW(GetDlgItem(dialog, 100)) == 5;
        SetDlgItemTextW(dialog, 105, L"80.25");
        SetDlgItemTextW(dialog, 103, L"120");
        KillTimer(nullptr, timer);
        SendMessageW(dialog, WM_COMMAND, IDOK, 0);
    }
} // namespace
int main()
{
    try
    {
        const auto root = fs::absolute(fs::path("build/new-difficulty-tests") / std::to_string(GetCurrentProcessId()));
        Check(!fs::exists(root), "Use a fresh fixture root.");
        const auto music = root / L"Music/음악.ymm";
        Write(music, "Version: 1\nFile: sample.wav\nMusic Name 1: Test\nArtist 1: Test\n");
        // Catalog discovery needs a file; this test does not decode audio.
        const auto sample = root / L"Music/sample.wav";
        Write(sample, "fixture");
        auto parsedMusic = chart::ChartParser{}.ParseMusicFile(music);
        Check(parsedMusic.Succeeded(), "YMM fixture must parse.");
        chart::SongCatalogEntry song{music, sample, parsedMusic.document, {}};
        chart::PatternDocument draft;
        draft.name = "Beginner";
        draft.makers = {"Tester"};
        draft.tags = {"new", "chart"};
        const auto created = editor_pattern_creation::Create(root, song, draft);
        Check(created.patternPath.parent_path() == root / L"Patterns/음악/Beginner", "Difficulty directory layout.");
        Check(created.pattern.musicMetadataFile == fs::path(L"Music/음악.ymm") &&
                  created.pattern.effectFile == L"음악 [Beginner].yme",
              "YMM root and sibling YME paths.");
        const auto parsed = chart::ChartParser{}.ParsePatternFile(created.patternPath);
        const auto effects = chart::ChartParser{}.ParseEffectFile(*created.effectPath);
        Check(parsed.Succeeded() && effects.Succeeded(), "New pair must parse.");
        chart::ChartEditor editor(parsed.document, effects.document);
        Check(editor.Notes().empty() && parsed.document.timing.empty() && effects.document.commands.empty() &&
                  parsed.document.baseBpm == 120 && parsed.document.patternOffsetMilliseconds == 0 &&
                  parsed.document.mode == "Taiko" && editor.Timeline().MeasureLength(0) == chart::Rational{1},
              "New chart defaults and empty score.");
        const auto before = Read(created.patternPath);
        bool refused{};
        try
        {
            static_cast<void>(editor_pattern_creation::Create(root, song, draft));
        }
        catch (const editor_pattern_creation::Error &e)
        {
            refused = e.reason == editor_pattern_creation::Failure::ExistingFolder;
        }
        Check(refused && Read(created.patternPath) == before, "Never overwrite existing difficulty.");
        for (const auto &name : {"../escape", "", "x/y", "name.", " name", "x\nheader"})
        {
            draft.name = name;
            refused = false;
            try
            {
                static_cast<void>(editor_pattern_creation::Create(root, song, draft));
            }
            catch (const editor_pattern_creation::Error &e)
            {
                refused = e.reason == editor_pattern_creation::Failure::InvalidName;
            }
            Check(refused, "Unsafe difficulty names must not create files.");
        }
        draft.name = "Invalid";
        draft.baseBpm = -1;
        refused = false;
        try
        {
            static_cast<void>(editor_pattern_creation::Create(root, song, draft));
        }
        catch (const std::exception &)
        {
            refused = true;
        }
        Check(refused && !fs::exists(root / L"Patterns/음악/Invalid"), "Invalid metadata leaves no folder.");
        draft.baseBpm = 120;
        draft.name = "Another";
        song.patterns.push_back(created);
        auto another = editor_pattern_creation::Create(root, song, draft);
        Check(another.patternPath.parent_path().parent_path() == created.patternPath.parent_path().parent_path(),
              "Reuse the song's actual pattern parent.");
        finger_drum::texts::TextCatalog texts(root / "NoLanguages");
        DialogProbe cancelled;
        cancelled.cancel = true;
        probe = &cancelled;
        const auto cancelTimer = SetTimer(nullptr, 0, 25, DriveDialog);
        Check(cancelTimer != 0, "Dialog test timer unavailable.");
        Check(!CreateDifficultyWithDialog(root, song, texts) && cancelled.passed, "Cancel creates nothing.");
        DialogProbe submitted;
        probe = &submitted;
        Check(SetTimer(nullptr, 0, 25, DriveDialog) != 0, "Dialog test timer unavailable.");
        const auto fromDialog = CreateDifficultyWithDialog(root, song, texts);
        Check(fromDialog && submitted.passed && fromDialog->pattern.judgementLevel == 80.25 &&
                  fromDialog->pattern.makers.size() == 2 &&
                  fromDialog->pattern.tags == std::vector<std::string>{"tag1", "tag2"},
              "Unicode metadata persists after correcting invalid input in the same dialog.");
        const auto catalog = chart::SongCatalog{}.Load(root);
        Check(catalog.songs.size() == 1 && catalog.PatternCount() == 3, "New difficulties reload into the catalog.");
        Check(Read(music) == "Version: 1\nFile: sample.wav\nMusic Name 1: Test\nArtist 1: Test\n",
              "The shared YMM must remain untouched.");
        std::cout << "Difficulty creation and native metadata dialog tests passed.\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
