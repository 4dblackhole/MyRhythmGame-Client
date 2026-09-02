#include "Catalog/SongCatalog.h"
#include "Judgement/JudgementProfile.h"
#include "Lane/Lane.h"
#include "Mode/PlayGameMode.h"
#include "Note/Note.h"
#include "Parsing/ChartParser.h"
#include "Taiko/TaikoMode.h"
#include "Time/RhythmTimer.h"
#include "Timing/MusicalTimeline.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
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
            rhythm::RhythmTime::zero(), heldDon);
        const rhythm::NoteProcessResult releasedTick = donBuzz.session->Update(
            rhythm::RhythmTime{125'000});
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
        constexpr std::array<std::array<rhythm::PhysicalKey, 3>, 4>
            ExpectedBindings{{
                {{'E', 'D', 'C'}},
                {{'R', 'F', 'V'}},
                {{'U', 'J', 'M'}},
                {{'I', 'K', 0xBC}},
            }};
        Require(
            mode::TaikoInputBindings.size() == ExpectedBindings.size(),
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
                physicalKeys == ExpectedBindings[index],
                "Taiko input bindings must retain the primary and secondary keys.");

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
            catalog.discoveredMusicFiles == 5 &&
            catalog.songs.size() == 5,
            "All five YMM music entries must appear in SONG LIST.");
        Require(
            catalog.discoveredPatternFiles >= 8 &&
            catalog.PatternCount() == catalog.discoveredPatternFiles,
            "All bundled and optional local YMP patterns must be associated with their songs.");
        mode::TaikoMode taiko;
        bool checkedSaikaTempoSequence = false;
        for (const chart::SongCatalogEntry& song : catalog.songs)
        {
            Require(std::filesystem::is_regular_file(song.audioPath),
                "Every YMM entry must resolve its music file.");
            for (const chart::SongCatalogPattern& pattern : song.patterns)
            {
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
                    checkedSaikaTempoSequence = true;
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
            checkedSaikaTempoSequence,
            "The catalog test must exercise the Saika multi-BPM pattern.");
        std::cout << "Catalog verified: " << catalog.songs.size()
                  << " songs, " << catalog.PatternCount()
                  << " patterns.\n";
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
        TestTaikoRollAndTickRollRules();
        TestBalloonDengDengAndBuzzRules();
        TestMusicalSubdivisionTicksFollowTempo();
        TestLongNoteKeepsFirstHead();
        TestTaikoUsesOneLaneForEveryNoteType();
        TestTaikoInputBindings();
        TestRationalNumberUsesExactOrderingAndArithmetic();
        TestTimingCommandWhitespaceGrammar();
        TestAbsoluteMeasurePositionsAndTempoAnchors();
        TestOutOfMeasureEntriesAreIgnored();
        TestLegacyParsingAndMicroseconds();
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
