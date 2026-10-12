#include "Taiko/Results/TaikoPlayResult.h"
#include "TestSupport.h"
#include <limits>

namespace finger_drum::tests
{
    void TestAccuracySixDecimalPrecision()
    {
        using rhythm::AccuracyScore;
        Require(AccuracyScore::FromRate(1.0 / 3).Units() == 33'333'333,
                "Percentage accuracy must truncate after six decimal places.");
        Require(AccuracyScore::Average(199'999'999, 2).Units() == 99'999'999,
                "The mean must use integer sums and truncate, rather than round up.");
        Require(AccuracyScore::FromRate(-1).Units() == 0 && AccuracyScore::FromRate(2).Units() == 100'000'000 &&
                    AccuracyScore::FromRate(std::numeric_limits<double>::quiet_NaN()).Units() == 0,
                "Score conversion must remain bounded and handle non-finite input.");
        for (const std::uint32_t units : {0U, 1U, 22'222'222U, 58'000'000U, 99'999'999U, 100'000'000U})
            Require(AccuracyScore::FromRate(AccuracyScore::FromUnits(units).Rate()).Units() == units,
                    "Converting a recorded score must not lose another decimal digit.");
        rhythm::NoteAccuracy ticks;
        ticks.target = {rhythm::NoteAccuracyKind::Ticks, 0, 3};
        ticks.acceptedTicks = 1;
        Require(ticks.Score().Units() == 33'333'333, "A logical tick note must record a six-decimal score.");
        rhythm::AccuracyRange range;
        const auto judgement = range.Evaluate({}, rhythm::RhythmTime{10'001});
        Require(judgement.scoreRate == AccuracyScore::FromRate(judgement.scoreRate).Rate(),
                "Each judgement score must already have the recording precision.");
    }

    void TestPlayResultCollection()
    {
        const auto parsed =
            chart::ChartParser{}.ParsePattern("[Metadata]\nName: Result regression\nBase BPM: 120\n"
                                              "[Difficulty]\nMode: Taiko\n[Time Signature]\n[Pattern]\n"
                                              "0/1,1,0\n1/4,3,0\n1/2,11,1,,HitCount=3\n3/4,11,2\n--\n0/1,1,0\n",
                                              "result.ymp");
        Require(parsed.Succeeded(), "Result test chart must parse.");
        auto loaded = mode::TaikoMode{}.CreateSession(parsed.document);
        Require(loaded.Succeeded(), "Result test session must load.");
        auto &session = *loaded.session;
        session.SetResultInitialTime(rhythm::RhythmTime{-2'000'000});
        const auto press = [&](char key, std::int64_t us)
        { return session.ProcessInput(key, rhythm::InputEdge::Pressed, rhythm::RhythmTime{us}); };
        static_cast<void>(press('F', -1'000'000)); // Empty input is retained, never POOR.
        static_cast<void>(press('D', 2'000));
        static_cast<void>(session.ProcessInput('D', rhythm::InputEdge::Released, rhythm::RhythmTime{3'000}));
        const auto normal = press('F', 10'000);
        static_cast<void>(session.ProcessInput('F', rhythm::InputEdge::Released, rhythm::RhythmTime{10'000}));
        static_cast<void>(press('F', 500'000));
        const auto big = press('J', 510'000);
        static_cast<void>(press('F', 1'000'000));
        const auto roll = session.Update(rhythm::RhythmTime{1'500'000});
        const auto expectedSum = static_cast<std::uint64_t>(normal.finalizedAccuracies.front().Score().Units()) +
                                 big.finalizedAccuracies.front().Score().Units() +
                                 roll.finalizedAccuracies.front().Score().Units();
        auto result = session.TakeResult(mode::PlayEndReason::Interrupted, rhythm::RhythmTime{1'500'000});
        const auto *taiko = dynamic_cast<const mode::TaikoPlayResult *>(result.get());
        if (!taiko)
            throw std::runtime_error("Taiko result type must be preserved.");
        Require(taiko->summary.totalNotes == 4 && taiko->summary.processedNotes == 3 &&
                    taiko->summary.unprocessedNotes == 1 && taiko->summary.accuracy.Units() == expectedSum / 4,
                "A long tail is not another logical note; interrupted accuracy uses the whole chart.");
        Require(taiko->judgements[0] == 1 && taiko->judgements[2] == 2 && taiko->judgements[5] == 1 &&
                    taiko->poorCount == 1 && taiko->okCount == 0,
                "Count graded inputs and failed completions once; releases and blanks are never POOR.");
        Require(taiko->timingErrors.size() == 3 && taiko->meanErrorMilliseconds &&
                    std::abs(*taiko->meanErrorMilliseconds - 20.0 / 3) < 1e-9 &&
                    std::abs(*taiko->varianceMillisecondsSquared - 200.0 / 9) < 1e-9,
                "Population variance must exclude automatic misses and free roll hits.");
        Require(taiko->groups[0].total == 2 && taiko->groups[1].total == 1 && taiko->groups[2].total == 1 &&
                    taiko->notes[2].accuracy.Units() == 33'333'333 && taiko->notes[3].accuracy.Units() == 0,
                "Groups retain equal logical-note weights and truncated partial roll accuracy.");
        const auto &inputs = taiko->summary.inputs;
        Require(inputs.size() == 8 && inputs[0].time.count() == -1'000'000 && inputs[3].time == inputs[4].time &&
                    inputs[3].edge == rhythm::InputEdge::Pressed && inputs[4].edge == rhythm::InputEdge::Released &&
                    inputs[4].physicalKey == 'F' && taiko->summary.inputMapping.at('F') == inputs[3].action &&
                    taiko->summary.initialTime.count() == -2'000'000,
                "Input logs must preserve signed microseconds, physical keys, edges and tied event order.");
        loaded.session.reset();
        Require(result->Summary().inputs.size() == 8 && taiko->notes.size() == 4,
                "Result ownership must be independent of the destroyed play session.");
    }

    void TestLongResultCompletionAndReset()
    {
        for (const auto type : {mode::TaikoNoteType::Roll, mode::TaikoNoteType::BigRoll, mode::TaikoNoteType::Balloon,
                                mode::TaikoNoteType::DengDeng, mode::TaikoNoteType::TickRoll,
                                mode::TaikoNoteType::BigTickRoll, mode::TaikoNoteType::Buzz})
        {
            const bool timed = type == mode::TaikoNoteType::TickRoll || type == mode::TaikoNoteType::BigTickRoll ||
                               type == mode::TaikoNoteType::Buzz;
            auto options = timed ? std::vector<std::string>{"TickDivision=4"} : std::vector<std::string>{"HitCount=1"};
            if (type == mode::TaikoNoteType::Buzz)
                options.push_back("Action=Don");
            auto loaded = mode::TaikoMode{}.CreateSession(MakeLongPattern(type, std::move(options)));
            Require(loaded.Succeeded(), "Long result session must load.");
            auto &session = *loaded.session;
            static_cast<void>(session.ProcessInput('F', rhythm::InputEdge::Pressed, {}));
            session.Reset();
            static_cast<void>(session.ProcessInput('F', rhythm::InputEdge::Pressed, {}));
            if (type == mode::TaikoNoteType::TickRoll || type == mode::TaikoNoteType::BigTickRoll)
                for (const std::int64_t us : {500'000, 1'000'000})
                    static_cast<void>(session.ProcessInput('F', rhythm::InputEdge::Pressed, rhythm::RhythmTime{us}));
            const std::array<rhythm::PhysicalKey, 1> held{'F'};
            static_cast<void>(session.Update(rhythm::RhythmTime{1'000'000}, held));
            static_cast<void>(session.Update(rhythm::RhythmTime{1'100'000}, held));
            auto result = session.TakeResult(mode::PlayEndReason::Completed, rhythm::RhythmTime{1'100'000});
            const auto *taiko = dynamic_cast<const mode::TaikoPlayResult *>(result.get());
            if (!taiko)
                throw std::runtime_error("Long Taiko result type must be preserved.");
            Require(taiko->okCount == 1 && taiko->judgements[5] == 0 && taiko->summary.processedNotes == 1 &&
                        taiko->summary.accuracy.Units() == 100'000'000,
                    "Each successful long-note completion must produce exactly one OK.");
            Require(taiko->timingErrors.size() == (type == mode::TaikoNoteType::Buzz ? 1U : 0U),
                    "Buzz head and HoldStarted must not duplicate one timing error.");
            Require(taiko->summary.inputs.size() ==
                        (type == mode::TaikoNoteType::TickRoll || type == mode::TaikoNoteType::BigTickRoll ? 3U : 1U),
                    "Reset must remove the previous run's log.");
        }
        auto loaded = mode::TaikoMode{}.CreateSession(MakeLongPattern(mode::TaikoNoteType::Buzz, {"Action=Don"}));
        Require(loaded.Succeeded(), "Interrupted Buzz fixture must load.");
        static_cast<void>(loaded.session->ProcessInput('F', rhythm::InputEdge::Pressed, {}));
        auto result = loaded.session->TakeResult(mode::PlayEndReason::Interrupted, rhythm::RhythmTime{10'000});
        Require(result->Summary().unprocessedNotes == 1 && result->Summary().accuracy.Units() == 0,
                "A partially active long note remains unprocessed with zero score on interruption.");
    }
} // namespace finger_drum::tests
