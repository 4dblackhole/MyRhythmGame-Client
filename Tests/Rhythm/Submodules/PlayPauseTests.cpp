#include "Taiko/Results/TaikoPlayResult.h"
#include "TestSupport.h"

namespace finger_drum::tests
{
    void TestRewindJudgementsAndTrace()
    {
        chart::PatternDocument pattern;
        pattern.mode = "Taiko";
        for (std::int64_t i = 0; i < 3; ++i)
            pattern.notes.push_back({.position = {0, {i, 4}}, .keyType = 1});
        auto loaded = mode::TaikoMode{}.CreateSession(pattern);
        Require(loaded.Succeeded(), "Rewind fixture must load.");
        auto &s = *loaded.session;
        for (const auto us : {0, 500'000})
        {
            static_cast<void>(s.ProcessInput('F', rhythm::InputEdge::Pressed, rhythm::RhythmTime{us}));
            static_cast<void>(s.ProcessInput('F', rhythm::InputEdge::Released, rhythm::RhythmTime{us + 1}));
        }
        s.RecordControl(
            {mode::PlayControlType::Pause, rhythm::RhythmTime{800'000}, rhythm::RhythmTime{800'000}, 0, true});
        Require(!s.CanUseHp(), "The first pause menu must disable HP for this attempt.");
        s.RewindJudgements(rhythm::RhythmTime{500'000});
        s.RecordControl({mode::PlayControlType::Rewind, rhythm::RhythmTime{800'000}, rhythm::RhythmTime{500'000}});
        Require(s.FinalizedNoteCount() == 1 && s.Gear().Lanes().front()->CurrentNote()->Timing().count() == 500'000,
                "Rewind must retain earlier grades and undo target-time grades.");
        static_cast<void>(s.ProcessInput('F', rhythm::InputEdge::Pressed, rhythm::RhythmTime{510'000}));
        static_cast<void>(s.ProcessInput('F', rhythm::InputEdge::Released, rhythm::RhythmTime{510'001}));
        s.RecordControl(
            {mode::PlayControlType::Pause, rhythm::RhythmTime{900'000}, rhythm::RhythmTime{900'000}, 0, true});
        s.RewindJudgements({});
        s.RecordControl({mode::PlayControlType::Rewind, rhythm::RhythmTime{900'000}, {}});
        Require(s.FinalizedNoteCount() == 0, "Repeated rewind must undo the newer branch too.");
        static_cast<void>(s.ProcessInput('F', rhythm::InputEdge::Pressed, {}));
        auto result = s.TakeResult(mode::PlayEndReason::Interrupted, rhythm::RhythmTime{1});
        const auto *taiko = dynamic_cast<const mode::TaikoPlayResult *>(result.get());
        if (!taiko)
            throw std::runtime_error("Taiko rewind result required.");
        Require(taiko->summary.inputs.size() == 7 && taiko->summary.controls.size() == 4 &&
                    taiko->summary.controls[1].inputIndex == 4 && taiko->summary.controls[3].inputIndex == 6,
                "Replay inputs and ordered pause/rewind markers must survive cancellation.");
        Require(taiko->timingErrors.size() == 1 && taiko->judgements[0] == 1 && taiko->summary.processedNotes == 1 &&
                    !taiko->summary.IsCleared() && !taiko->summary.CanUseHp(),
                "Statistics must contain only the current branch, and practice must remain latched.");
    }
    void TestLongNoteRewindState()
    {
        const std::array<rhythm::PhysicalKey, 1> held{'F'};
        auto loaded = mode::TaikoMode{}.CreateSession(
            MakeLongPattern(mode::TaikoNoteType::Buzz, {"TickDivision=8", "Action=Don"}));
        Require(loaded.Succeeded(), "Buzz rewind fixture must load.");
        auto &s = *loaded.session;
        static_cast<void>(s.ProcessInput('F', rhythm::InputEdge::Pressed, {}));
        static_cast<void>(s.Update(rhythm::RhythmTime{250'000}, held));
        static_cast<void>(s.Update(rhythm::RhythmTime{500'000}, held));
        s.RewindJudgements(rhythm::RhythmTime{400'000});
        Require(s.Gear().Lanes().front()->Notes().front()->ProcessedTickCount() == 1,
                "Rewind inside a long note must preserve the head and earlier tick, not reset the whole note.");
        static_cast<void>(s.Update(rhythm::RhythmTime{500'000}, held));
        auto result = s.TakeResult(mode::PlayEndReason::Interrupted, rhythm::RhythmTime{550'000});
        const auto *taiko = dynamic_cast<const mode::TaikoPlayResult *>(result.get());
        if (!taiko)
            throw std::runtime_error("Buzz rewind result required.");
        Require(taiko->acceptedTicks == 2 && taiko->timingErrors.size() == 1,
                "Replayed ticks and head grades must not be double counted.");
    }
    void TestRetireAndRetryEligibility()
    {
        for (const auto type : {mode::TaikoNoteType::Roll, mode::TaikoNoteType::BigRoll, mode::TaikoNoteType::Balloon,
                                mode::TaikoNoteType::DengDeng, mode::TaikoNoteType::TickRoll,
                                mode::TaikoNoteType::BigTickRoll, mode::TaikoNoteType::Buzz})
        {
            auto p = MakeLongPattern(type, {"HitCount=4", "TickDivision=8", "Action=Don"});
            auto loaded = mode::TaikoMode{}.CreateSession(p);
            Require(loaded.Succeeded(), "Retirement fixture must load.");
            auto &s = *loaded.session;
            static_cast<void>(s.ProcessInput('F', rhythm::InputEdge::Pressed, {}));
            s.RecordControl(
                {mode::PlayControlType::Pause, rhythm::RhythmTime{300'000}, rhythm::RhythmTime{300'000}, 0, true});
            s.Reset();
            Require(s.CanUseHp(), "Retry must reset pause eligibility for a new attempt.");
            static_cast<void>(s.ProcessInput('F', rhythm::InputEdge::Pressed, {}));
            s.RecordControl(
                {mode::PlayControlType::Pause, rhythm::RhythmTime{300'000}, rhythm::RhythmTime{300'000}, 0, true});
            s.RecordControl({mode::PlayControlType::Retire, rhythm::RhythmTime{300'000}, rhythm::RhythmTime{300'000}});
            s.RetireRemaining(rhythm::RhythmTime{300'000});
            auto result = s.TakeResult(mode::PlayEndReason::Interrupted, rhythm::RhythmTime{300'000});
            const auto *taiko = dynamic_cast<const mode::TaikoPlayResult *>(result.get());
            if (!taiko)
                throw std::runtime_error("Retired Taiko result required.");
            Require(taiko->summary.processedNotes == 1 && taiko->summary.unprocessedNotes == 0 &&
                        taiko->judgements[5] == 1 && taiko->okCount == 0 && taiko->notes[0].accuracy.Units() == 0 &&
                        taiko->notes[0].finalState == rhythm::NoteState::Missed,
                    "Every unfinished long note must retire as a zero-score logical MISS.");
            Require(taiko->summary.inputs.size() == 1 && taiko->summary.controls.size() == 2 &&
                        !taiko->summary.CanUseHp() && !taiko->summary.IsCleared(),
                    "The new attempt must omit old controls and preserve its own retirement markers.");
        }
        chart::PatternDocument pattern;
        pattern.mode = "Taiko";
        pattern.notes.push_back({.position = {}, .keyType = 1});
        auto completed = mode::TaikoMode{}.CreateSession(pattern);
        Require(completed.Succeeded(), "Completed practice fixture must load.");
        auto &session = *completed.session;
        session.RecordControl({mode::PlayControlType::Pause, {}, {}, 0, true});
        static_cast<void>(session.ProcessInput('F', rhythm::InputEdge::Pressed, {}));
        auto result = session.TakeResult(mode::PlayEndReason::Completed, {});
        Require(result->Summary().processedNotes == 1 && !result->Summary().IsCleared() &&
                    !result->Summary().CanUseHp(),
                "Finishing every note must not restore clear/HP eligibility after the pause menu.");
    }
} // namespace finger_drum::tests
