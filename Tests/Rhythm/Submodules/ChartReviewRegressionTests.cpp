#include "TestSupport.h"
#include <Windows.h>
#include <limits>

namespace tests
{
    namespace
    {
        std::string ReadFile(const std::filesystem::path &path)
        {
            std::ifstream file(path, std::ios::binary);
            return {std::istreambuf_iterator<char>(file), {}};
        }

        class RenameLock final
        {
          public:
            explicit RenameLock(const std::filesystem::path &path)
                : handle_(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr))
            {
                Require(handle_ != INVALID_HANDLE_VALUE, "Could not lock save target for regression test.");
            }
            ~RenameLock()
            {
                CloseHandle(handle_);
            }
            RenameLock(const RenameLock &) = delete;
            RenameLock &operator=(const RenameLock &) = delete;

          private:
            HANDLE handle_;
        };
    } // namespace

    void TestChartSaveFailureRecovery()
    {
        TemporaryDirectory folder("chart-save-recovery");
        PatternDocument pattern;
        pattern.sourcePath = folder.Path() / L"한글 악보.ymp";
        ChartEditor editor(pattern);
        editor.AddNote({0, {}}, 1);
        editor.Save();
        const auto effectPath = editor.Effects().sourcePath;
        const auto originalPattern = ReadFile(pattern.sourcePath), originalEffect = ReadFile(effectPath);
        editor.AddNote({1, {}}, 2);
        const auto revision = editor.Revision();
        for (const auto &lockedPath : {pattern.sourcePath, effectPath})
        {
            bool rejected = false;
            {
                RenameLock lock(lockedPath);
                try
                {
                    editor.Save();
                }
                catch (const std::filesystem::filesystem_error &)
                {
                    rejected = true;
                }
            }
            Require(rejected && editor.Dirty() && editor.Revision() == revision,
                    "Failed installation must preserve dirty state and revision.");
            Require(ReadFile(pattern.sourcePath) == originalPattern && ReadFile(effectPath) == originalEffect,
                    "Failure backing up either target must restore both original files.");
            for (auto target : {pattern.sourcePath, effectPath})
            {
                target += L".editor.tmp";
                Require(!std::filesystem::exists(target), "Failed saves must remove their own staging files.");
            }
        }
        auto recovery = pattern.sourcePath;
        recovery += L".editor.tmp";
        WriteTextFile(recovery, "existing recovery data");
        bool refused = false;
        try
        {
            editor.Save();
        }
        catch (const std::runtime_error &)
        {
            refused = true;
        }
        Require(refused && ReadFile(recovery) == "existing recovery data",
                "A save must preserve recovery files created before the transaction.");
        std::filesystem::remove(recovery);
        editor.Save();
        Require(!editor.Dirty() && ChartParser{}.ParsePatternFile(pattern.sourcePath).document.notes.size() == 2,
                "A save must succeed after its transient failure is resolved.");
        pattern = editor.Pattern();
        pattern.effectFile = pattern.sourcePath.filename();
        editor.Replace(pattern, editor.Effects());
        refused = false;
        try
        {
            editor.Save();
        }
        catch (const std::invalid_argument &)
        {
            refused = true;
        }
        Require(refused && ChartParser{}.ParsePatternFile(pattern.sourcePath).Succeeded(),
                "YMP/YME aliasing must not overwrite the score.");
    }

    void TestChartSavePathCollisions()
    {
        const std::array<std::pair<const wchar_t *, const wchar_t *>, 9> names{{
            {L"score.ymp", L"score.ymp"},
            {L"score.ymp", L"score.ymp.editor.tmp"},
            {L"score.ymp", L"score.ymp.editor.bak"},
            {L"score.ymp.editor.tmp", L"score.ymp"},
            {L"score.ymp.editor.bak", L"score.ymp"},
            {L"score.ymp", L"SCORE.YMP"},
            {L"score.ymp", L"SCORE.YMP.EDITOR.TMP"},
            {L"score.ymp", L"SCORE.YMP.EDITOR.BAK"},
            {L"score.ymp", L"./score.ymp.editor.bak"}}};
        const auto check = [](const PatternDocument &pattern)
        {
            const auto original = ReadFile(pattern.sourcePath);
            ChartEditor editor(pattern);
            editor.AddNote({0, {}}, 1);
            const auto revision = editor.Revision();
            bool refused = false;
            try { editor.Save(); }
            catch (const std::invalid_argument &) { refused = true; }
            Require(refused && editor.Dirty() && editor.Revision() == revision &&
                        ReadFile(pattern.sourcePath) == original && editor.Pattern().notes.size() == 1,
                    "Save-path aliases must be rejected before changing files, dirty state or document.");
            editor.Undo();
            Require(editor.Pattern().notes.empty(), "A rejected save must preserve undo history.");
            editor.Redo();
            Require(editor.Pattern().notes.size() == 1, "A rejected save must preserve redo history.");
        };
        for (const auto &[source, effect] : names)
        {
            TemporaryDirectory folder("save-path-alias");
            PatternDocument pattern;
            pattern.sourcePath = folder.Path() / source;
            pattern.effectFile = effect;
            WriteTextFile(pattern.sourcePath, "preserved source bytes");
            check(pattern);
            Require(std::distance(std::filesystem::directory_iterator(folder.Path()),
                                  std::filesystem::directory_iterator{}) == 1,
                    "Refusing a nonexistent alias must not create a YME or recovery file.");
        }
        for (const auto suffix : {L"", L".editor.tmp", L".editor.bak"})
        {
            TemporaryDirectory folder("save-hardlink-alias");
            PatternDocument pattern;
            pattern.sourcePath = folder.Path() / L"score.ymp";
            pattern.effectFile = "effects.yme";
            WriteTextFile(pattern.sourcePath, "preserved hardlink bytes");
            auto alias = folder.Path() / pattern.effectFile;
            alias += suffix;
            std::filesystem::create_hard_link(pattern.sourcePath, alias);
            check(pattern);
            Require(ReadFile(alias) == "preserved hardlink bytes", "Existing recovery aliases must be preserved.");
        }
    }

    void TestChartParserBoundaries()
    {
        ChartParser parser;
        const auto music = parser.ParseMusic("\xEF\xBB\xBFVersion: 7\nFile: music.wav\nTypo: value\n");
        Require(music.Succeeded() && music.document.version == 7 && music.diagnostics.size() == 1 &&
                    music.diagnostics.front().severity == DiagnosticSeverity::Warning &&
                    music.diagnostics.front().location.line == 3,
                "BOM must preserve the first field and unknown fields must carry line-numbered warnings.");
        const auto pattern = parser.ParsePattern("\xEF\xBB\xBFVersion: 7\nBase BPM: 120\nTypo: value\n");
        Require(pattern.Succeeded() && pattern.document.version == 7 && pattern.diagnostics.size() == 1,
                "YMP must share BOM handling and unknown-field diagnostics.");
        Require(parser.ParseEffect("\xEF\xBB\xBFVersion: 7\n").document.version == 7, "YME must share BOM handling.");
        TemporaryDirectory folder("unicode-chart");
        const auto missing = folder.Path() / L"없는 채보.ymp";
        const auto missingPattern = parser.ParsePatternFile(missing);
        Require(!missingPattern.Succeeded() && missingPattern.diagnostics.front().location.file == missing &&
                    !parser.ParseMusicFile(missing).Succeeded() && !parser.ParseEffectFile(missing).Succeeded(),
                "Missing Unicode files must return diagnostics without throwing ANSI conversion errors.");
        WriteTextFile(folder.Path() / L"이름 없는 곡.ymm", "File: music.wav\n");
        WriteTextFile(folder.Path() / L"다른 곡.ymm", "File: music.wav\n");
        WriteTextFile(folder.Path() / "music.wav", "test-only file existence fixture");
        Require(SongCatalog{}.Load(folder.Path()).songs.size() == 2,
                "Catalog sorting must support Unicode filename fallbacks without a title.");

        MusicalPosition position;
        Require(TryParseMusicalPosition("99999,0/1", 0, position) &&
                    !TryParseMusicalPosition("100000,0/1", 0, position) &&
                    !TryParseMusicalPosition("9223372036854775807,0/1", 0, position),
                "Musical positions must enforce the 100,000-measure limit before allocation.");
        Require(parser.ParseEffect("[Zone]\n100000,0/1,#Kiai ON\n").Succeeded() &&
                    !parser.ParseEffect("[Zone]\n100001,0/1,#Kiai ON\n").Succeeded() &&
                    !parser.ParseEffect("[Speed]\n1,0/1,Area,100001,0/1,#ScrollSpeed Whole,From=1,To=2,Curve=Linear\n")
                         .Succeeded(),
                "YME point and area positions must enforce the one-based limit.");
        std::string text = "[Pattern]\n";
        for (std::int64_t i = 1; i < MaximumMeasureCount; ++i)
            text += "--\n";
        Require(parser.ParsePattern(text + "0/1,1,0\n").Succeeded() &&
                    !parser.ParsePattern(text + "--\n0/1,1,0\n").Succeeded(),
                "YMP implicit measures must accept the last valid measure and reject overflow.");
        PatternDocument huge;
        huge.notes.push_back({{std::numeric_limits<std::int64_t>::max(), {}}});
        bool rejected = false;
        try
        {
            MusicalTimeline timeline(huge);
        }
        catch (const std::invalid_argument &)
        {
            rejected = true;
        }
        Require(rejected, "Programmatic documents must reject giant measures before reserve/arithmetic.");
        huge.notes.clear();
        huge.timing.push_back({{std::numeric_limits<std::int64_t>::max(), {}}, TimingDirectiveType::Bpm, 120});
        rejected = false;
        try { MusicalTimeline timeline(huge); }
        catch (const std::invalid_argument &) { rejected = true; }
        Require(rejected, "Programmatic timing must enforce the same limit before reserve.");
        EffectDocument invalidEffect;
        EffectCommand command;
        command.type = EffectCommandType::NoteSpeed;
        command.position.measure = MaximumMeasureCount;
        command.beginValue = command.endValue = 1;
        invalidEffect.commands.push_back(command);
        PatternDocument effectPattern;
        effectPattern.mode = "Taiko";
        Require(!TaikoMode{}.CreateSession(effectPattern, invalidEffect).Succeeded(),
                "Programmatic effects must not author notes at the exclusive end sentinel.");
        invalidEffect.commands.clear();
        invalidEffect.hitSounds.emplace("1", "sound.wav");
        invalidEffect.hitSoundChanges.push_back({{MaximumMeasureCount, {}}, "1", 1});
        Require(!TaikoMode{}.CreateSession(effectPattern, invalidEffect).Succeeded(),
                "Programmatic hit-sound points must enforce the same limit.");
    }

    void TestChartPartialEdits()
    {
        ChartEditor editor(PatternDocument{});
        editor.AddNote({1, {}}, 1);
        editor.AddNote({2, {}}, 11, MusicalPosition{3, {}});
        const auto sourceRevision = editor.AudioSourceRevision();
        const auto noteId = editor.Notes().front().note.sourceOrder;
        editor.ReplaceTiming({{{0, {}}, TimingDirectiveType::Bpm, 240},
                              {{1, {}}, TimingDirectiveType::DelayMilliseconds, -100}});
        Require(editor.Notes().front().timing == RhythmTime{900'000} &&
                    editor.Notes().front().note.sourceOrder == noteId &&
                    editor.AudioSourceRevision() == sourceRevision &&
                    editor.NotesInTimeRange(RhythmTime{900'000}, RhythmTime{900'000}).size() == 1,
                "Partial timing edits must preserve note IDs/sources and rebuild exact time queries.");
        const auto revision = editor.Revision();
        bool rejected = false;
        try
        {
            editor.ReplaceTiming({{{0, {}}, TimingDirectiveType::Bpm, -1}});
        }
        catch (const std::invalid_argument &)
        {
            rejected = true;
        }
        Require(rejected && editor.Revision() == revision &&
                    editor.Notes().front().timing == RhythmTime{900'000},
                "Failed timing edits must preserve caches and revision.");
        EffectDocument effects;
        EffectCommand speed;
        speed.type = EffectCommandType::NoteSpeed;
        speed.beginValue = speed.endValue = .5;
        effects.commands.push_back(speed);
        editor.ReplaceEffects(effects);
        Require(editor.Notes().front().scrollMultiplier == .5 && editor.AudioSourceRevision() == sourceRevision &&
                    editor.NotesInMeasures(2, 3).size() == 2 &&
                    editor.Notes().front().timing == RhythmTime{900'000},
                "Partial effect edits must keep time indices/pairs and update note speed.");
        const auto effectRevision = editor.Revision();
        effects.commands.front().endValue = 0;
        rejected = false;
        try
        {
            editor.ReplaceEffects(effects);
        }
        catch (const std::invalid_argument &)
        {
            rejected = true;
        }
        Require(rejected && editor.Revision() == effectRevision && editor.Notes().front().scrollMultiplier == .5,
                "Failed effects must preserve the owning tree and lookup pointers.");
        auto copy = editor;
        editor.ReplaceEffects({});
        Require(copy.EffectValueAt(EffectCommandType::NoteSpeed, {2, {}}) == .5 &&
                    editor.EffectValueAt(EffectCommandType::NoteSpeed, {2, {}}) == 1,
                "Effect transactions and copies must not leave dangling lookup pointers.");
    }
} // namespace tests
