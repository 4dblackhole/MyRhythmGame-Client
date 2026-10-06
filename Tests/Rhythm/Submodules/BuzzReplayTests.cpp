#include "AllNotesReplay.h"

namespace finger_drum::tests
{
    namespace
    {
        void VerifyDenseBuzz(const int division, const bool kat, const rhythm::RhythmDuration delay,
                             const bool incremental)
        {
            auto pattern = MakeLongPattern(mode::TaikoNoteType::Buzz, {kat ? "Action=Kat" : "Action=Don",
                                                                       "TickDivision=" + std::to_string(division)});
            pattern.baseBpm = 180;
            pattern.patternOffsetMilliseconds = 104;
            auto loaded = mode::TaikoMode{}.CreateSession(pattern);
            Require(loaded.Succeeded(), "Dense Buzz fixture must load through TaikoMode.");
            auto &session = *loaded.session;
            const auto &note = *session.Gear().Lanes().front()->Notes().front();
            const auto &view = *session.FindNotePresentation(note.Id());
            const auto press = note.Timing() + delay;
            const rhythm::PhysicalKey key = kat ? rhythm::PhysicalKey{'D'} : rhythm::PhysicalKey{'F'};
            rhythm::NoteProcessResult log;
            if (incremental)
                for (auto time = note.Timing(); time < press; time += rhythm::RhythmDuration{997})
                    log.Append(session.Update(time));
            const auto head = session.ProcessInput(key, rhythm::InputEdge::Pressed, press);
            log.Append(head);
            const bool withinGood = note.Profile().IsWithin(rhythm::JudgementGrade::Good, note.Timing(), press);
            Require(HasEvent(head, rhythm::NoteEventType::HitAccepted) == withinGood &&
                        HasEvent(head, rhythm::NoteEventType::HoldStarted) == withinGood,
                    "Buzz head must respect the inclusive GOOD boundary.");
            const std::array held{key};
            log.Append(session.Update(view.endTime, held));
            log.Append(session.Update(view.endTime + rhythm::RhythmDuration{100'000}, held));
            std::size_t index = 0, accepted = 0;
            for (const auto &event : log.events)
            {
                if (event.type != rhythm::NoteEventType::TickAccepted &&
                    event.type != rhythm::NoteEventType::TickMissed)
                    continue;
                Require(index < view.tickTimes.size() && event.tickIndex == index &&
                            event.eventTime == view.tickTimes[index],
                        "Dense ticks must not disappear or repeat: division=" + std::to_string(division) +
                            " delay=" + std::to_string(delay.count()) + " incremental=" + std::to_string(incremental) +
                            " expected index=" + std::to_string(index) + " actual=" + std::to_string(event.tickIndex) +
                            " time=" + std::to_string(event.eventTime.count()) + " expected=" +
                            std::to_string(view.tickTimes[std::min(index, view.tickTimes.size() - 1)].count()));
                const bool success = withinGood && view.tickTimes[index] >= press;
                ++index;
                Require((event.type == rhythm::NoteEventType::TickAccepted) == success,
                        "Overdue ticks must use pre-press state; exact-time tick uses held state.");
                if (success)
                    ++accepted;
            }
            Require(index == view.tickTimes.size() && note.Accuracy().acceptedTicks == accepted &&
                        log.finalizedAccuracies.size() == 1,
                    "Buzz must finalize every tick and its accuracy exactly once.");
            const auto tickCues =
                std::ranges::count_if(log.audioCues, [](const auto &cue) { return cue.bus == "TickSound"; });
            Require(static_cast<std::size_t>(tickCues) == accepted,
                    "Missed ticks must not produce tick sounds or retroactive hit credit.");
        }
    } // namespace

    void TestDenseBuzzLateGoodHead()
    {
        for (const int division : {64, 256, 1024})
            for (const bool kat : {false, true})
                for (const bool incremental : {false, true})
                    for (const long long delay : {-54'500, 0, 50'000, 54'500, 54'501})
                        VerifyDenseBuzz(division, kat, rhythm::RhythmDuration{delay}, incremental);
        // At 180 BPM the 32nd tick of division 1024 is rounded to 41,667 us.
        for (const long long delay : {41'666, 41'667, 41'668})
            VerifyDenseBuzz(1024, false, rhythm::RhythmDuration{delay}, false);
        std::cout << "Dense Buzz late-GOOD regression verified: 63 cases.\n";
    }

    void TestAngelDreamAllNotesReplay(const std::filesystem::path &songsRoot)
    {
        auto loaded = mode::TaikoMode{}.LoadSession(songsRoot / "Pattern/angeldream/angeldream [all notes test].ymp");
        Require(loaded.Succeeded(), "Real AngelDream all-notes YMP must load.");
        const auto parsed =
            chart::ChartParser{}.ParsePatternFile(songsRoot / "Pattern/angeldream/angeldream [all notes test].ymp");
        std::set<int> types;
        for (const auto &note : parsed.document.notes)
            types.insert(note.keyType);
        Require(types == std::set<int>{1, 2, 3, 4, 5, 11, 12, 13, 14, 15, 16, 17},
                "Actual YMP must cover all twelve persisted note IDs.");
        AllNotesReplay replay(*loaded.session);
        for (auto time = rhythm::RhythmTime{-2'000'000}; time <= replay.End(); time += rhythm::RhythmDuration{16'667})
            static_cast<void>(replay.Advance(*loaded.session, time));
        static_cast<void>(replay.Advance(*loaded.session, replay.End()));
        replay.Verify(*loaded.session);
    }
} // namespace finger_drum::tests
