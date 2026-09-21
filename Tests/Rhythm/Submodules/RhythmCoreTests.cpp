#include "TestSupport.h"

namespace finger_drum::tests
{
    void TestJudgementScalingAndInterpolation()
    {
        rhythm::JudgementProfile profile("default", 50);
        Require(profile.HalfWindow(rhythm::JudgementGrade::Max).count() == 4'500,
                "Level 50 Max window must be 4.5 ms.");
        profile.SetLevel(100);
        Require(profile.HalfWindow(rhythm::JudgementGrade::Max).count() == 2'250 &&
                    profile.HalfWindow(rhythm::JudgementGrade::Perfect).count() == 4'750,
                "Level 100 windows must be exactly half of level 50.");

        const std::array<rhythm::JudgementBand, rhythm::JudgementProfile::BandCount> bands{{
            {rhythm::JudgementGrade::Max, rhythm::RhythmDuration{5'000}, 1.0},
            {rhythm::JudgementGrade::Perfect, rhythm::RhythmDuration{9'000}, 0.9},
            {rhythm::JudgementGrade::Great, rhythm::RhythmDuration{20'000}, 0.8},
            {rhythm::JudgementGrade::Good, rhythm::RhythmDuration{50'000}, 0.5},
            {rhythm::JudgementGrade::Bad, rhythm::RhythmDuration{90'000}, 0.0},
        }};
        rhythm::JudgementProfile interpolation("linear", 50, bands);
        const rhythm::JudgementResult result =
            interpolation.Evaluate(rhythm::RhythmTime::zero(), rhythm::RhythmTime{7'000});
        Require(result.grade == rhythm::JudgementGrade::Perfect &&
                    std::abs(result.scoreRate - 0.95) < 1e-9,
                "A 7 ms hit between 5/9 ms anchors must score 95 percent.");
    }

    void TestRhythmTimerClockMapping()
    {
        rhythm::RhythmTimer timer;
        timer.Start(1'000, 1'000, rhythm::RhythmTime{250'000});
        Require(timer.Now(1'500) == rhythm::RhythmTime{750'000},
                "QPC mapping must advance the single rhythm timeline.");
        timer.AnchorDspClock(rhythm::RhythmTime{750'000}, 48'000, 48'000);
        Require(timer.ToDspClock(rhythm::RhythmTime{1'750'000}) == 96'000,
                "One second of rhythm time must map to one mixer sample-rate span.");
    }

    void TestLaneFocusRules()
    {
        auto profile = std::make_shared<rhythm::JudgementProfile>();
        rhythm::Lane lane;
        lane.AddNote(MakeTap(1, rhythm::RhythmTime::zero(), profile));
        lane.AddNote(MakeTap(2, rhythm::RhythmTime{100'000}, profile));
        lane.Finalize();

        const rhythm::NoteProcessResult early =
            lane.ProcessInput({rhythm::RhythmTime{-70'000}, 1, 'F', rhythm::InputEdge::Pressed});
        Require(lane.CurrentIndex() == 0 && HasEvent(early, rhythm::NoteEventType::InputRejected),
                "Early Bad must keep focus on the first note.");

        const rhythm::NoteProcessResult forwarded =
            lane.ProcessInput({rhythm::RhythmTime{60'000}, 1, 'F', rhythm::InputEdge::Pressed});
        Require(HasEvent(forwarded, rhythm::NoteEventType::Missed) &&
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
        pattern.notes.push_back(
            chart::PatternNote{.position = {0, chart::Rational{0, 1}},
                               .keyType = static_cast<int>(mode::TaikoNoteType::BigDon),
                               .actionType = static_cast<int>(mode::TaikoPatternAction::Down)});

        mode::TaikoMode taiko;
        mode::ModeLoadResult loaded = taiko.CreateSession(pattern);
        Require(loaded.Succeeded(), "Taiko mode must create the large-note session.");

        const rhythm::NoteProcessResult early = loaded.session->ProcessInput(
            'F', rhythm::InputEdge::Pressed, rhythm::RhythmTime{-60'000});
        Require(early.audioCues.size() == 1 &&
                    early.audioCues.front().sound == "Taiko.Don.FreeInput",
                "An out-of-Good large note must not play its dedicated sound.");

        const rhythm::NoteProcessResult first = loaded.session->ProcessInput(
            'F', rhythm::InputEdge::Pressed, rhythm::RhythmTime::zero());
        Require(first.audioCues.size() == 1 &&
                    first.audioCues.front().sound == "Taiko.BigDon.FirstHit",
                "The first Good large-note hit must emit exactly one large-note cue.");

        const rhythm::NoteProcessResult second = loaded.session->ProcessInput(
            'J', rhythm::InputEdge::Pressed, rhythm::RhythmTime{1'000});
        Require(second.audioCues.empty() && HasEvent(second, rhythm::NoteEventType::Completed),
                "The second large-note hit must complete without replaying the first-hit cue.");
    }

    void TestHoldTicksAreExactlyOnce()
    {
        auto profile = std::make_shared<rhythm::JudgementProfile>();
        auto sound = std::make_shared<rhythm::MappedNoteSoundPolicy>();
        sound->Bind(rhythm::SoundBinding{.eventType = rhythm::NoteEventType::TickAccepted,
                                         .cue = rhythm::AudioCueRequest{.sound = "tick"}});
        rhythm::RuleBasedNote note(7, rhythm::RhythmTime::zero(), profile,
                                   std::make_unique<rhythm::HoldInputRule>(
                                       1, rhythm::RhythmTime{30'000},
                                       std::vector<rhythm::RhythmTime>{rhythm::RhythmTime{10'000},
                                                                       rhythm::RhythmTime{20'000}}),
                                   sound);
        static_cast<void>(
            note.ProcessInput({rhythm::RhythmTime::zero(), 1, 'F', rhythm::InputEdge::Pressed}));
        const std::array<rhythm::NoteAction, 1> held{1};
        const rhythm::NoteProcessResult first = note.Update({rhythm::RhythmTime{15'000}, held});
        const rhythm::NoteProcessResult repeated = note.Update({rhythm::RhythmTime{15'000}, held});
        const rhythm::NoteProcessResult second = note.Update({rhythm::RhythmTime{25'000}, held});
        Require(first.audioCues.size() == 1 && repeated.audioCues.empty() &&
                    second.audioCues.size() == 1,
                "Each hold tick must emit one sound even across repeated updates.");
    }

    void TestLongNoteRemainsInScrollSnapshotUntilItsTail()
    {
        auto profile = std::make_shared<rhythm::JudgementProfile>();
        rhythm::ScrollGear gear;
        rhythm::Lane &lane = gear.CreateLane();
        lane.AddNote(std::make_unique<rhythm::RuleBasedNote>(
            1, rhythm::RhythmTime::zero(), profile,
            std::make_unique<rhythm::TimedSequenceInputRule>(std::vector<rhythm::NoteAction>{1}, 3,
                                                             rhythm::RhythmTime{1'000'000})));
        gear.Finalize();

        const rhythm::ScrollGearSnapshot active =
            gear.BuildSnapshot(rhythm::RhythmTime{500'000}, rhythm::RhythmDuration{2'000'000},
                               rhythm::RhythmDuration{220'000});
        Require(active.notes.size() == 1 &&
                    active.notes.front().expireTime == rhythm::RhythmTime{1'000'000},
                "A long note head must remain visible after the ordinary past "
                "window until its tail expires.");

        static_cast<void>(gear.Update(rhythm::RhythmTime{1'000'000}));
        const rhythm::ScrollGearSnapshot missedTrail =
            gear.BuildSnapshot(rhythm::RhythmTime{1'100'000}, rhythm::RhythmDuration{2'000'000},
                               rhythm::RhythmDuration{220'000});
        const rhythm::ScrollGearSnapshot expiredTrail =
            gear.BuildSnapshot(rhythm::RhythmTime{1'220'001}, rhythm::RhythmDuration{2'000'000},
                               rhythm::RhythmDuration{220'000});
        Require(missedTrail.notes.size() == 1 &&
                    missedTrail.notes.front().state == rhythm::NoteState::Missed &&
                    expiredTrail.notes.empty(),
                "A missed timed note must remain in the snapshot for its explicit "
                "post-expiry travel window only.");
    }

    void TestOverlappingLongTrailIsNotHiddenByShorterNote()
    {
        auto profile = std::make_shared<rhythm::JudgementProfile>();
        rhythm::ScrollGear gear;
        rhythm::Lane &lane = gear.CreateLane();
        lane.AddNote(std::make_unique<rhythm::RuleBasedNote>(
            1, rhythm::RhythmTime::zero(), profile,
            std::make_unique<rhythm::TimedSequenceInputRule>(std::vector<rhythm::NoteAction>{1}, 2,
                                                             rhythm::RhythmTime{1'000'000})));
        lane.AddNote(std::make_unique<rhythm::RuleBasedNote>(
            2, rhythm::RhythmTime{100'000}, profile,
            std::make_unique<rhythm::TimedSequenceInputRule>(std::vector<rhythm::NoteAction>{1}, 2,
                                                             rhythm::RhythmTime{100'000})));
        gear.Finalize();

        static_cast<void>(gear.Update(rhythm::RhythmTime{1'000'000}));
        const rhythm::ScrollGearSnapshot snapshot =
            gear.BuildSnapshot(rhythm::RhythmTime{1'100'000}, rhythm::RhythmDuration{2'000'000},
                               rhythm::RhythmDuration{220'000});
        Require(snapshot.notes.size() == 1 && snapshot.notes.front().noteId == 1,
                "An older long-note trail must remain visible when an intervening "
                "short note has already left the past window.");
    }
} // namespace finger_drum::tests
