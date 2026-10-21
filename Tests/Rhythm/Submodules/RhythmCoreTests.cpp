#include "Note/Submodules/HoldInputRule.h"
#include "Note/Submodules/TimedSequenceInputRule.h"
#include "TestSupport.h"

namespace tests
{
    void TestJudgementScalingAndInterpolation()
    {
        AccuracyRange profile("default", 50);
        Require(profile.HalfWindow(JudgementGrade::Max).count() == 4'500,
                "Level 50 Max window must be 4.5 ms.");
        profile.SetLevel(100);
        Require(profile.HalfWindow(JudgementGrade::Max).count() == 2'250 &&
                    profile.HalfWindow(JudgementGrade::Perfect).count() == 4'750,
                "Level 100 windows must be exactly half of level 50.");
        profile.SetLevel(50.25);
        Require(profile.Level() == 50.25 && profile.HalfWindow(JudgementGrade::Bad).count() == 89'552,
                "Fractional JudgeLevel must reach actual judgement window scaling without integer truncation.");

        const std::array<JudgementBand, AccuracyRange::BandCount> bands{{
            {JudgementGrade::Max, RhythmDuration{5'000}, 1.0},
            {JudgementGrade::Perfect, RhythmDuration{9'000}, 0.9},
            {JudgementGrade::Great, RhythmDuration{20'000}, 0.8},
            {JudgementGrade::Good, RhythmDuration{50'000}, 0.5},
            {JudgementGrade::Bad, RhythmDuration{90'000}, 0.0},
        }};
        AccuracyRange interpolation("linear", 50, bands);
        const JudgementResult result =
            interpolation.Evaluate(RhythmTime::zero(), RhythmTime{7'000});
        Require(result.grade == JudgementGrade::Perfect &&
                    std::abs(result.scoreRate - 0.95) < 1e-9,
                "A 7 ms hit between 5/9 ms anchors must score 95 percent.");
    }

    void TestRhythmTimerClockMapping()
    {
        RhythmTimer timer;
        timer.Start(1'000, 1'000, RhythmTime{250'000});
        Require(timer.Now(1'500) == RhythmTime{750'000},
                "QPC mapping must advance the single rhythm timeline.");
        timer.AnchorDspClock(RhythmTime{750'000}, 48'000, 48'000);
        Require(timer.ToDspClock(RhythmTime{1'750'000}) == 96'000,
                "One second of rhythm time must map to one mixer sample-rate span.");
    }

    void TestLaneFocusRules()
    {
        auto profile = std::make_shared<AccuracyRange>();
        Lane lane;
        lane.AddNote(MakeTap(1, RhythmTime::zero(), profile));
        lane.AddNote(MakeTap(2, RhythmTime{100'000}, profile));
        lane.Finalize();

        const NoteProcessResult early =
            lane.ProcessInput({RhythmTime{-70'000}, 1, 'F', InputEdge::Pressed});
        Require(lane.CurrentIndex() == 0 && HasEvent(early, NoteEventType::InputRejected),
                "Early Bad must keep focus on the first note.");

        const NoteProcessResult forwarded =
            lane.ProcessInput({RhythmTime{60'000}, 1, 'F', InputEdge::Pressed});
        Require(HasEvent(forwarded, NoteEventType::Missed) &&
                    HasEvent(forwarded, NoteEventType::HitAccepted) &&
                    lane.CurrentIndex() == 2,
                "Late Bad must miss the old note and apply the same Good input to the next note.");
    }

    void TestLargeNoteSoundState()
    {
        PatternDocument pattern;
        pattern.mode = "Taiko";
        pattern.baseBpm = 120.0;
        pattern.judgementLevel = 50.25;
        pattern.notes.push_back(
            PatternNote{.position = {0, Rational{0, 1}},
                               .keyType = static_cast<int>(TaikoNoteType::BigDon),
                               .actionType = static_cast<int>(TaikoPatternAction::Down)});

        TaikoMode taiko;
        ModeLoadResult loaded = taiko.CreateSession(pattern);
        Require(loaded.Succeeded(), "Taiko mode must create the large-note session.");

        const NoteProcessResult early = loaded.session->ProcessInput(
            'F', InputEdge::Pressed, RhythmTime{-60'000});
        Require(early.audioCues.size() == 1 &&
                    early.audioCues.front().sound == "Taiko.Don.FreeInput",
                "An out-of-Good large note must not play its dedicated sound.");

        const NoteProcessResult first = loaded.session->ProcessInput(
            'F', InputEdge::Pressed, RhythmTime::zero());
        Require(first.audioCues.size() == 1 &&
                    first.audioCues.front().sound == "Taiko.BigDon.FirstHit",
                "The first Good large-note hit must emit exactly one large-note cue.");

        const NoteProcessResult second = loaded.session->ProcessInput(
            'J', InputEdge::Pressed, RhythmTime{1'000});
        Require(second.audioCues.empty() && HasEvent(second, NoteEventType::Completed),
                "The second large-note hit must complete without replaying the first-hit cue.");
        const auto result = loaded.session->TakeResult(PlayEndReason::Completed, RhythmTime{1'000});
        Require(result && result->Summary().judgementLevel == 50.25,
                "Result snapshots must preserve fractional JudgeLevel.");
    }

    void TestHoldTicksAreExactlyOnce()
    {
        auto profile = std::make_shared<AccuracyRange>();
        auto sound = std::make_shared<MappedNoteSoundPolicy>();
        sound->Bind(SoundBinding{.eventType = NoteEventType::TickAccepted,
                                         .cue = AudioCueRequest{.sound = "tick"}});
        RuleBasedNote note(7, RhythmTime::zero(), profile,
                                   std::make_unique<HoldInputRule>(
                                       1, RhythmTime{30'000},
                                       std::vector<RhythmTime>{RhythmTime{10'000},
                                                                       RhythmTime{20'000}}),
                                   sound);
        static_cast<void>(
            note.ProcessInput({RhythmTime::zero(), 1, 'F', InputEdge::Pressed}));
        const std::array<NoteAction, 1> held{1};
        const NoteProcessResult first = note.Update({RhythmTime{15'000}, held});
        const NoteProcessResult repeated = note.Update({RhythmTime{15'000}, held});
        const NoteProcessResult second = note.Update({RhythmTime{25'000}, held});
        Require(first.audioCues.size() == 1 && repeated.audioCues.empty() &&
                    second.audioCues.size() == 1,
                "Each hold tick must emit one sound even across repeated updates.");
    }

    void TestLongNoteRemainsInScrollSnapshotUntilItsTail()
    {
        auto profile = std::make_shared<AccuracyRange>();
        ScrollGear gear;
        Lane &lane = gear.CreateLane();
        lane.AddNote(std::make_unique<RuleBasedNote>(
            1, RhythmTime::zero(), profile,
            std::make_unique<TimedSequenceInputRule>(std::vector<NoteAction>{1}, 3,
                                                             RhythmTime{1'000'000})));
        gear.Finalize();

        const ScrollGearSnapshot active =
            gear.BuildSnapshot(RhythmTime{500'000}, RhythmDuration{2'000'000},
                               RhythmDuration{220'000});
        Require(active.notes.size() == 1 &&
                    active.notes.front().expireTime == RhythmTime{1'000'000},
                "A long note head must remain visible after the ordinary past "
                "window until its tail expires.");

        static_cast<void>(gear.Update(RhythmTime{1'000'000}));
        const ScrollGearSnapshot missedTrail =
            gear.BuildSnapshot(RhythmTime{1'100'000}, RhythmDuration{2'000'000},
                               RhythmDuration{220'000});
        const ScrollGearSnapshot expiredTrail =
            gear.BuildSnapshot(RhythmTime{1'220'001}, RhythmDuration{2'000'000},
                               RhythmDuration{220'000});
        Require(missedTrail.notes.size() == 1 &&
                    missedTrail.notes.front().state == NoteState::Missed &&
                    expiredTrail.notes.empty(),
                "A missed timed note must remain in the snapshot for its explicit "
                "post-expiry travel window only.");
    }

    void TestOverlappingLongTrailIsNotHiddenByShorterNote()
    {
        auto profile = std::make_shared<AccuracyRange>();
        ScrollGear gear;
        Lane &lane = gear.CreateLane();
        lane.AddNote(std::make_unique<RuleBasedNote>(
            1, RhythmTime::zero(), profile,
            std::make_unique<TimedSequenceInputRule>(std::vector<NoteAction>{1}, 2,
                                                             RhythmTime{1'000'000})));
        lane.AddNote(std::make_unique<RuleBasedNote>(
            2, RhythmTime{100'000}, profile,
            std::make_unique<TimedSequenceInputRule>(std::vector<NoteAction>{1}, 2,
                                                             RhythmTime{100'000})));
        gear.Finalize();

        static_cast<void>(gear.Update(RhythmTime{1'000'000}));
        const ScrollGearSnapshot snapshot =
            gear.BuildSnapshot(RhythmTime{1'100'000}, RhythmDuration{2'000'000},
                               RhythmDuration{220'000});
        Require(snapshot.notes.size() == 1 && snapshot.notes.front().noteId == 1,
                "An older long-note trail must remain visible when an intervening "
                "short note has already left the past window.");
    }
} // namespace youmu::tests
