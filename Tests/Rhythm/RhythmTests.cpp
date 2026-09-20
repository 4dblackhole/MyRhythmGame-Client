#include "Catalog/SongCatalog.h"
#include "Judgement/JudgementProfile.h"
#include "Lane/Lane.h"
#include "Mode/PlayGameMode.h"
#include "Note/Note.h"
#include "Parsing/ChartParser.h"
#include "Taiko/TaikoMode.h"
#include "Time/RhythmTimer.h"
#include "Timing/MusicalTimeline.h"
#include "Editing/ChartEditor.h"
#include "../../Client/EditorScene/EditorAudioAnalysis.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    using namespace finger_drum;

    void Require(const bool condition, const std::string& message)
    {
        if (!condition)
        {
            throw std::runtime_error(message);
        }
    }

    class TemporaryDirectory final
    {
    public:
        explicit TemporaryDirectory(std::string_view name)
        {
            const auto nonce = std::chrono::steady_clock::now()
                .time_since_epoch().count();
            path_ = std::filesystem::temp_directory_path() /
                (std::string(name) + "-" + std::to_string(nonce));
            std::filesystem::create_directories(path_);
        }

        ~TemporaryDirectory()
        {
            std::error_code ignored;
            std::filesystem::remove_all(path_, ignored);
        }

        [[nodiscard]] const std::filesystem::path& Path() const noexcept
        {
            return path_;
        }

    private:
        std::filesystem::path path_;
    };

    void WriteTextFile(
        const std::filesystem::path& path,
        const std::string_view contents)
    {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream stream(path, std::ios::binary);
        if (!stream)
        {
            throw std::runtime_error("Failed to create a temporary chart file.");
        }
        stream.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    }

    [[nodiscard]] bool HasEvent(
        const rhythm::NoteProcessResult& result,
        const rhythm::NoteEventType type)
    {
        return std::ranges::any_of(
            result.events,
            [type](const rhythm::NoteEvent& event)
            {
                return event.type == type;
            });
    }

    void TestJudgementScalingAndInterpolation()
    {
        rhythm::JudgementProfile profile("default", 50);
        Require(
            profile.HalfWindow(rhythm::JudgementGrade::Max).count() == 4'500,
            "Level 50 Max window must be 4.5 ms.");
        profile.SetLevel(100);
        Require(
            profile.HalfWindow(rhythm::JudgementGrade::Max).count() == 2'250 &&
            profile.HalfWindow(rhythm::JudgementGrade::Perfect).count() == 4'750,
            "Level 100 windows must be exactly half of level 50.");

        const std::array<rhythm::JudgementBand,
            rhythm::JudgementProfile::BandCount> bands{{
            {rhythm::JudgementGrade::Max, rhythm::RhythmDuration{5'000}, 1.0},
            {rhythm::JudgementGrade::Perfect, rhythm::RhythmDuration{9'000}, 0.9},
            {rhythm::JudgementGrade::Great, rhythm::RhythmDuration{20'000}, 0.8},
            {rhythm::JudgementGrade::Good, rhythm::RhythmDuration{50'000}, 0.5},
            {rhythm::JudgementGrade::Bad, rhythm::RhythmDuration{90'000}, 0.0},
        }};
        rhythm::JudgementProfile interpolation("linear", 50, bands);
        const rhythm::JudgementResult result = interpolation.Evaluate(
            rhythm::RhythmTime::zero(),
            rhythm::RhythmTime{7'000});
        Require(
            result.grade == rhythm::JudgementGrade::Perfect &&
            std::abs(result.scoreRate - 0.95) < 1e-9,
            "A 7 ms hit between 5/9 ms anchors must score 95 percent.");
    }

    void TestRhythmTimerClockMapping()
    {
        rhythm::RhythmTimer timer;
        timer.Start(1'000, 1'000, rhythm::RhythmTime{250'000});
        Require(
            timer.Now(1'500) == rhythm::RhythmTime{750'000},
            "QPC mapping must advance the single rhythm timeline.");
        timer.AnchorDspClock(rhythm::RhythmTime{750'000}, 48'000, 48'000);
        Require(
            timer.ToDspClock(rhythm::RhythmTime{1'750'000}) == 96'000,
            "One second of rhythm time must map to one mixer sample-rate span.");
    }

    [[nodiscard]] std::unique_ptr<rhythm::INote> MakeTap(
        const rhythm::NoteId id,
        const rhythm::RhythmTime time,
        const std::shared_ptr<const rhythm::JudgementProfile>& profile)
    {
        return std::make_unique<rhythm::RuleBasedNote>(
            id,
            time,
            profile,
            std::make_unique<rhythm::TapInputRule>(1));
    }

    void TestLaneFocusRules()
    {
        auto profile = std::make_shared<rhythm::JudgementProfile>();
        rhythm::Lane lane;
        lane.AddNote(MakeTap(1, rhythm::RhythmTime::zero(), profile));
        lane.AddNote(MakeTap(2, rhythm::RhythmTime{100'000}, profile));
        lane.Finalize();

        const rhythm::NoteProcessResult early = lane.ProcessInput({
            rhythm::RhythmTime{-70'000}, 1, 'F', rhythm::InputEdge::Pressed});
        Require(
            lane.CurrentIndex() == 0 &&
            HasEvent(early, rhythm::NoteEventType::InputRejected),
            "Early Bad must keep focus on the first note.");

        const rhythm::NoteProcessResult forwarded = lane.ProcessInput({
            rhythm::RhythmTime{60'000}, 1, 'F', rhythm::InputEdge::Pressed});
        Require(
            HasEvent(forwarded, rhythm::NoteEventType::Missed) &&
            HasEvent(forwarded, rhythm::NoteEventType::HitAccepted) &&
            lane.CurrentIndex() == 2,
            "Late Bad must miss the old note and apply the same Good input to the next note.");
    }

    void TestLargeNoteSoundState()
    {
        chart::PatternDocument pattern;
        pattern.mode = "Taiko";
        pattern.baseBpm = 120.0;
        pattern.judgementLevel = 50;
        pattern.notes.push_back(chart::PatternNote{
            .position = {0, chart::Rational{0, 1}},
            .keyType = static_cast<int>(mode::TaikoNoteType::BigDon),
            .actionType = static_cast<int>(mode::TaikoPatternAction::Down)});

        mode::TaikoMode taiko;
        mode::ModeLoadResult loaded = taiko.CreateSession(pattern);
        Require(loaded.Succeeded(), "Taiko mode must create the large-note session.");

        const rhythm::NoteProcessResult early = loaded.session->ProcessInput(
            'F', rhythm::InputEdge::Pressed, rhythm::RhythmTime{-60'000});
        Require(
            early.audioCues.size() == 1 &&
            early.audioCues.front().sound == "Taiko.Don.FreeInput",
            "An out-of-Good large note must not play its dedicated sound.");

        const rhythm::NoteProcessResult first = loaded.session->ProcessInput(
            'F', rhythm::InputEdge::Pressed, rhythm::RhythmTime::zero());
        Require(
            first.audioCues.size() == 1 &&
            first.audioCues.front().sound == "Taiko.BigDon.FirstHit",
            "The first Good large-note hit must emit exactly one large-note cue.");

        const rhythm::NoteProcessResult second = loaded.session->ProcessInput(
            'J', rhythm::InputEdge::Pressed, rhythm::RhythmTime{1'000});
        Require(
            second.audioCues.empty() &&
            HasEvent(second, rhythm::NoteEventType::Completed),
            "The second large-note hit must complete without replaying the first-hit cue.");
    }

    void TestHoldTicksAreExactlyOnce()
    {
        auto profile = std::make_shared<rhythm::JudgementProfile>();
        auto sound = std::make_shared<rhythm::MappedNoteSoundPolicy>();
        sound->Bind(rhythm::SoundBinding{
            .eventType = rhythm::NoteEventType::TickAccepted,
            .cue = rhythm::AudioCueRequest{.sound = "tick"}});
        rhythm::RuleBasedNote note(
            7,
            rhythm::RhythmTime::zero(),
            profile,
            std::make_unique<rhythm::HoldInputRule>(
                1,
                rhythm::RhythmTime{30'000},
                std::vector<rhythm::RhythmTime>{
                    rhythm::RhythmTime{10'000},
                    rhythm::RhythmTime{20'000}}),
            sound);
        static_cast<void>(note.ProcessInput({
            rhythm::RhythmTime::zero(), 1, 'F', rhythm::InputEdge::Pressed}));
        const std::array<rhythm::NoteAction, 1> held{1};
        const rhythm::NoteProcessResult first = note.Update({
            rhythm::RhythmTime{15'000}, held});
        const rhythm::NoteProcessResult repeated = note.Update({
            rhythm::RhythmTime{15'000}, held});
        const rhythm::NoteProcessResult second = note.Update({
            rhythm::RhythmTime{25'000}, held});
        Require(
            first.audioCues.size() == 1 &&
            repeated.audioCues.empty() &&
            second.audioCues.size() == 1,
            "Each hold tick must emit one sound even across repeated updates.");
    }

    void TestLongNoteRemainsInScrollSnapshotUntilItsTail()
    {
        auto profile = std::make_shared<rhythm::JudgementProfile>();
        rhythm::ScrollGear gear;
        rhythm::Lane& lane = gear.CreateLane();
        lane.AddNote(std::make_unique<rhythm::RuleBasedNote>(
            1,
            rhythm::RhythmTime::zero(),
            profile,
            std::make_unique<rhythm::TimedSequenceInputRule>(
                std::vector<rhythm::NoteAction>{1},
                3,
                rhythm::RhythmTime{1'000'000})));
        gear.Finalize();

        const rhythm::ScrollGearSnapshot active = gear.BuildSnapshot(
            rhythm::RhythmTime{500'000},
            rhythm::RhythmDuration{2'000'000},
            rhythm::RhythmDuration{220'000});
        Require(
            active.notes.size() == 1 &&
            active.notes.front().expireTime == rhythm::RhythmTime{1'000'000},
            "A long note head must remain visible after the ordinary past "
            "window until its tail expires.");

        static_cast<void>(gear.Update(rhythm::RhythmTime{1'000'000}));
        const rhythm::ScrollGearSnapshot missedTrail = gear.BuildSnapshot(
            rhythm::RhythmTime{1'100'000},
            rhythm::RhythmDuration{2'000'000},
            rhythm::RhythmDuration{220'000});
        const rhythm::ScrollGearSnapshot expiredTrail = gear.BuildSnapshot(
            rhythm::RhythmTime{1'220'001},
            rhythm::RhythmDuration{2'000'000},
            rhythm::RhythmDuration{220'000});
        Require(
            missedTrail.notes.size() == 1 &&
            missedTrail.notes.front().state == rhythm::NoteState::Missed &&
            expiredTrail.notes.empty(),
            "A missed timed note must remain in the snapshot for its explicit "
            "post-expiry travel window only.");
    }

    void TestOverlappingLongTrailIsNotHiddenByShorterNote()
    {
        auto profile = std::make_shared<rhythm::JudgementProfile>();
        rhythm::ScrollGear gear;
        rhythm::Lane& lane = gear.CreateLane();
        lane.AddNote(std::make_unique<rhythm::RuleBasedNote>(
            1,
            rhythm::RhythmTime::zero(),
            profile,
            std::make_unique<rhythm::TimedSequenceInputRule>(
                std::vector<rhythm::NoteAction>{1},
                2,
                rhythm::RhythmTime{1'000'000})));
        lane.AddNote(std::make_unique<rhythm::RuleBasedNote>(
            2,
            rhythm::RhythmTime{100'000},
            profile,
            std::make_unique<rhythm::TimedSequenceInputRule>(
                std::vector<rhythm::NoteAction>{1},
                2,
                rhythm::RhythmTime{100'000})));
        gear.Finalize();

        static_cast<void>(gear.Update(rhythm::RhythmTime{1'000'000}));
        const rhythm::ScrollGearSnapshot snapshot = gear.BuildSnapshot(
            rhythm::RhythmTime{1'100'000},
            rhythm::RhythmDuration{2'000'000},
            rhythm::RhythmDuration{220'000});
        Require(
            snapshot.notes.size() == 1 && snapshot.notes.front().noteId == 1,
            "An older long-note trail must remain visible when an intervening "
            "short note has already left the past window.");
    }

    [[nodiscard]] chart::PatternDocument MakeLongPattern(
        const mode::TaikoNoteType type,
        std::vector<std::string> extraData = {})
    {
        chart::PatternDocument pattern;
        pattern.mode = "Taiko";
        pattern.baseBpm = 120.0;
        pattern.judgementLevel = 50;
        pattern.notes.push_back(chart::PatternNote{
            .position = {0, chart::Rational{0, 1}},
            .keyType = static_cast<int>(type),
            .actionType = static_cast<int>(
                mode::TaikoPatternAction::LongNoteStart),
            .extraData = std::move(extraData),
            .sourceOrder = 0});
        pattern.notes.push_back(chart::PatternNote{
            .position = {0, chart::Rational{1, 2}},
            .keyType = static_cast<int>(type),
            .actionType = static_cast<int>(
                mode::TaikoPatternAction::LongNoteEnd),
            .sourceOrder = 1});
        return pattern;
    }

    void TestTaikoRollAndTickRollRules()
    {
        mode::TaikoMode taiko;
        mode::ModeLoadResult roll = taiko.CreateSession(
            MakeLongPattern(mode::TaikoNoteType::Roll));
        Require(roll.Succeeded(), "Roll must create a playable session.");
        const rhythm::NoteProcessResult don = roll.session->ProcessInput(
            'F', rhythm::InputEdge::Pressed, rhythm::RhythmTime{100'000});
        const rhythm::NoteProcessResult kat = roll.session->ProcessInput(
            'D', rhythm::InputEdge::Pressed, rhythm::RhythmTime{200'000});
        Require(
            HasEvent(don, rhythm::NoteEventType::TickAccepted) &&
            HasEvent(kat, rhythm::NoteEventType::TickAccepted),
            "Roll must accept arbitrary Don and Kat presses in its interval.");

        mode::ModeLoadResult tickRoll = taiko.CreateSession(MakeLongPattern(
            mode::TaikoNoteType::TickRoll,
            {"TickDivision=16"}));
        Require(tickRoll.Succeeded(), "TickRoll must create a playable session.");
        Require(
            tickRoll.session->FindNotePresentation(1)->tickTimes.size() == 8,
            "TickRoll presentation must expose every authored tick to the scene.");
        const rhythm::NoteProcessResult first = tickRoll.session->ProcessInput(
            'F', rhythm::InputEdge::Pressed, rhythm::RhythmTime::zero());
        const rhythm::NoteProcessResult duplicate =
            tickRoll.session->ProcessInput(
                'D', rhythm::InputEdge::Pressed, rhythm::RhythmTime::zero());
        const rhythm::NoteProcessResult outsideGood =
            tickRoll.session->ProcessInput(
                'F', rhythm::InputEdge::Pressed, rhythm::RhythmTime{70'000});
        const rhythm::NoteProcessResult insideGood =
            tickRoll.session->ProcessInput(
                'D', rhythm::InputEdge::Pressed, rhythm::RhythmTime{71'000});
        Require(
            HasEvent(first, rhythm::NoteEventType::TickAccepted) &&
            !HasEvent(duplicate, rhythm::NoteEventType::TickAccepted) &&
            !HasEvent(outsideGood, rhythm::NoteEventType::TickAccepted) &&
            HasEvent(insideGood, rhythm::NoteEventType::TickAccepted),
            "TickRoll must cap each tick to one hit and use the Good window.");

        mode::ModeLoadResult bigTickRoll = taiko.CreateSession(MakeLongPattern(
            mode::TaikoNoteType::BigTickRoll,
            {"TickDivision=16"}));
        mode::ModeLoadResult bigRoll = taiko.CreateSession(
            MakeLongPattern(mode::TaikoNoteType::BigRoll));
        Require(
            bigRoll.Succeeded() &&
            bigRoll.session->FindNotePresentation(1)->visualId ==
                "Taiko.BigRoll" &&
            bigTickRoll.Succeeded() &&
            bigTickRoll.session->FindNotePresentation(1)->visualId ==
                "Taiko.BigRoll",
            "BigRoll variants must use the large long-note presentation.");
    }

    void TestBalloonDengDengAndBuzzRules()
    {
        mode::TaikoMode taiko;
        constexpr std::string_view BareBalloonPattern = R"(
[Metadata]
Base BPM: 120
[Difficulty]
Mode: Taiko
[Time Signature]
[Pattern]
0/1,15,1,,    8
1/4,15,2
)";
        const auto parsedBareBalloon = chart::ChartParser{}.ParsePattern(
            BareBalloonPattern,
            "bare-balloon.ymp");
        mode::ModeLoadResult parsedBalloon = taiko.CreateSession(
            parsedBareBalloon.document);
        Require(
            parsedBareBalloon.Succeeded() && parsedBalloon.Succeeded() &&
            parsedBalloon.session->Gear().Lanes().front()->Notes().front()->
                Progress() == rhythm::NoteProgress{0, 8},
            "The YMP parser and Taiko mode must preserve a bare Balloon "
            "ExtraData count.");

        mode::ModeLoadResult balloon = taiko.CreateSession(MakeLongPattern(
            mode::TaikoNoteType::Balloon,
            {"3"}));
        Require(
            balloon.Succeeded() &&
            balloon.session->Gear().Lanes().front()->Notes().front()->
                Progress() == rhythm::NoteProgress{0, 3},
            "Balloon must accept a bare ExtraData hit count.");
        const rhythm::ScrollGearSnapshot initialBalloonSnapshot =
            balloon.session->Gear().BuildSnapshot(
                rhythm::RhythmTime::zero(),
                rhythm::RhythmDuration{2'000'000});
        Require(
            initialBalloonSnapshot.notes.size() == 1 &&
            initialBalloonSnapshot.notes.front().progress ==
                rhythm::NoteProgress{0, 3},
            "The scroll snapshot must expose Balloon progress to presentation.");
        const rhythm::NoteProcessResult wrongBalloon =
            balloon.session->ProcessInput(
                'D', rhythm::InputEdge::Pressed, rhythm::RhythmTime{10'000});
        static_cast<void>(balloon.session->ProcessInput(
            'F', rhythm::InputEdge::Pressed, rhythm::RhythmTime{20'000}));
        static_cast<void>(balloon.session->ProcessInput(
            'J', rhythm::InputEdge::Pressed, rhythm::RhythmTime{30'000}));
        const rhythm::ScrollGearSnapshot activeBalloonSnapshot =
            balloon.session->Gear().BuildSnapshot(
                rhythm::RhythmTime{30'000},
                rhythm::RhythmDuration{2'000'000});
        Require(
            activeBalloonSnapshot.notes.size() == 1 &&
            activeBalloonSnapshot.notes.front().progress ==
                rhythm::NoteProgress{2, 3},
            "The scroll snapshot must update Balloon progress after hits.");
        const rhythm::NoteProcessResult popped = balloon.session->ProcessInput(
            'F', rhythm::InputEdge::Pressed, rhythm::RhythmTime{40'000});
        Require(
            !HasEvent(wrongBalloon, rhythm::NoteEventType::HitAccepted) &&
            HasEvent(popped, rhythm::NoteEventType::Completed) &&
            std::ranges::any_of(
                popped.audioCues,
                [](const rhythm::AudioCueRequest& cue)
                {
                    return cue.sound == "Taiko.Balloon.Pop";
                }),
            "Balloon must count only Don and play its pop cue on completion.");

        chart::PatternDocument defaultBalloonPattern = MakeLongPattern(
            mode::TaikoNoteType::Balloon);
        defaultBalloonPattern.notes.back().position.fraction =
            chart::Rational{1, 4};
        mode::ModeLoadResult defaultBalloon = taiko.CreateSession(
            defaultBalloonPattern);
        Require(
            defaultBalloon.Succeeded() &&
            defaultBalloon.session->Gear().Lanes().front()->Notes().front()->
                Progress() == rhythm::NoteProgress{0, 3},
            "An unspecified quarter-note Balloon must default to "
            "ceil(1/4 * 12) = 3 hits.");
        static_cast<void>(defaultBalloon.session->ProcessInput(
            'F', rhythm::InputEdge::Pressed, rhythm::RhythmTime{10'000}));
        static_cast<void>(defaultBalloon.session->ProcessInput(
            'J', rhythm::InputEdge::Pressed, rhythm::RhythmTime{20'000}));
        const rhythm::NoteProcessResult defaultPopped =
            defaultBalloon.session->ProcessInput(
                'F', rhythm::InputEdge::Pressed, rhythm::RhythmTime{30'000});
        Require(
            HasEvent(defaultPopped, rhythm::NoteEventType::Completed),
            "The computed Balloon hit count must drive completion.");

        mode::ModeLoadResult dengDeng = taiko.CreateSession(MakeLongPattern(
            mode::TaikoNoteType::DengDeng,
            {"HitCount=4"}));
        Require(dengDeng.Succeeded(), "DengDeng must create a playable session.");
        static_cast<void>(dengDeng.session->ProcessInput(
            'F', rhythm::InputEdge::Pressed, rhythm::RhythmTime{10'000}));
        const rhythm::NoteProcessResult repeatedDon =
            dengDeng.session->ProcessInput(
                'J', rhythm::InputEdge::Pressed, rhythm::RhythmTime{20'000});
        static_cast<void>(dengDeng.session->ProcessInput(
            'D', rhythm::InputEdge::Pressed, rhythm::RhythmTime{30'000}));
        static_cast<void>(dengDeng.session->ProcessInput(
            'F', rhythm::InputEdge::Pressed, rhythm::RhythmTime{40'000}));
        const rhythm::NoteProcessResult alternated =
            dengDeng.session->ProcessInput(
                'K', rhythm::InputEdge::Pressed, rhythm::RhythmTime{50'000});
        Require(
            !HasEvent(repeatedDon, rhythm::NoteEventType::HitAccepted) &&
            HasEvent(alternated, rhythm::NoteEventType::Completed),
            "DengDeng must require a Don/Kat alternating sequence.");

        mode::ModeLoadResult donBuzz = taiko.CreateSession(MakeLongPattern(
            mode::TaikoNoteType::Buzz,
            {"Action=Don", "TickDivision=16"}));
        Require(donBuzz.Succeeded(), "Don Buzz must create a playable session.");
        static_cast<void>(donBuzz.session->ProcessInput(
            'F', rhythm::InputEdge::Pressed, rhythm::RhythmTime::zero()));
        const std::array<rhythm::PhysicalKey, 1> heldDon{'F'};
        const rhythm::NoteProcessResult donTick = donBuzz.session->Update(
            rhythm::RhythmTime{125'000}, heldDon);
        const rhythm::NoteProcessResult releasedTick = donBuzz.session->Update(
            rhythm::RhythmTime{250'000});
        Require(
            HasEvent(donTick, rhythm::NoteEventType::TickAccepted) &&
            donTick.audioCues.size() == 1 &&
            donTick.audioCues.front().sound == "Taiko.Don.Hit" &&
            HasEvent(releasedTick, rhythm::NoteEventType::TickMissed),
            "Buzz must sound only authored ticks while its action is held.");

        mode::ModeLoadResult katBuzz = taiko.CreateSession(MakeLongPattern(
            mode::TaikoNoteType::Buzz,
            {"Action=Kat", "TickDivision=16"}));
        Require(
            katBuzz.Succeeded() &&
            katBuzz.session->FindNotePresentation(1)->visualId ==
                "Taiko.Buzz.Kat",
            "Buzz must preserve the selected Don/Kat presentation.");

        mode::ModeLoadResult invalidBuzz = taiko.CreateSession(MakeLongPattern(
            mode::TaikoNoteType::Buzz,
            {"Action=Center"}));
        Require(
            !invalidBuzz.Succeeded(),
            "Buzz must reject an action other than Don or Kat.");
    }

    void TestTimingAccuracyAndSessionAggregation()
    {
        auto profile = std::make_shared<rhythm::JudgementProfile>();
        mode::PlaySession session;
        session.SetInputMapping({{'F', 1}, {'J', 1}});
        auto& lane = session.Gear().CreateLane();
        lane.AddNote(std::make_unique<rhythm::RuleBasedNote>(1,
            rhythm::RhythmTime{0}, profile,
            std::make_unique<rhythm::CountedHitInputRule>(1, 2)));
        lane.AddNote(MakeTap(2, rhythm::RhythmTime{200'000}, profile));
        session.Gear().Finalize();
        Require(!session.AccuracyRate(), "Accuracy must be unavailable before a final note.");
        const auto first = session.ProcessInput('F', rhythm::InputEdge::Pressed,
            rhythm::RhythmTime{0});
        Require(first.finalizedAccuracies.empty(), "Big first hit must not finalize accuracy.");
        const auto second = session.ProcessInput('J', rhythm::InputEdge::Pressed,
            rhythm::RhythmTime{16'500});
        const double hit2 = profile->Evaluate({}, rhythm::RhythmTime{16'500}).scoreRate;
        const double bigRate = (1.0 + hit2) * 0.5;
        Require(second.finalizedAccuracies.size() == 1 &&
            std::abs(*session.AccuracyRate() - bigRate) < 1e-9 &&
            session.LastNoteAccuracy()->hitScoreRates == std::vector<double>{1.0, hit2},
            "Big accuracy must preserve both interpolated hits and their mean.");
        static_cast<void>(session.Update(rhythm::RhythmTime{400'000}));
        static_cast<void>(session.Update(rhythm::RhythmTime{500'000}));
        Require(session.FinalizedNoteCount() == 2 &&
            std::abs(*session.AccuracyRate() - bigRate * 0.5) < 1e-9,
            "Each logical note, including a miss, must contribute exactly once.");
        session.Reset();
        Require(!session.AccuracyRate() && !session.LastNoteAccuracy(),
            "Reset must clear totals and the last-note details.");
        static_cast<void>(session.ProcessInput('F', rhythm::InputEdge::Pressed, {}));
        const auto partial = session.Update(rhythm::RhythmTime{60'000});
        Require(partial.finalizedAccuracies.size() == 1 &&
            partial.finalizedAccuracies.front().ScoreRate() == 0.5 &&
            partial.finalizedAccuracies.front().hitScoreRates[1] == 0.0,
            "An unfilled second hit must count as zero, not discard the first hit.");
#if defined(_DEBUG)
        Require(session.LastNoteAccuracy()->DebugText().find(L"Hit2=0.00%") !=
            std::wstring::npos, "Debug details must expose unfilled hit components.");
#endif
    }

    void TestPurpleNoteBothOrders()
    {
        chart::PatternDocument pattern;
        pattern.baseBpm = 120.0;
        pattern.notes.push_back(chart::PatternNote{
            .position = {0, chart::Rational{0, 1}},
            .keyType = 5,
            .actionType = static_cast<int>(mode::TaikoPatternAction::Down)});
        for (const bool katFirst : {false, true})
        {
            auto loaded = mode::TaikoMode{}.CreateSession(pattern);
            Require(loaded.Succeeded() &&
                loaded.session->FindNotePresentation(1)->visualId == "Taiko.Purple",
                "ID 5 must create a purple large-note presentation.");
            const auto firstKey = katFirst ? 'D' : 'F';
            const auto secondKey = katFirst ? 'F' : 'D';
            const auto first = loaded.session->ProcessInput(firstKey,
                rhythm::InputEdge::Pressed, {});
            const auto repeat = loaded.session->ProcessInput(firstKey,
                rhythm::InputEdge::Pressed, rhythm::RhythmTime{1'000});
            const auto second = loaded.session->ProcessInput(secondKey,
                rhythm::InputEdge::Pressed, rhythm::RhythmTime{2'000});
            Require(first.audioCues.size() == 1 &&
                first.audioCues.front().sound == (katFirst ? "Taiko.Kat.Hit" : "Taiko.Don.Hit") &&
                !HasEvent(repeat, rhythm::NoteEventType::HitAccepted) &&
                HasEvent(second, rhythm::NoteEventType::Completed) &&
                loaded.session->AccuracyRate() == 1.0,
                "Purple must accept Don and Kat once each, in either order.");
        }
    }

    void TestCountedLongNoteAccuracyAndSyntax()
    {
        for (const auto type : {mode::TaikoNoteType::Roll, mode::TaikoNoteType::BigRoll,
            mode::TaikoNoteType::Balloon, mode::TaikoNoteType::DengDeng})
        {
            for (const std::string count : {"10", "HitCount=10"})
            {
                const std::string pattern = "[Metadata]\nBase BPM: 120\n"
                    "[Difficulty]\nMode: Taiko\n[Time Signature]\n[Pattern]\n"
                    "0/1," + std::to_string(static_cast<int>(type)) + ",1,," + count +
                    "\n1/2," + std::to_string(static_cast<int>(type)) + ",2\n";
                const auto parsed = chart::ChartParser{}.ParsePattern(pattern, "count.ymp");
                auto loaded = mode::TaikoMode{}.CreateSession(parsed.document);
                Require(parsed.Succeeded() && loaded.Succeeded(),
                    "All counted long notes must parse bare and named ExtraData counts.");
                for (int hit = 0; hit < 8; ++hit)
                {
                    const auto key = type == mode::TaikoNoteType::DengDeng && hit % 2 != 0
                        ? 'D' : 'F';
                    static_cast<void>(loaded.session->ProcessInput(key,
                        rhythm::InputEdge::Pressed, rhythm::RhythmTime{hit * 10'000}));
                }
                const auto expired = loaded.session->Update(rhythm::RhythmTime{1'000'000});
                Require(HasEvent(expired, rhythm::NoteEventType::Missed) &&
                    std::abs(*loaded.session->AccuracyRate() - 0.8) < 1e-9 &&
                    loaded.session->LastNoteAccuracy()->acceptedHits == 8 &&
                    loaded.session->LastNoteAccuracy()->target.hits == 10,
                    "Eight of ten hits must finalize at 80 percent, even on timeout.");
            }
            auto pattern = MakeLongPattern(type);
            pattern.notes.back().position.fraction = chart::Rational{1, 7};
            auto loaded = mode::TaikoMode{}.CreateSession(pattern);
            Require(loaded.Succeeded() && loaded.session->Gear().Lanes().front()->
                Notes().front()->Accuracy().target.hits == 2,
                "All counted defaults must use exact ceil(1/7 * 12) = 2.");
            const auto invalid = mode::TaikoMode{}.CreateSession(MakeLongPattern(type, {"0"}));
            Require(!invalid.Succeeded(), "A nonpositive explicit target must be rejected.");
        }
        auto roll = mode::TaikoMode{}.CreateSession(MakeLongPattern(mode::TaikoNoteType::Roll, {"2"}));
        for (int hit = 0; hit < 5; ++hit)
        {
            static_cast<void>(roll.session->ProcessInput('F', rhythm::InputEdge::Pressed,
                rhythm::RhythmTime{hit * 1'000}));
        }
        static_cast<void>(roll.session->Update(rhythm::RhythmTime{1'000'000}));
        Require(roll.session->AccuracyRate() == 1.0 &&
            roll.session->LastNoteAccuracy()->acceptedHits == 5,
            "Roll may keep accepting hits, but its final accuracy must cap at 100 percent.");
    }

    void TestTickAndHoldAccuracy()
    {
        auto tickRoll = mode::TaikoMode{}.CreateSession(MakeLongPattern(
            mode::TaikoNoteType::TickRoll));
        const auto early = tickRoll.session->ProcessInput('F', rhythm::InputEdge::Pressed,
            rhythm::RhythmTime{-70'000});
        Require(std::ranges::none_of(early.events, [](const auto& event)
            { return event.judgement.grade == rhythm::JudgementGrade::Bad; }),
            "TickRoll must not emit a head BAD judgement.");
        for (const int time : {-54'500, 70'500})
        {
            const auto hit = tickRoll.session->ProcessInput('F', rhythm::InputEdge::Pressed,
                rhythm::RhythmTime{time});
            Require(std::ranges::any_of(hit.events, [](const auto& event)
                { return event.type == rhythm::NoteEventType::TickAccepted &&
                    event.judgement.grade == rhythm::JudgementGrade::Unjudged &&
                    event.judgement.scoreRate == 1.0; }),
                "Each TickRoll tick, including head, is binary within inclusive Good.");
        }
        static_cast<void>(tickRoll.session->Update(rhythm::RhythmTime{1'000'000}));
        Require(tickRoll.session->AccuracyRate() == 0.25,
            "Two accepted ticks of eight must score 25 percent.");

        auto profile = std::make_shared<rhythm::JudgementProfile>();
        std::vector<rhythm::RhythmTime> ticks;
        for (int tick = 1; tick <= 100; ++tick)
        {
            ticks.emplace_back(tick * 100'000);
        }
        rhythm::RuleBasedNote hold(1, {}, profile,
            std::make_unique<rhythm::HoldInputRule>(1, rhythm::RhythmTime{10'100'000}, ticks));
        // Default interpolation: 14,166 us is approximately 96 percent.
        static_cast<void>(hold.ProcessInput({rhythm::RhythmTime{14'166}, 1, 'F',
            rhythm::InputEdge::Pressed}));
        static_cast<void>(hold.ProcessInput({rhythm::RhythmTime{9'850'000}, 1, 'F',
            rhythm::InputEdge::Released}));
        const auto done = hold.Update({rhythm::RhythmTime{10'100'000}, {}});
        const double head = profile->Evaluate({}, rhythm::RhythmTime{14'166}).scoreRate;
        Require(done.finalizedAccuracies.size() == 1 &&
            hold.Accuracy().acceptedTicks == 98 &&
            std::abs(hold.Accuracy().ScoreRate() - (head + 0.98) * 0.5) < 1e-9 &&
            std::abs(hold.Accuracy().ScoreRate() - 0.97) < 0.00001,
            "Buzz must combine interpolated head and 98/100 held ticks equally.");
        Require(hold.Update({rhythm::RhythmTime{11'000'000}, {}}).finalizedAccuracies.empty(),
            "An ended hold must not emit its accuracy twice.");
        hold.Reset();
        static_cast<void>(hold.ProcessInput({{}, 1, 'F', rhythm::InputEdge::Pressed}));
        static_cast<void>(hold.ProcessInput({rhythm::RhythmTime{1}, 1, 'J', rhythm::InputEdge::Pressed}));
        static_cast<void>(hold.ProcessInput({rhythm::RhythmTime{2}, 1, 'F', rhythm::InputEdge::Released}));
        static_cast<void>(hold.ProcessInput({rhythm::RhythmTime{150'000}, 1, 'J', rhythm::InputEdge::Released}));
        Require(hold.Accuracy().acceptedTicks == 1,
            "Releasing one physical key must not cancel another held key of the same action.");

        auto buzz = mode::TaikoMode{}.CreateSession(MakeLongPattern(mode::TaikoNoteType::Buzz,
            {"Action=Don", "TickDivision=16"}));
        Require(buzz.session->FindNotePresentation(1)->tickTimes.size() == 7 &&
            buzz.session->FindNotePresentation(1)->tickTimes.front() == rhythm::RhythmTime{125'000},
            "Buzz body ticks must exclude the independently judged head.");
    }

    void TestMusicalSubdivisionTicksFollowTempo()
    {
        chart::PatternDocument pattern;
        pattern.baseBpm = 120.0;
        pattern.timing.push_back(chart::TimingDirective{
            .position = {0, chart::Rational{1, 4}},
            .type = chart::TimingDirectiveType::Bpm,
            .value = 240.0});
        const chart::MusicalTimeline timeline(pattern);
        const std::vector<rhythm::RhythmTime> ticks =
            timeline.CompileSubdivisions(
                {0, chart::Rational{0, 1}},
                {0, chart::Rational{1, 2}},
                4);
        Require(
            ticks == std::vector<rhythm::RhythmTime>{
                rhythm::RhythmTime{0},
                rhythm::RhythmTime{500'000}},
            "Musical subdivision ticks must follow BPM changes.");
        Require(
            timeline.CountSubdivisions(
                {0, chart::Rational{0, 1}},
                {0, chart::Rational{1, 8}},
                12) == 2,
            "Subdivision counts must use exact rational ceil(length * divisions).");
    }

    void TestLongNoteKeepsFirstHead()
    {
        chart::PatternDocument pattern;
        pattern.mode = "Taiko";
        pattern.baseBpm = 120.0;
        const auto type = static_cast<int>(mode::TaikoNoteType::TickRoll);
        const auto start = static_cast<int>(
            mode::TaikoPatternAction::LongNoteStart);
        const auto end = static_cast<int>(
            mode::TaikoPatternAction::LongNoteEnd);
        pattern.notes.push_back(chart::PatternNote{
            .position = {0, chart::Rational{0, 1}},
            .keyType = type,
            .actionType = start});
        pattern.notes.push_back(chart::PatternNote{
            .position = {0, chart::Rational{1, 4}},
            .keyType = type,
            .actionType = start});
        pattern.notes.push_back(chart::PatternNote{
            .position = {0, chart::Rational{2, 4}},
            .keyType = type,
            .actionType = end});

        mode::TaikoMode taiko;
        mode::ModeLoadResult loaded = taiko.CreateSession(pattern);
        Require(loaded.Succeeded(), "Taiko mode must create the long-note session.");
        const auto& notes = loaded.session->Gear().Lanes().front()->Notes();
        Require(
            notes.size() == 1 &&
            notes.front()->Timing() == rhythm::RhythmTime::zero(),
            "A duplicate LNStart must not replace the first active head.");
    }

    void TestTaikoUsesOneLaneForEveryNoteType()
    {
        chart::PatternDocument pattern;
        pattern.mode = "Taiko";
        pattern.baseBpm = 120.0;
        const auto down = static_cast<int>(mode::TaikoPatternAction::Down);
        const auto start = static_cast<int>(
            mode::TaikoPatternAction::LongNoteStart);
        const auto end = static_cast<int>(
            mode::TaikoPatternAction::LongNoteEnd);
        const auto add = [&pattern](
            const std::int64_t numerator,
            const mode::TaikoNoteType type,
            const int action)
        {
            pattern.notes.push_back(chart::PatternNote{
                .position = {0, chart::Rational{numerator, 8}},
                .keyType = static_cast<int>(type),
                .actionType = action,
                .sourceOrder = pattern.notes.size()});
        };
        add(0, mode::TaikoNoteType::Don, down);
        add(1, mode::TaikoNoteType::Kat, down);
        add(2, mode::TaikoNoteType::BigDon, down);
        add(3, mode::TaikoNoteType::BigKat, down);
        add(4, mode::TaikoNoteType::Roll, start);
        add(5, mode::TaikoNoteType::Don, down); // Ignored inside the roll.
        add(6, mode::TaikoNoteType::Roll, end);

        mode::TaikoMode taiko;
        mode::ModeLoadResult loaded = taiko.CreateSession(pattern);
        Require(loaded.Succeeded(), "Taiko mode must create a mixed session.");
        Require(
            loaded.session->Gear().LaneCount() == 1 &&
            loaded.session->Gear().Lanes().front()->Notes().size() == 5,
            "Don, Kat, large notes and rolls must share one ordered Lane.");
        Require(
            loaded.session->FindNotePresentation(1)->visualId == "Taiko.Don" &&
            loaded.session->FindNotePresentation(4)->visualId == "Taiko.BigKat" &&
            loaded.session->FindNotePresentation(5)->visualId == "Taiko.Roll" &&
            loaded.session->FindNotePresentation(5)->hasEndTime,
            "Every logical note must expose its mode-owned visual identity.");
    }

    void TestTaikoInputBindings()
    {
        Require(
            mode::TaikoInputBindings.size() == 4,
            "Taiko must expose one input binding for every playfield key.");

        mode::TaikoMode taiko;
        for (std::size_t index = 0;
             index < mode::TaikoInputBindings.size();
             ++index)
        {
            const mode::TaikoInputBinding& binding =
                mode::TaikoInputBindings[index];
            const std::array physicalKeys{
                binding.primaryKey,
                binding.secondaryKeys[0],
                binding.secondaryKeys[1]};
            Require(
                binding.primaryKey != 0 && binding.IsPrimary(binding.primaryKey),
                "Each configured layout must have a primary input key.");

            chart::PatternDocument pattern;
            pattern.mode = "Taiko";
            pattern.baseBpm = 120.0;
            pattern.notes.push_back(chart::PatternNote{
                .position = {0, chart::Rational{0, 1}},
                .keyType = static_cast<int>(
                    binding.action == mode::TaikoAction::Don
                        ? mode::TaikoNoteType::Don
                        : mode::TaikoNoteType::Kat),
                .actionType = static_cast<int>(
                    mode::TaikoPatternAction::Down)});
            mode::ModeLoadResult loaded = taiko.CreateSession(pattern);
            Require(
                loaded.Succeeded(),
                "Taiko input bindings require a playable session.");
            for (const rhythm::PhysicalKey physicalKey : physicalKeys)
            {
                if (physicalKey == 0)
                {
                    continue; // Unassigned secondary slot in a customized layout.
                }
                loaded.session->Reset();
                Require(
                    HasEvent(
                        loaded.session->ProcessInput(
                            physicalKey,
                            rhythm::InputEdge::Pressed,
                            rhythm::RhythmTime::zero()),
                        rhythm::NoteEventType::HitAccepted),
                    "Every Taiko primary and secondary key must trigger its mapped action.");
            }
        }
    }

    void TestRationalNumberUsesExactOrderingAndArithmetic()
    {
        Require(
            chart::RationalNumber{3, 8} < chart::RationalNumber{1, 2} &&
            chart::RationalNumber{4, 8} == chart::RationalNumber{1, 2} &&
            chart::RationalNumber{1, 6} + chart::RationalNumber{1, 3} ==
                chart::RationalNumber{1, 2} &&
            chart::RationalNumber{-1, 2} < chart::RationalNumber{-3, 8},
            "RationalNumber must compare and add reduced fractions exactly.");

        bool rejectedZeroDenominator = false;
        try
        {
            static_cast<void>(chart::RationalNumber{1, 0});
        }
        catch (const std::invalid_argument&)
        {
            rejectedZeroDenominator = true;
        }
        Require(
            rejectedZeroDenominator,
            "RationalNumber must reject a zero denominator.");
    }

    [[nodiscard]] chart::ParseResult<chart::PatternDocument>
    ParseTimingBody(const std::string_view timingBody)
    {
        const std::string pattern =
            "[Metadata]\nBase BPM: 120\n"
            "[Time Signature]\n" + std::string(timingBody) +
            "\n[Pattern]\n0/1,1,0\n";
        return chart::ChartParser{}.ParsePattern(pattern, "memory.ymp");
    }

    void TestTimingCommandWhitespaceGrammar()
    {
        Require(
            ParseTimingBody("1/2, #bpm 150").Succeeded() &&
            ParseTimingBody("1/2,        #bpm   150").Succeeded() &&
            ParseTimingBody("#measure 4/4").Succeeded() &&
            ParseTimingBody("#measure C").Succeeded(),
            "Timing grammar must allow whitespace after commas and between a command and its value.");

        Require(
            !ParseTimingBody("1/ 2, #bpm 150").Succeeded() &&
            !ParseTimingBody("1/2, #bpm150").Succeeded() &&
            !ParseTimingBody("#measure4/4").Succeeded() &&
            !ParseTimingBody("# measure 4/4").Succeeded() &&
            !ParseTimingBody("#measure 4/  4").Succeeded(),
            "Timing grammar must reject whitespace inside fractions and commands without a separating space.");
    }

    void TestAbsoluteMeasurePositionsAndTempoAnchors()
    {
        constexpr std::string_view Pattern = R"(
[Metadata]
Base BPM: 140
[Time Signature]
#measure 3/4
1/2,        #bpm   150
--
#measure 4/4
0/1, #bpm 160
[Pattern]
1/2,1,0
--
0/1,2,0
1/4,1,0
)";
        const auto parsed = chart::ChartParser{}.ParsePattern(
            Pattern,
            "tempo-example.ymp");
        Require(
            parsed.Succeeded() && parsed.document.notes.size() == 3,
            "The 3/4 to 4/4 tempo example must parse without errors.");

        const chart::MusicalTimeline timeline(parsed.document);
        const auto compiled = timeline.CompileNotes(parsed.document);
        Require(
            compiled[0].timing == rhythm::RhythmTime{857'143} &&
            compiled[1].timing == rhythm::RhythmTime{1'257'143} &&
            compiled[2].timing == rhythm::RhythmTime{1'632'143},
            "N/D must be an absolute whole-note position inside its measure and BPM anchors must accumulate from the previous anchor.");
    }

    void TestYmpSystemBreaksAdvanceAndRemainAvailable()
    {
        constexpr std::string_view Pattern = R"(
[Metadata]
Base BPM: 120
[Time Signature]
---
#measure 3/4
[Pattern]
0/4,1,0
---
0/4,2,0
--
0/4,1,0
---
0/4,2,0
)";
        const auto parsed = chart::ChartParser{}.ParsePattern(
            Pattern,
            "system-breaks.ymp");
        Require(
            parsed.Succeeded() && parsed.document.notes.size() == 4 &&
            parsed.document.notes[0].position.measure == 0 &&
            parsed.document.notes[1].position.measure == 1 &&
            parsed.document.notes[2].position.measure == 2 &&
            parsed.document.notes[3].position.measure == 3,
            "Both YMP measure separators must advance the current measure.");
        Require(
            parsed.document.systemBreakMeasures ==
                std::vector<std::int64_t>{1, 3},
            "The parser must preserve each unique measure that starts after "
            "a triple-dash system break.");
    }

    void TestOutOfMeasureEntriesAreIgnored()
    {
        constexpr std::string_view Pattern = R"(
[Metadata]
Base BPM: 120
[Time Signature]
#measure 2/4
3/4, #bpm 180
[Pattern]
1/4,1,0
3/4,2,0
)";
        const auto parsed = chart::ChartParser{}.ParsePattern(
            Pattern,
            "out-of-measure.ymp");
        Require(
            parsed.Succeeded() && parsed.document.notes.size() == 1 &&
            std::ranges::none_of(
                parsed.document.timing,
                [](const chart::TimingDirective& directive)
                {
                    return directive.type == chart::TimingDirectiveType::Bpm;
                }) &&
            std::ranges::count_if(
                parsed.diagnostics,
                [](const chart::Diagnostic& diagnostic)
                {
                    return diagnostic.severity ==
                        chart::DiagnosticSeverity::Warning;
                }) == 2,
            "Notes and timing directives at or beyond the current measure length must be ignored with warnings.");
    }

    void TestSongCatalog(const std::filesystem::path& songsRoot)
    {
        const chart::SongCatalogLoadResult catalog =
            chart::SongCatalog{}.Load(songsRoot);
        if (!catalog.Succeeded())
        {
            for (const chart::Diagnostic& diagnostic : catalog.diagnostics)
            {
                if (diagnostic.severity == chart::DiagnosticSeverity::Error)
                {
                    std::cerr << diagnostic.location.file.string() << ':'
                              << diagnostic.location.line << ": "
                              << diagnostic.message << '\n';
                }
            }
        }
        Require(catalog.Succeeded(),
            "The copied RPG song catalog must parse without errors.");
        Require(
            catalog.discoveredMusicFiles >= 1 &&
            catalog.songs.size() == catalog.discoveredMusicFiles,
            "Every discovered YMM music entry must appear in SONG LIST.");
        Require(
            catalog.discoveredPatternFiles >= 3 &&
            catalog.PatternCount() == catalog.discoveredPatternFiles,
            "All bundled and optional local YMP patterns must be associated with their songs.");
        mode::TaikoMode taiko;
        bool checkedAngelDream = false;
        const auto analysis = editor::AnalyzeAudio(catalog.songs.front().audioPath, {});
        Require(analysis.durationSeconds > 1 && analysis.frames.size() > 100 &&
            std::ranges::any_of(analysis.frames, [](const auto& f) { return f.peak > 0.01F; }),
            "The shipped MP3 must decode into real waveform/spectrum data.");
        const auto soundsRoot = songsRoot.parent_path() / "Skins/Default Skin/HitSounds/TaikoMode";
        if (std::filesystem::is_directory(soundsRoot))
            for (const auto* name : {"don.wav", "kat.wav", "bigdon.wav", "bigkat.wav"})
            {
                const auto hit = editor::AnalyzeAudio(soundsRoot / name, {});
                Require(!hit.frames.empty() && std::ranges::any_of(hit.frames, [](const auto& frame) {
                    return std::ranges::any_of(frame.bands, [](float value) { return value > .1F; }); }),
                    "Every default hit sound must produce a non-empty real FFT spectrum.");
            }
        for (const chart::SongCatalogEntry& song : catalog.songs)
        {
            Require(std::filesystem::is_regular_file(song.audioPath),
                "Every YMM entry must resolve its music file.");
            if (song.metadataPath.filename() ==
                "angel dream hand shaking.ymm")
            {
                constexpr std::array RequiredPatterns{
                    "angeldream [measure test].ymp",
                    "angeldream [test].ymp",
                    "angeldream.ymp"};
                for (const std::string_view required : RequiredPatterns)
                {
                    Require(
                        std::ranges::any_of(
                            song.patterns,
                            [required](const chart::SongCatalogPattern& pattern)
                            {
                                return pattern.patternPath.filename() == required;
                            }),
                        "The bundled AngelDream catalog must include every test pattern.");
                }
                checkedAngelDream = true;
            }
            for (const chart::SongCatalogPattern& pattern : song.patterns)
            {
                TemporaryDirectory saved("finger-drum-catalog-save");
                auto copy = pattern.pattern;
                copy.sourcePath = saved.Path() / pattern.patternPath.filename();
                chart::EffectDocument effect;
                if (pattern.effectPath) effect = chart::ChartParser{}.ParseEffectFile(*pattern.effectPath).document;
                chart::ChartEditor editing(copy, effect);
                editing.Save();
                const auto reloaded = chart::ChartParser{}.ParsePatternFile(copy.sourcePath);
                Require(reloaded.Succeeded() &&
                    chart::ChartEditor::WritePattern(reloaded.document) == chart::ChartEditor::WritePattern(copy),
                    "Every provided chart must save/reload without source-data loss (using a temporary copy).");
                if (pattern.patternPath.filename().string() ==
                    "Rapbit - Saika [test].ymp")
                {
                    const chart::MusicalTimeline timeline(pattern.pattern);
                    const rhythm::RhythmTime firstChange = timeline.Compile(
                        {1, chart::Rational{1, 8}});
                    const rhythm::RhythmTime secondChange = timeline.Compile(
                        {1, chart::Rational{2, 8}});
                    const rhythm::RhythmTime finalChange = timeline.Compile(
                        {1, chart::Rational{18, 8}});
                    const rhythm::RhythmTime nextBarline = timeline.Compile(
                        {2, chart::Rational{0, 1}});
                    Require(
                        secondChange - firstChange ==
                            rhythm::RhythmDuration{166'667} &&
                        finalChange < nextBarline,
                        "Saika's 20/8 measure must keep every BPM anchor at its absolute N/D position.");
                }
                mode::ModeLoadResult loaded = taiko.LoadSession(
                    pattern.patternPath,
                    pattern.effectPath);
                Require(
                    loaded.Succeeded() &&
                    loaded.session->Gear().LaneCount() == 1,
                    "Every copied Taiko pattern must create one playable Lane.");
            }
        }
        Require(
            checkedAngelDream,
            "The catalog test must exercise the bundled AngelDream song.");
        std::cout << "Catalog verified: " << catalog.songs.size()
                  << " songs, " << catalog.PatternCount()
                  << " patterns.\n";
    }

    void TestSongCatalogIsolatesInvalidFiles()
    {
        TemporaryDirectory fixture("FingerDrumCatalogValidation");
        const std::filesystem::path music = fixture.Path() / "Music";
        const std::filesystem::path patterns = fixture.Path() / "Pattern";
        WriteTextFile(
            music / "valid.ymm",
            "Version: 1\nFile: valid.mp3\nMusic Name 1: Valid\n");
        WriteTextFile(music / "valid.mp3", "audio-placeholder");
        WriteTextFile(
            music / "invalid.ymm",
            "Version: nope\nMusic Name 1: Invalid\n");
        WriteTextFile(
            patterns / "valid.ymp",
            "Version: 1\nMusic metadata: Music/valid.ymm\n"
            "Pattern Name: Valid\nMode: Taiko\nBase BPM: 120\n"
            "[Pattern]\n0/1,1,0\n");
        WriteTextFile(
            patterns / "invalid.ymp",
            "Version: nope\nMusic metadata: Music/valid.ymm\n"
            "Pattern Name: Invalid\nMode: Taiko\nBase BPM: 120\n"
            "[Pattern]\n0/1,1,0\n");

        const chart::SongCatalogLoadResult catalog =
            chart::SongCatalog{}.Load(fixture.Path());
        Require(
            !catalog.Succeeded() && catalog.discoveredMusicFiles == 2 &&
                catalog.discoveredPatternFiles == 2,
            "Catalog discovery must retain diagnostics and source counts for invalid files.");
        Require(
            catalog.songs.size() == 1 && catalog.PatternCount() == 1,
            "Invalid YMM/YMP files must be isolated from otherwise playable catalog entries "
            "(songs=" + std::to_string(catalog.songs.size()) +
            ", patterns=" + std::to_string(catalog.PatternCount()) + ").");
    }

    void TestInvalidNumericFieldsReportDiagnostics()
    {
        chart::ChartParser parser;
        const auto pattern = parser.ParsePattern(
            "Version: nope\nBase BPM: 120\nJudgeLevel: 0\n",
            "invalid.ymp");
        Require(
            !pattern.Succeeded() && pattern.diagnostics.size() >= 2,
            "Invalid YMP integer fields must produce parse diagnostics.");

        const auto effects = parser.ParseEffect(
            "Version: nope\n[AudioAutomation]\n"
            "0/1,#BusVolume,HitSound,invalid,0.5,-1,Linear\n",
            "invalid.yme");
        Require(
            !effects.Succeeded() && effects.document.commands.empty(),
            "Invalid YME numeric fields must be diagnosed and excluded.");
    }

    void TestLegacyParsingAndMicroseconds()
    {
        constexpr std::string_view Pattern = R"(
[Metadata]
Pattern Name: Unit Test
Base BPM: 120
[Difficulty]
Mode: Taiko
JudgeLevel: 50
[Time Signature]
0/1,#bpm 120
#measure 1/2
#measureLineVisible OFF
[Pattern]
1/4,1,0
--
0/1,2,0
)";
        chart::ChartParser parser;
        const chart::ParseResult<chart::PatternDocument> parsed =
            parser.ParsePattern(Pattern, "memory.ymp");
        Require(parsed.Succeeded() && parsed.document.notes.size() == 2,
            "Legacy YMP notes and measure separators must parse.");
        chart::MusicalTimeline timeline(parsed.document);
        const auto compiled = timeline.CompileNotes(parsed.document);
        Require(
            compiled[0].timing == rhythm::RhythmTime{500'000} &&
            compiled[1].timing == rhythm::RhythmTime{1'000'000},
            "Legacy #measure must preserve absolute in-measure positions and exact microsecond timestamps.");
        Require(
            std::ranges::any_of(
                parsed.diagnostics,
                [](const chart::Diagnostic& diagnostic)
                {
                    return diagnostic.severity ==
                        chart::DiagnosticSeverity::Warning;
                }),
            "Legacy visual directives must remain loadable and report migration warnings.");

        chart::MusicalPosition extendedPosition;
        Require(
            chart::TryParseMusicalPosition(
                "18/8", 3, extendedPosition) &&
            extendedPosition.fraction == chart::Rational{18, 8},
            "Legacy extended measures must accept positions beyond one whole.");

        constexpr std::string_view Effects = R"(
Version: 1
[AudioAutomation]
0/1,#BusVolume,HitSound,1.0,0.5,500,Linear
1/2,#ReverbSend,HitSound,0.0,0.8,1000,Smoothstep
)";
        const chart::ParseResult<chart::EffectDocument> effects =
            parser.ParseEffect(Effects, "memory.yme");
        Require(
            effects.Succeeded() && effects.document.commands.size() == 2 &&
            effects.document.commands[1].type ==
                chart::EffectCommandType::ReverbSend,
            "YME must parse real-time audio automation independently of YMP.");
    }
}

namespace
{
    void TestIndexedHitSoundChanges()
    {
        using namespace finger_drum;
        chart::ChartParser parser;
        const auto parsed = parser.ParsePattern(R"(
Base BPM: 120
[HitSounds]
1: Sounds/pop.wav
2: Sounds/kat.wav
[Time Signature]
--
#measure 3/4
1/4, #bpm 180
)", "Songs/Pattern/test.ymp");
        Require(parsed.Succeeded(), "Indexed hit sound table must parse.");
        const auto effects = parser.ParseEffect(R"(
Version: 1
[HitSound Changes]
4, 2/4, 1, 2
5, 0/4, 2, 1
5, 0/4, 2, 2
)", "Songs/Pattern/test.yme");
        Require(effects.Succeeded() && effects.document.hitSoundChanges.size() == 3 &&
            effects.document.hitSoundChanges.front().position.measure == 3,
            "Explicit YME measures must be one-based, with independently targeted key IDs.");
        mode::TaikoMode mode;
        auto loaded = mode.CreateSession(parsed.document, effects.document);
        Require(loaded.Succeeded(), "Valid indexed changes must load.");
        Require(loaded.session->HitSoundFiles().at("Chart.HitSound.1") ==
            std::filesystem::path("Songs/Pattern/Sounds/pop.wav"),
            "Hit sound paths must be relative to the YMP, not the working directory.");
        chart::MusicalTimeline timeline(parsed.document);
        const auto first = timeline.Compile({3, {2, 4}});
        const auto second = timeline.Compile({4, {0, 4}});
        auto sound = [&](rhythm::PhysicalKey key, rhythm::RhythmTime time)
        {
            const auto result = loaded.session->ProcessInput(
                key, rhythm::InputEdge::Pressed, time);
            Require(result.audioCues.size() == 1, "One input must produce one cue.");
            return result.audioCues.front().sound;
        };
        Require(sound('D', first - rhythm::RhythmDuration{1}) == "Taiko.Kat.FreeInput",
            "Change must not apply a microsecond early.");
        Require(sound('D', first) == "Chart.HitSound.1" &&
            sound('F', first) == "Taiko.Don.FreeInput",
            "Kat-only change must apply at the exact compiled BPM/measure boundary.");
        Require(sound('D', second) == "Chart.HitSound.2" &&
            sound('F', second) == "Chart.HitSound.2",
            "Two changes at one position must change both actions independently.");
        Require(sound('D', second + rhythm::RhythmDuration{10'000'000}) == "Chart.HitSound.2",
            "The last change must persist.");
        loaded.session->Reset();
        Require(sound('D', first - rhythm::RhythmDuration{1}) == "Taiko.Kat.FreeInput",
            "Reset/backward seeking must restore earlier defaults without stale state.");

        for (const std::string row : {"0, 2/4, 1, 2", "4, 2/ 4, 1, 2",
            "4, 2/4, 1, 3", "4, 2/4, 1", "4, 2/4, , 2",
            "4, 2/0, 1, 2", "4, -1/4, 1, 2"})
        {
            Require(!parser.ParseEffect("[HitSound Changes]\n" + row).Succeeded(),
                "Malformed indexed hit sound rows must be rejected: " + row);
        }
        const auto unknown = parser.ParseEffect("[HitSound Changes]\n4, 2/4, 999, 2");
        Require(!mode.CreateSession(parsed.document, unknown.document).Succeeded(),
            "Undefined table references must fail chart loading.");
        const auto outside = parser.ParseEffect("[HitSound Changes]\n4, 3/4, 1, 2");
        Require(!mode.CreateSession(parsed.document, outside.document).Succeeded(),
            "A change at or outside the measure end must be rejected.");
        Require(!parser.ParsePattern("[HitSounds]\n1: a.wav\n1: b.wav").Succeeded(),
            "Duplicate table entries must not be silently discarded.");

        // Explicit note assignments override timed Don/Kat defaults.
        auto pattern = parsed.document;
        pattern.notes.push_back({{3, {2, 4}}, 2, 0, "2"});
        auto explicitNote = mode.CreateSession(pattern, effects.document);
        const auto noteResult = explicitNote.session->ProcessInput(
            'D', rhythm::InputEdge::Pressed, first);
        Require(noteResult.audioCues.size() == 1 &&
            noteResult.audioCues.front().sound == "Chart.HitSound.2",
            "Explicit per-note hit sound must take precedence.");

        // Both big note first-hit sounds and Kat roll ticks follow Kat changes.
        for (int type : {4, 11, 12, 13, 14})
        {
            pattern = parsed.document;
            pattern.notes.push_back({{3, {2, 4}}, type, type == 4 ? 0 : 1});
            if (type != 4) pattern.notes.push_back({{4, {1, 4}}, type, 2});
            auto special = mode.CreateSession(pattern, effects.document);
            Require(special.Succeeded(), "Special-note sound test must load.");
            const auto hit = special.session->ProcessInput('D', rhythm::InputEdge::Pressed, first);
            Require(hit.audioCues.size() == 1 && hit.audioCues.front().sound == "Chart.HitSound.1",
                "BigKat and Kat roll inputs must use the current Kat sound.");
        }
    }

    void TestSpecialNoteSoundOverridePriority()
    {
        for (const int type : {15, 16, 17})
        {
            chart::PatternDocument pattern;
            pattern.hitSounds["1"] = "explicit.wav";
            pattern.hitSounds["2"] = "timed.wav";
            pattern.notes.push_back({{0,{}}, type, 1, "1", {"HitCount=8", "Action=Don", "TickDivision=16"}});
            pattern.notes.push_back({{0,{1,2}}, type, 2});
            chart::EffectDocument effects;
            effects.hitSoundChanges.push_back({{0,{}}, "2", 1});
            auto loaded = mode::TaikoMode{}.CreateSession(pattern, effects);
            Require(loaded.Succeeded(), "Special sound priority chart must load.");
            const auto head = loaded.session->ProcessInput('F', rhythm::InputEdge::Pressed, rhythm::RhythmTime{});
            Require(head.audioCues.size() == 1 && head.audioCues.front().sound == "Chart.HitSound.1",
                "Balloon, DengDeng and Buzz explicit sounds must override timed Don defaults.");
            if (type == 17)
            {
                const std::array<rhythm::PhysicalKey,1> held{'F'};
                const auto tick = loaded.session->Update(rhythm::RhythmTime{125'000}, held);
                Require(tick.audioCues.size() == 1 && tick.audioCues.front().sound == "Chart.HitSound.1",
                    "Buzz body ticks must retain the explicit sound assignment.");
            }
        }
    }

    void TestChartEditingAndSave()
    {
        using namespace finger_drum;
        chart::PatternDocument p;
        p.baseBpm = 120; p.musicMetadataFile = "../Music/test.ymm";
        p.makers = {"테스트"}; p.tags = {"editor"};
        chart::ChartEditor editor(p);
        editor.AddNote({0,{1,4}},1);
        editor.AddNote({0,{3,4}},11,chart::MusicalPosition{1,{3,4}}, {"8"});
        editor.SetMeasureLength(0,{2,4});
        Require(editor.Pattern().notes[0].position == chart::MusicalPosition{0,{1,4}} &&
            editor.Pattern().notes[1].position == chart::MusicalPosition{1,{1,4}},
            "Only overflowing note positions must carry into later measures with exact remainders.");
        chart::ChartEditor collapsed(p);
        collapsed.AddNote({0,{3,4}},11,chart::MusicalPosition{1,{1,4}});
        bool rejected = false;
        try { collapsed.SetMeasureLength(0,{2,4}); }
        catch (const std::invalid_argument&) { rejected = true; }
        Require(rejected && collapsed.Timeline().MeasureLength(0) == chart::Rational{1,1} &&
            collapsed.Pattern().notes.front().position == chart::MusicalPosition{0,{3,4}},
            "A collapsed long-note pair must reject the whole signature edit without altering the source.");
        editor.DeleteNote(1);
        Require(editor.Pattern().notes.size() == 1, "Deleting a long-note head must also remove its matching tail.");
        const auto& timeline = editor.Timeline();
        for (const chart::Rational beat : {chart::Rational{1,3},chart::Rational{1,1},chart::Rational{1000,3}})
            Require(timeline.PositionToWholeNotes(timeline.PositionAtWholeNotes(beat)) == beat,
                "Prefix sum inverse must remain exact, including extrapolated measures.");

        auto pattern = editor.Pattern();
        pattern.hitSounds["1"] = "Sounds/pop.wav";
        pattern.timing.push_back({{0,{1,4}},chart::TimingDirectiveType::Bpm,240});
        chart::EffectDocument effects;
        chart::EffectCommand speed;
        speed.type = chart::EffectCommandType::ScrollSpeed;
        speed.beginValue = 1; speed.endValue = 2; speed.curve = chart::AutomationCurve::Linear;
        speed.endPosition = chart::MusicalPosition{1,{0,1}};
        effects.commands.push_back(speed);
        effects.hitSoundChanges.push_back({{1,{1,4}},"1",2});
        editor.Replace(pattern,effects);
        const auto revision = editor.Revision();
        editor.Replace(pattern,effects);
        Require(editor.Revision() == revision + 1, "Edits must invalidate dependent editor caches exactly once.");
        Require(std::abs(editor.Notes()[0].scrollMultiplier - 1.5) < 1e-9,
            "Region speed must interpolate by rational beat, not by elapsed seconds across a BPM change.");
        auto serialized = chart::ChartEditor::WriteEffects(effects);
        const auto parsedEffects = chart::ChartParser{}.ParseEffect(serialized);
        Require(parsedEffects.Succeeded() && parsedEffects.document.commands[0].endPosition == speed.endPosition &&
            parsedEffects.document.hitSoundChanges.size() == 1, "YME regions and indexed hit sound changes must round-trip.");

        TemporaryDirectory directory("finger-drum-editor");
        pattern.sourcePath = directory.Path() / "edited.ymp";
        editor.Replace(pattern,effects); editor.Save();
        auto yme = pattern.sourcePath; yme.replace_extension(".yme");
        Require(std::filesystem::exists(yme) && !editor.Dirty(), "Save must produce adjacent YMP/YME and clear dirty state.");
        const auto parsed = chart::ChartParser{}.ParsePatternFile(pattern.sourcePath);
        Require(parsed.Succeeded() && parsed.document.notes.size() == pattern.notes.size() &&
            parsed.document.makers == pattern.makers && parsed.document.hitSounds == pattern.hitSounds &&
            chart::MusicalTimeline(parsed.document).CompileNotes(parsed.document)[0].timing == editor.Notes()[0].timing,
            "Saving must preserve Unicode metadata, hit sound table and exact compiled note timing.");
        editor.Save();
        Require(!std::filesystem::exists(pattern.sourcePath.string()+".editor.bak"), "Successful save must clean its own backup.");
    }
}

int main(const int argumentCount, char* arguments[])
{
    try
    {
        TestJudgementScalingAndInterpolation();
        TestRhythmTimerClockMapping();
        TestLaneFocusRules();
        TestLargeNoteSoundState();
        TestHoldTicksAreExactlyOnce();
        TestLongNoteRemainsInScrollSnapshotUntilItsTail();
        TestOverlappingLongTrailIsNotHiddenByShorterNote();
        TestTaikoRollAndTickRollRules();
        TestBalloonDengDengAndBuzzRules();
        TestTimingAccuracyAndSessionAggregation();
        TestPurpleNoteBothOrders();
        TestCountedLongNoteAccuracyAndSyntax();
        TestTickAndHoldAccuracy();
        TestMusicalSubdivisionTicksFollowTempo();
        TestLongNoteKeepsFirstHead();
        TestTaikoUsesOneLaneForEveryNoteType();
        TestTaikoInputBindings();
        TestIndexedHitSoundChanges();
        TestSpecialNoteSoundOverridePriority();
        TestChartEditingAndSave();
        TestRationalNumberUsesExactOrderingAndArithmetic();
        TestTimingCommandWhitespaceGrammar();
        TestAbsoluteMeasurePositionsAndTempoAnchors();
        TestYmpSystemBreaksAdvanceAndRemainAvailable();
        TestOutOfMeasureEntriesAreIgnored();
        TestLegacyParsingAndMicroseconds();
        TestInvalidNumericFieldsReportDiagnostics();
        TestSongCatalogIsolatesInvalidFiles();
        if (argumentCount == 3 &&
            std::string_view(arguments[1]) == "--catalog-root")
        {
            TestSongCatalog(std::filesystem::path(arguments[2]));
        }
        std::cout << "FingerDrum rhythm tests passed.\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "FingerDrum rhythm test failure: "
                  << exception.what() << '\n';
        return EXIT_FAILURE;
    }
}
