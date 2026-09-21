#include "TestSupport.h"

namespace finger_drum::tests
{
    void TestSongCatalog(const std::filesystem::path &songsRoot)
    {
        const chart::SongCatalogLoadResult catalog = chart::SongCatalog{}.Load(songsRoot);
        if (!catalog.Succeeded())
        {
            for (const chart::Diagnostic &diagnostic : catalog.diagnostics)
            {
                if (diagnostic.severity == chart::DiagnosticSeverity::Error)
                {
                    std::cerr << diagnostic.location.file.string() << ':'
                              << diagnostic.location.line << ": " << diagnostic.message << '\n';
                }
            }
        }
        Require(catalog.Succeeded(), "The copied RPG song catalog must parse without errors.");
        Require(catalog.discoveredMusicFiles >= 1 &&
                    catalog.songs.size() == catalog.discoveredMusicFiles,
                "Every discovered YMM music entry must appear in SONG LIST.");
        Require(catalog.discoveredPatternFiles >= 3 &&
                    catalog.PatternCount() == catalog.discoveredPatternFiles,
                "All bundled and optional local YMP patterns must be associated with their songs.");
        mode::TaikoMode taiko;
        bool checkedAngelDream = false;
        for (const chart::SongCatalogEntry &song : catalog.songs)
        {
            Require(std::filesystem::is_regular_file(song.audioPath),
                    "Every YMM entry must resolve its music file.");
            if (song.metadataPath.filename() == "angel dream hand shaking.ymm")
            {
                constexpr std::array RequiredPatterns{"angeldream [measure test].ymp",
                                                      "angeldream [test].ymp", "angeldream.ymp"};
                for (const std::string_view required : RequiredPatterns)
                {
                    Require(
                        std::ranges::any_of(song.patterns,
                                            [required](const chart::SongCatalogPattern &pattern) {
                                                return pattern.patternPath.filename() == required;
                                            }),
                        "The bundled AngelDream catalog must include every test pattern.");
                }
                checkedAngelDream = true;
            }
            for (const chart::SongCatalogPattern &pattern : song.patterns)
            {
                TemporaryDirectory saved("finger-drum-catalog-save");
                auto copy = pattern.pattern;
                copy.sourcePath = saved.Path() / pattern.patternPath.filename();
                chart::EffectDocument effect;
                if (pattern.effectPath)
                    effect = chart::ChartParser{}.ParseEffectFile(*pattern.effectPath).document;
                chart::ChartEditor editing(copy, effect);
                editing.Save();
                const auto reloaded = chart::ChartParser{}.ParsePatternFile(copy.sourcePath);
                Require(reloaded.Succeeded() &&
                            chart::ChartEditor::WritePattern(reloaded.document) ==
                                chart::ChartEditor::WritePattern(copy),
                        "Every provided chart must save/reload without source-data loss (using a "
                        "temporary copy).");
                if (pattern.patternPath.filename().string() == "Rapbit - Saika [test].ymp")
                {
                    const chart::MusicalTimeline timeline(pattern.pattern);
                    const rhythm::RhythmTime firstChange =
                        timeline.Compile({1, chart::Rational{1, 8}});
                    const rhythm::RhythmTime secondChange =
                        timeline.Compile({1, chart::Rational{2, 8}});
                    const rhythm::RhythmTime finalChange =
                        timeline.Compile({1, chart::Rational{18, 8}});
                    const rhythm::RhythmTime nextBarline =
                        timeline.Compile({2, chart::Rational{0, 1}});
                    Require(secondChange - firstChange == rhythm::RhythmDuration{166'667} &&
                                finalChange < nextBarline,
                            "Saika's 20/8 measure must keep every BPM anchor at its absolute N/D "
                            "position.");
                }
                mode::ModeLoadResult loaded =
                    taiko.LoadSession(pattern.patternPath, pattern.effectPath);
                Require(loaded.Succeeded() && loaded.session->Gear().LaneCount() == 1,
                        "Every copied Taiko pattern must create one playable Lane.");
            }
        }
        Require(checkedAngelDream, "The catalog test must exercise the bundled AngelDream song.");
        std::cout << "Catalog verified: " << catalog.songs.size() << " songs, "
                  << catalog.PatternCount() << " patterns.\n";
    }

    void TestSongCatalogIsolatesInvalidFiles()
    {
        TemporaryDirectory fixture("FingerDrumCatalogValidation");
        const std::filesystem::path music = fixture.Path() / "Music";
        const std::filesystem::path patterns = fixture.Path() / "Pattern";
        WriteTextFile(music / "valid.ymm", "Version: 1\nFile: valid.mp3\nMusic Name 1: Valid\n");
        WriteTextFile(music / "valid.mp3", "audio-placeholder");
        WriteTextFile(music / "invalid.ymm", "Version: nope\nMusic Name 1: Invalid\n");
        WriteTextFile(patterns / "valid.ymp", "Version: 1\nMusic metadata: Music/valid.ymm\n"
                                              "Pattern Name: Valid\nMode: Taiko\nBase BPM: 120\n"
                                              "[Pattern]\n0/1,1,0\n");
        WriteTextFile(patterns / "invalid.ymp",
                      "Version: nope\nMusic metadata: Music/valid.ymm\n"
                      "Pattern Name: Invalid\nMode: Taiko\nBase BPM: 120\n"
                      "[Pattern]\n0/1,1,0\n");

        const chart::SongCatalogLoadResult catalog = chart::SongCatalog{}.Load(fixture.Path());
        Require(!catalog.Succeeded() && catalog.discoveredMusicFiles == 2 &&
                    catalog.discoveredPatternFiles == 2,
                "Catalog discovery must retain diagnostics and source counts for invalid files.");
        Require(catalog.songs.size() == 1 && catalog.PatternCount() == 1,
                "Invalid YMM/YMP files must be isolated from otherwise playable catalog entries "
                "(songs=" +
                    std::to_string(catalog.songs.size()) +
                    ", patterns=" + std::to_string(catalog.PatternCount()) + ").");
    }
} // namespace finger_drum::tests
