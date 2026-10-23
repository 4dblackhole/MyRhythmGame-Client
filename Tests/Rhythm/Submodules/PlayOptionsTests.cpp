#include "Records/PlayRecordStore.h"
#include "Taiko/Results/TaikoPlayResult.h"
#include "TestSupport.h"
#include <limits>

namespace tests
{
    void TestAcceleratedClockAndWindows()
    {
        AccuracyRange range("test", 60);
        for (double rate : {.5, 1., 1.5, 2.})
        {
            RhythmTimer timer;
            timer.Start(1'000'000, 1'000'000, RhythmTime{-100'000}, rate);
            timer.AnchorDspClock(RhythmTime{-100'000}, 48'000, 48'000);
            const auto at = timer.Now(2'000'000);
            Require(at == RhythmTime{static_cast<RhythmTime::rep>(rate * 1'000'000 - 100'000)},
                    "Rate scales QPC chart time once.");
            Require(timer.ToDspClock(at) == 96'000 && timer.FromDspClock(96'000) == at,
                    "Rate preserves roundtrip QPC/DSP mapping.");
            const auto window = range.HalfWindow(JudgementGrade::Good);
            const auto wallEdge = static_cast<std::int64_t>(std::ceil(window.count() / rate));
            timer.Start(0, 1'000'000, RhythmTime{}, rate);
            Require(timer.Now(wallEdge).count() >= window.count(),
                    "Real-time judgement half-window is chart window divided by rate.");
            timer.Pause(2'000'000);
            const auto frozen = timer.Now(9'000'000);
            timer.Resume(9'000'000);
            Require(timer.Now(10'000'000) == frozen + RhythmTime{static_cast<RhythmTime::rep>(rate * 1'000'000)},
                    "Pause/resume preserves rate.");
            timer.Seek(RhythmTime{123}, 11'000'000);
            Require(timer.Now(11'000'000) == RhythmTime{123}, "Seek uses original chart coordinates.");
        }
    }
    void TestTaikoPlayOptions()
    {
        PatternDocument pattern;
        pattern.mode = "Taiko";
        pattern.baseBpm = 120;
        pattern.judgementLevel = 50;
        auto add = [&](int measure, int type, int action, std::vector<std::string> extra = {})
        {
            PatternNote n;
            n.position = {measure, {}};
            n.keyType = type;
            n.actionType = action;
            n.extraData = std::move(extra);
            n.sourceOrder = pattern.notes.size();
            pattern.notes.push_back(n);
        };
        add(0, 1, 0);
        add(1, 2, 0);
        add(2, 3, 0);
        add(3, 4, 0);
        add(4, 5, 0);
        add(5, 17, 1, {"Action=Don", "TickDivision=16"});
        add(6, 17, 2);
        add(7, 17, 1, {"Action=Kat", "TickDivision=16"});
        add(8, 17, 2);
        EffectDocument effects;
        EffectCommand speed;
        speed.type = EffectCommandType::ScrollSpeed;
        speed.target = "Whole";
        speed.beginValue = speed.endValue = 3;
        effects.commands.push_back(speed);
        speed.type = EffectCommandType::NoteSpeed;
        speed.target = "Separate";
        speed.beginValue = speed.endValue = 2;
        effects.commands.push_back(speed);
        PlaySessionOptions options;
        options.play.mirror = true;
        options.play.judgeLevel = 60;
        options.play.scrollUnit = PlayOptions::ScrollUnit::Bpm;
        options.play.scrollSpeed = 240;
        options.play.ignoreScrollEffects = true;
        options.play.metronome = true;
        auto loaded = TaikoMode(options).CreateSession(pattern, effects);
        Require(loaded.Succeeded(), "Option fixture loads.");
        const std::array expected{NoteVisualKind::Kat,    NoteVisualKind::Don,    NoteVisualKind::BigKat,
                                  NoteVisualKind::BigDon, NoteVisualKind::Purple, NoteVisualKind::KatBuzz,
                                  NoteVisualKind::DonBuzz};
        for (std::size_t i = 0; i < expected.size(); ++i)
            Require(loaded.session->FindNotePresentation(i + 1)->visualKind == expected[i],
                    "Mirror changes all eligible color groups, preserving purple and long pairing.");
        Require(loaded.session->Effects().empty(), "Ignore speed removes Whole/Separate automation.");
        Require(loaded.session->ScrollDistance(RhythmTime{1'000'000}, RhythmTime{}) == 2'000'000,
                "BPM scroll target scales against base BPM.");
        Require(loaded.session->MinimumScrollMultiplier() == 2, "Visibility expansion uses global scroll multiplier.");
        Require(!loaded.session->metronomeBeats.empty() && loaded.session->metronomeBeats.front().accent,
                "Metronome compiles quarter beats and measure accents off the update thread.");
        const auto hit = loaded.session->ProcessInput('D', InputEdge::Pressed, RhythmTime{});
        Require(HasEvent(hit, NoteEventType::Completed) && hit.audioCues.front().sound == "Taiko.Kat.Hit",
                "Mirrored gameplay action and default sound match presentation.");
        auto result = loaded.session->TakeResult(PlayEndReason::Interrupted, RhythmTime{});
        Require(result->Summary().judgementLevel == 60 && result->Summary().playOptions.mirror,
                "Result freezes effective JudgeLevel and options.");
        Require(pattern.notes.front().keyType == 1, "Play options never mutate source pattern documents.");
        options.play.random = true;
        options.play.randomAttemptProbability = 0;
        auto unchanged = TaikoMode(options).CreateSession(pattern);
        for (std::size_t i = 0; i < expected.size(); ++i)
            Require(unchanged.session->FindNotePresentation(i + 1)->visualKind == expected[i],
                    "Zero attempt probability preserves mirrored notes.");
        options.play.randomAttemptProbability = 1;
        options.play.randomSeed = 123456;
        auto a = TaikoMode(options).CreateSession(pattern), b = TaikoMode(options).CreateSession(pattern);
        bool stayed{}, changed{};
        for (std::size_t i = 0; i < expected.size(); ++i)
        {
            const auto kind = a.session->FindNotePresentation(i + 1)->visualKind;
            Require(kind == b.session->FindNotePresentation(i + 1)->visualKind,
                    "Same Random seed reproduces the whole attempt.");
            if (i < 4)
            {
                stayed |= kind == expected[i];
                changed |= kind != expected[i];
            }
            if (i < 2)
                Require(kind == NoteVisualKind::Don || kind == NoteVisualKind::Kat,
                        "Small Random stays in the small group.");
            else if (i < 4)
                Require(kind == NoteVisualKind::BigDon || kind == NoteVisualKind::BigKat,
                        "Big Random stays in the big group.");
            else if (i == 4)
                Require(kind == NoteVisualKind::Purple, "Random excludes purple.");
            else
                Require(kind == NoteVisualKind::DonBuzz || kind == NoteVisualKind::KatBuzz,
                        "Buzz randomization stays in the buzz group.");
        }
        Require(stayed && changed, "100 percent attempts allow both same-color and changed assignments.");
    }
    void TestPersistentRecordConditions()
    {
        TemporaryDirectory directory("youmu-record-options");
        const auto database = directory.Path() / "plays.sqlite3";
        const auto pattern = directory.Path() / "real-pattern.ymp";
        std::ofstream(pattern) << "fixture";
        PlayRecord r;
        r.patternPath = pattern;
        r.mode = "Taiko";
        r.completed = true;
        r.cleared = true;
        r.judgeLevel = 70;
        r.options.random = true;
        r.options.randomSeed = std::numeric_limits<std::uint64_t>::max();
        r.options.randomAttemptProbability = .35;
        r.options.accelerate = 1.5;
        r.options.preserveHitSoundPitch = false;
        r.options.judgeLevel = 70;
        r.accuracy = AccuracyScore::FromUnits(99'123'456);
        r.totalNotes = 123;
        r.processedNotes = 123;
        PlayRecordStore(database).Save(r);
        r.judgeLevel = 50;
        r.options.random = false;
        r.options.judgeLevel.reset();
        PlayRecordStore(database).Save(r);
        auto all = PlayRecordStore(database).Query(pattern, "Taiko", "", 0, 10);
        Require(all.size() == 2 && all[0].id > all[1].id, "A new store instance sees persistent newest-first records.");
        const auto filtered =
            PlayRecordStore(database).Query(pattern, "Taiko", "JudgeLevel >= 60 AND Random = true", 0, 10);
        Require(filtered.size() == 1 && filtered[0].accuracy.Units() == 99'123'456 &&
                    filtered[0].options.randomSeed == std::numeric_limits<std::uint64_t>::max(),
                "Condition query preserves exact accuracy, full uint64 seed and actual effective level.");
        Require(filtered[0].options.judgeLevel == JudgeLevel{70} && !filtered[0].options.preserveHitSoundPitch &&
                    filtered[0].options.accelerate == 1.5,
                "Records preserve independent pitch and JudgeLevel settings.");
        Require(PlayRecordStore(database)
                        .Query(pattern, "Taiko", "NOT (Random = false) OR JudgeLevel < 60", 0, 10)
                        .size() == 2,
                "Boolean precedence and nested conditions work.");
        Require(PlayRecordStore(database).Query(pattern, "Taiko", "RandomSeed = 18446744073709551615", 0, 10).size() ==
                    2,
                "Seed equality uses the full integer range without floating-point loss.");
        Require(PlayRecordStore(database).Query(pattern, "Taiko", "", 1, 1).size() == 1,
                "History pagination does not hide older records.");
        for (const auto invalid : {"JudgeLevel >= 60; DELETE FROM Plays", "FakeOption = 1", "Random > true",
                                   "Random = 2", "JudgeLevel = nan", "(Random=true", "RandomSeed > 1"})
        {
            bool rejected{};
            try
            {
                PlayRecordStore::ValidateQuery(invalid);
            }
            catch (const std::invalid_argument &)
            {
                rejected = true;
            }
            Require(rejected, "Invalid query is rejected before execution.");
        }
        Require(PlayRecordStore(database).Query(pattern, "Taiko", "", 0, 10).size() == 2,
                "Invalid queries never mutate stored records.");
    }
} // namespace tests
