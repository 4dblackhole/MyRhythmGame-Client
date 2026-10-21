#include "Taiko/Results/TaikoPlayResult.h"
#include "TestSupport.h"

namespace tests
{
    void TestExpiredInputOrdering()
    {

        // Identical input, with and without an intervening unlimited-loop update.
        for (const bool split : {false, true})
        {
            PatternDocument pattern;
            pattern.mode = "Taiko";
            for (const auto fraction : {Rational{0, 1}, Rational{1, 400}, Rational{1, 20}})
                pattern.notes.push_back({.position = {0, fraction}, .keyType = 1});
            auto loaded = TaikoMode{}.CreateSession(pattern);
            Require(loaded.Succeeded(), "Expired input fixture must load.");
            auto &session = *loaded.session;
            static_cast<void>(session.Update(RhythmTime{89'000}));
            if (split)
                static_cast<void>(session.Update(RhythmTime{99'999}));
            const auto hit = session.ProcessInput('F', InputEdge::Pressed, RhythmTime{100'000});
            Require(HasEvent(hit, NoteEventType::HitAccepted), "Input must pass all expired notes before judging.");
            auto result = session.TakeResult(PlayEndReason::Completed, RhythmTime{100'001});
            const auto &taiko = dynamic_cast<const TaikoPlayResult &>(*result);
            Require(taiko.judgements[0] == 1 && taiko.judgements[5] == 2 && taiko.timingErrors.size() == 1 &&
                        taiko.timingErrors.front().error == RhythmDuration{} && taiko.summary.processedNotes == 3,
                    "Frame splitting must preserve hit/miss counts, accuracy and timing error.");
        }
        auto profile = std::make_shared<AccuracyRange>();
        Lane boundary;
        boundary.AddNote(MakeTap(1, {}, profile));
        boundary.Finalize();
        const auto end = boundary.Notes().front()->ExpireTime();
        Require(!HasEvent(boundary.ProcessInput({end, 2, 'D', InputEdge::Pressed}), NoteEventType::Missed),
                "The exact expiry boundary remains closed.");
        Require(HasEvent(boundary.ProcessInput({end + RhythmDuration{1}, 1, 'F', InputEdge::Pressed}),
                         NoteEventType::Missed),
                "The first microsecond after expiry must retire the note.");

        for (const bool split : {false, true})
        {
            auto pattern = MakeLongPattern(TaikoNoteType::Buzz, {"TickDivision=8", "Action=Don"});
            pattern.notes.push_back({.position = {0, {1001, 2000}}, .keyType = 1});
            auto loaded = TaikoMode{}.CreateSession(pattern);
            Require(loaded.Succeeded(), "Tail-to-head fixture must load.");
            auto &session = *loaded.session;
            static_cast<void>(session.ProcessInput('J', InputEdge::Pressed, {}));
            if (split)
                static_cast<void>(session.Update(RhythmTime{1'000'000}, std::array<PhysicalKey, 1>{'J'}));
            const auto result = session.ProcessInput('F', InputEdge::Pressed, RhythmTime{1'001'000});
            Require(HasEvent(result, NoteEventType::HitAccepted) && session.FinalizedNoteCount() == 2,
                    "A held long tail must complete before the next timestamped head.");
            Require(session.Gear().Lanes().front()->Notes().front()->Accuracy().acceptedTicks == 4,
                    "Expiry must use the preceding held state for every remaining long-note tick.");
        }
    }

    void TestAuthoritativeHeldInputs()
    {

        for (const bool split : {false, true})
        {
            auto loaded = TaikoMode{}.CreateSession(
                MakeLongPattern(TaikoNoteType::Buzz, {"TickDivision=8", "Action=Don"}));
            Require(loaded.Succeeded(), "Held-input fixture must load.");
            auto &session = *loaded.session;
            static_cast<void>(session.Update(RhythmTime{-100'000}, std::array<PhysicalKey, 1>{'F'}));
            static_cast<void>(session.ProcessInput('J', InputEdge::Pressed, {}));
            if (split)
                static_cast<void>(session.Update(RhythmTime{249'999}, std::array<PhysicalKey, 2>{'F', 'J'}));
            const auto release = session.ProcessInput('J', InputEdge::Released, RhythmTime{250'000});
            Require(HasEvent(release, NoteEventType::TickAccepted) && !HasEvent(release, NoteEventType::TickMissed),
                    "Releasing the head key must preserve another preheld key at the same tick.");
            static_cast<void>(session.ProcessInput('J', InputEdge::Pressed, RhythmTime{300'000}));
            static_cast<void>(session.ProcessInput('F', InputEdge::Released, RhythmTime{400'000}));
            const auto lastRelease = session.ProcessInput('J', InputEdge::Released, RhythmTime{500'000});
            Require(HasEvent(lastRelease, NoteEventType::TickMissed), "The final physical release must miss the tick.");
            // Pause snapshots can replace held keys without a judged press.
            static_cast<void>(session.Update(RhythmTime{500'001}, std::array<PhysicalKey, 1>{'F'}));
            static_cast<void>(session.ProcessInput('J', InputEdge::Pressed, RhythmTime{600'000}));
            const auto pausedRelease = session.ProcessInput('J', InputEdge::Released, RhythmTime{750'000});
            Require(HasEvent(pausedRelease, NoteEventType::TickAccepted),
                    "Resume must use the restored physical state.");
            session.RewindJudgements(RhythmTime{800'000});
            const auto &accuracy = session.Gear().Lanes().front()->Notes().front()->Accuracy();
            Require(accuracy.acceptedTicks == 2 &&
                        session.Gear().Lanes().front()->Notes().front()->ProcessedTickCount() == 3,
                    "Rewind must reproduce physical snapshots and exactly-once tick decisions.");
        }
    }
} // namespace youmu::tests
