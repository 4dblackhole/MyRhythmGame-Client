#include "TestSupport.h"

namespace finger_drum::tests
{
    void TestTimingAccuracyAndSessionAggregation()
    {
        auto profile = std::make_shared<rhythm::JudgementProfile>();
        mode::PlaySession session;
        session.SetInputMapping({{'F', 1}, {'J', 1}});
        auto &lane = session.Gear().CreateLane();
        lane.AddNote(std::make_unique<rhythm::RuleBasedNote>(
            1, rhythm::RhythmTime{0}, profile,
            std::make_unique<rhythm::CountedHitInputRule>(1, 2)));
        lane.AddNote(MakeTap(2, rhythm::RhythmTime{200'000}, profile));
        session.Gear().Finalize();
        Require(!session.AccuracyRate(), "Accuracy must be unavailable before a final note.");
        const auto first =
            session.ProcessInput('F', rhythm::InputEdge::Pressed, rhythm::RhythmTime{0});
        Require(first.finalizedAccuracies.empty(), "Big first hit must not finalize accuracy.");
        const auto second =
            session.ProcessInput('J', rhythm::InputEdge::Pressed, rhythm::RhythmTime{16'500});
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
        Require(session.LastNoteAccuracy()->DebugText().find(L"Hit2=0.00%") != std::wstring::npos,
                "Debug details must expose unfilled hit components.");
#endif
    }

    void TestPurpleNoteBothOrders()
    {
        chart::PatternDocument pattern;
        pattern.baseBpm = 120.0;
        pattern.notes.push_back(
            chart::PatternNote{.position = {0, chart::Rational{0, 1}},
                               .keyType = 5,
                               .actionType = static_cast<int>(mode::TaikoPatternAction::Down)});
        for (const bool katFirst : {false, true})
        {
            auto loaded = mode::TaikoMode{}.CreateSession(pattern);
            Require(loaded.Succeeded() && loaded.session->FindNotePresentation(1)->visualKind ==
                                              finger_drum::mode::NoteVisualKind::Purple,
                    "ID 5 must create a purple large-note presentation.");
            const auto firstKey = katFirst ? 'D' : 'F';
            const auto secondKey = katFirst ? 'F' : 'D';
            const auto first =
                loaded.session->ProcessInput(firstKey, rhythm::InputEdge::Pressed, {});
            const auto repeat = loaded.session->ProcessInput(firstKey, rhythm::InputEdge::Pressed,
                                                             rhythm::RhythmTime{1'000});
            const auto second = loaded.session->ProcessInput(secondKey, rhythm::InputEdge::Pressed,
                                                             rhythm::RhythmTime{2'000});
            Require(first.audioCues.size() == 1 &&
                        first.audioCues.front().sound ==
                            (katFirst ? "Taiko.Kat.Hit" : "Taiko.Don.Hit") &&
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
                const std::string pattern =
                    "[Metadata]\nBase BPM: 120\n"
                    "[Difficulty]\nMode: Taiko\n[Time Signature]\n[Pattern]\n"
                    "0/1," +
                    std::to_string(static_cast<int>(type)) + ",1,," + count + "\n1/2," +
                    std::to_string(static_cast<int>(type)) + ",2\n";
                const auto parsed = chart::ChartParser{}.ParsePattern(pattern, "count.ymp");
                auto loaded = mode::TaikoMode{}.CreateSession(parsed.document);
                Require(parsed.Succeeded() && loaded.Succeeded(),
                        "All counted long notes must parse bare and named ExtraData counts.");
                for (int hit = 0; hit < 8; ++hit)
                {
                    const auto key =
                        type == mode::TaikoNoteType::DengDeng && hit % 2 != 0 ? 'D' : 'F';
                    static_cast<void>(loaded.session->ProcessInput(
                        key, rhythm::InputEdge::Pressed, rhythm::RhythmTime{hit * 10'000}));
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
            Require(loaded.Succeeded() && loaded.session->Gear()
                                                  .Lanes()
                                                  .front()
                                                  ->Notes()
                                                  .front()
                                                  ->Accuracy()
                                                  .target.hits == 2,
                    "All counted defaults must use exact ceil(1/7 * 12) = 2.");
            const auto invalid = mode::TaikoMode{}.CreateSession(MakeLongPattern(type, {"0"}));
            Require(!invalid.Succeeded(), "A nonpositive explicit target must be rejected.");
        }
        auto roll =
            mode::TaikoMode{}.CreateSession(MakeLongPattern(mode::TaikoNoteType::Roll, {"2"}));
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
        auto tickRoll =
            mode::TaikoMode{}.CreateSession(MakeLongPattern(mode::TaikoNoteType::TickRoll));
        const auto early = tickRoll.session->ProcessInput('F', rhythm::InputEdge::Pressed,
                                                          rhythm::RhythmTime{-70'000});
        Require(std::ranges::none_of(early.events,
                                     [](const auto &event) {
                                         return event.judgement.grade ==
                                                rhythm::JudgementGrade::Bad;
                                     }),
                "TickRoll must not emit a head BAD judgement.");
        for (const int time : {-54'500, 70'500})
        {
            const auto hit = tickRoll.session->ProcessInput('F', rhythm::InputEdge::Pressed,
                                                            rhythm::RhythmTime{time});
            Require(std::ranges::any_of(hit.events,
                                        [](const auto &event) {
                                            return event.type ==
                                                       rhythm::NoteEventType::TickAccepted &&
                                                   event.judgement.grade ==
                                                       rhythm::JudgementGrade::Unjudged &&
                                                   event.judgement.scoreRate == 1.0;
                                        }),
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
        rhythm::RuleBasedNote hold(
            1, {}, profile,
            std::make_unique<rhythm::HoldInputRule>(1, rhythm::RhythmTime{10'100'000}, ticks));
        // Default interpolation: 14,166 us is approximately 96 percent.
        static_cast<void>(
            hold.ProcessInput({rhythm::RhythmTime{14'166}, 1, 'F', rhythm::InputEdge::Pressed}));
        static_cast<void>(hold.ProcessInput(
            {rhythm::RhythmTime{9'850'000}, 1, 'F', rhythm::InputEdge::Released}));
        const auto done = hold.Update({rhythm::RhythmTime{10'100'000}, {}});
        const double head = profile->Evaluate({}, rhythm::RhythmTime{14'166}).scoreRate;
        Require(done.finalizedAccuracies.size() == 1 && hold.Accuracy().acceptedTicks == 98 &&
                    std::abs(hold.Accuracy().ScoreRate() - (head + 0.98) * 0.5) < 1e-9 &&
                    std::abs(hold.Accuracy().ScoreRate() - 0.97) < 0.00001,
                "Buzz must combine interpolated head and 98/100 held ticks equally.");
        Require(hold.Update({rhythm::RhythmTime{11'000'000}, {}}).finalizedAccuracies.empty(),
                "An ended hold must not emit its accuracy twice.");
        hold.Reset();
        static_cast<void>(hold.ProcessInput({{}, 1, 'F', rhythm::InputEdge::Pressed}));
        static_cast<void>(
            hold.ProcessInput({rhythm::RhythmTime{1}, 1, 'J', rhythm::InputEdge::Pressed}));
        static_cast<void>(
            hold.ProcessInput({rhythm::RhythmTime{2}, 1, 'F', rhythm::InputEdge::Released}));
        static_cast<void>(
            hold.ProcessInput({rhythm::RhythmTime{150'000}, 1, 'J', rhythm::InputEdge::Released}));
        Require(hold.Accuracy().acceptedTicks == 1,
                "Releasing one physical key must not cancel another held key of the same action.");

        auto buzz = mode::TaikoMode{}.CreateSession(
            MakeLongPattern(mode::TaikoNoteType::Buzz, {"Action=Don", "TickDivision=16"}));
        Require(buzz.session->FindNotePresentation(1)->tickTimes.size() == 7 &&
                    buzz.session->FindNotePresentation(1)->tickTimes.front() ==
                        rhythm::RhythmTime{125'000},
                "Buzz body ticks must exclude the independently judged head.");
    }
} // namespace finger_drum::tests
