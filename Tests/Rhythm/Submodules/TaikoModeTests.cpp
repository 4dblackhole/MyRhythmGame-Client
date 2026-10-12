#include "TestSupport.h"
#include "Taiko/Results/TaikoPlayResult.h"

namespace finger_drum::tests
{
    void TestDengDengEitherStart()
    {
        for (const char first : {'F', 'D'})
        {
            auto pattern = MakeLongPattern(mode::TaikoNoteType::DengDeng, {"HitCount=3"});
            pattern.patternOffsetMilliseconds = 500;
            auto loaded = mode::TaikoMode{}.CreateSession(pattern);
            Require(loaded.Succeeded(), "Either-start DengDeng fixture must load.");
            auto &session = *loaded.session;
            const auto &note = *session.Gear().Lanes().front()->Notes().front();
            const char other = first == 'F' ? 'D' : 'F';
            const auto reject = [&](char key, std::int64_t us, std::size_t count, rhythm::InputEdge edge)
            {
                const auto state = note.State();
                const auto result = session.ProcessInput(key, edge, rhythm::RhythmTime{us});
                Require(
                    note.Progress() == rhythm::NoteProgress{count, 3} && note.State() == state &&
                        !HasEvent(result, rhythm::NoteEventType::HitAccepted) &&
                        std::ranges::all_of(result.events, [](const auto &event)
                                            { return event.judgement.grade == rhythm::JudgementGrade::Unjudged; }),
                    "Wrong/released/out-of-range input must not change the phase, count or emit a timing rejection.");
            };
            const auto accept = [&](char key, std::int64_t us, std::size_t count)
            {
                const auto result = session.ProcessInput(key, rhythm::InputEdge::Pressed, rhythm::RhythmTime{us});
                const auto sound = key == 'F' ? "Taiko.Don.Hit" : "Taiko.Kat.Hit";
                Require(HasEvent(result, rhythm::NoteEventType::HitAccepted) &&
                            note.Progress() == rhythm::NoteProgress{count, 3} && result.audioCues.size() == 1 &&
                            result.audioCues.front().sound == sound,
                        "Accepted alternation must count once and sound the actual action, including Kat-first.");
            };
            reject(other, 499'999, 0, rhythm::InputEdge::Pressed);
            reject(other, 500'000, 0, rhythm::InputEdge::Released);
            accept(first, 900'000, 1);
            reject(first, 910'000, 1, rhythm::InputEdge::Pressed);
            accept(other, 920'000, 2);
            reject(other, 930'000, 2, rhythm::InputEdge::Pressed);
            accept(first, 940'000, 3);
            Require(note.Accuracy().Score().Units() == 100'000'000,
                    "Wrong presses between correct alternations must not reduce full completion accuracy.");
            session.Reset();
            Require(note.Progress() == rhythm::NoteProgress{0, 3}, "Reset must restore the initial unbound phase.");
            // Start with the opposite action after Reset, proving the prior phase is gone.
            accept(other, 500'000, 1);
            reject(other, 510'000, 1, rhythm::InputEdge::Pressed);
            accept(first, 520'000, 2);
            static_cast<void>(session.Update(rhythm::RhythmTime{1'500'000}));
            const auto result = session.TakeResult(mode::PlayEndReason::Completed, rhythm::RhythmTime{1'500'000});
            const auto *taiko = dynamic_cast<const mode::TaikoPlayResult *>(result.get());
            Require(taiko && taiko->poorCount == 0 && taiko->timingErrors.empty() &&
                        taiko->summary.accuracy.Units() == 66'666'666,
                    "Wrong order must not add POOR/timing samples or subtract from the two accepted hits.");
        }
        // Explicit YME sound assignments still override action-based default sounds.
        auto pattern = MakeLongPattern(mode::TaikoNoteType::DengDeng, {"HitCount=3"});
        pattern.notes.front().hitSound = "1";
        chart::EffectDocument effects;
        effects.hitSounds["1"] = "explicit.wav";
        auto loaded = mode::TaikoMode{}.CreateSession(pattern, effects);
        Require(loaded.Succeeded(), "Explicit Kat-first sound fixture must load.");
        const auto hit = loaded.session->ProcessInput('D', rhythm::InputEdge::Pressed, rhythm::RhythmTime{});
        Require(hit.audioCues.size() == 1 && hit.audioCues.front().sound == "Chart.HitSound.1",
                "Kat-first must preserve the authored sound override.");
    }

    void TestCountOnlyRollHeadJudgements()
    {
        for (const auto type : {mode::TaikoNoteType::Roll, mode::TaikoNoteType::BigRoll, mode::TaikoNoteType::Balloon,
                                mode::TaikoNoteType::DengDeng})
        {
            auto pattern = MakeLongPattern(type, {"HitCount=3"});
            pattern.patternOffsetMilliseconds = 500;
            auto loaded = mode::TaikoMode{}.CreateSession(pattern);
            Require(loaded.Succeeded(), "Count-only interval fixture must load.");
            auto &session = *loaded.session;
            const auto &note = *session.Gear().Lanes().front()->Notes().front();
            const auto ungraded = [](const rhythm::NoteProcessResult &result)
            {
                return std::ranges::all_of(result.events, [](const auto &event)
                                           { return event.judgement.grade == rhythm::JudgementGrade::Unjudged; });
            };
            for (const auto us : {430'000, 500'000, 900'000, 1'500'000})
                Require(note.Preview({rhythm::RhythmTime{us}, 1, 'F'}).grade == rhythm::JudgementGrade::Unjudged,
                        "Count-only heads must never supply a timing grade.");
            const auto before = session.ProcessInput('F', rhythm::InputEdge::Pressed, rhythm::RhythmTime{430'000});
            Require(ungraded(before) && note.Accuracy().acceptedHits == 0,
                    "An early head input must be ungraded and must not count.");
            // The first hit is far beyond BAD; only the remaining interval and count matter.
            for (int index = 0; index < 3; ++index)
            {
                const char key = type == mode::TaikoNoteType::DengDeng && index == 1 ? 'D' : 'F';
                const auto hit = session.ProcessInput(key, rhythm::InputEdge::Pressed,
                                                      rhythm::RhythmTime{900'000 + index * 100'000});
                Require(ungraded(hit), "Accepted count-only hits must not create head timing feedback.");
            }
            static_cast<void>(session.Update(rhythm::RhythmTime{1'500'000}));
            Require(note.Accuracy().finalized && note.Accuracy().Score().Units() == 100'000'000,
                    "A late start meeting the count before the tail must earn full accuracy.");
            session.Reset();
            static_cast<void>(session.ProcessInput('F', rhythm::InputEdge::Pressed, rhythm::RhythmTime{500'000}));
            const char second = type == mode::TaikoNoteType::DengDeng ? 'D' : 'F';
            static_cast<void>(session.ProcessInput(second, rhythm::InputEdge::Pressed, rhythm::RhythmTime{1'500'000}));
            static_cast<void>(session.Update(rhythm::RhythmTime{1'500'000}));
            Require(note.Accuracy().Score().Units() == 66'666'666 && note.Accuracy().acceptedHits == 2,
                    "Both interval boundaries count, and partial accuracy retains six decimal places.");
            session.Reset();
            static_cast<void>(session.ProcessInput('F', rhythm::InputEdge::Pressed, rhythm::RhythmTime{1'500'001}));
            Require(note.Accuracy().acceptedHits == 0 && note.Accuracy().Score().Units() == 0,
                    "Inputs after the tail must not count.");
        }
        for (const auto type : {mode::TaikoNoteType::TickRoll, mode::TaikoNoteType::Buzz})
        {
            auto loaded = mode::TaikoMode{}.CreateSession(MakeLongPattern(
                type, type == mode::TaikoNoteType::Buzz ? std::vector<std::string>{"Action=Don"} : std::vector<std::string>{}));
            Require(loaded.Succeeded(), "Timing-based long-note fixture must load.");
            const auto &note = *loaded.session->Gear().Lanes().front()->Notes().front();
            Require(note.Preview({rhythm::RhythmTime{20'000}, 1, 'F'}).grade == rhythm::JudgementGrade::Great,
                    "TickRoll and Buzz must retain their start timing judgement.");
        }
    }

    void TestTaikoRollAndTickRollRules()
    {
        mode::TaikoMode taiko;
        mode::ModeLoadResult roll = taiko.CreateSession(MakeLongPattern(mode::TaikoNoteType::Roll));
        Require(roll.Succeeded(), "Roll must create a playable session.");
        const rhythm::NoteProcessResult don = roll.session->ProcessInput(
            'F', rhythm::InputEdge::Pressed, rhythm::RhythmTime{100'000});
        const rhythm::NoteProcessResult kat = roll.session->ProcessInput(
            'D', rhythm::InputEdge::Pressed, rhythm::RhythmTime{200'000});
        Require(HasEvent(don, rhythm::NoteEventType::TickAccepted) &&
                    HasEvent(kat, rhythm::NoteEventType::TickAccepted),
                "Roll must accept arbitrary Don and Kat presses in its interval.");

        mode::ModeLoadResult tickRoll = taiko.CreateSession(
            MakeLongPattern(mode::TaikoNoteType::TickRoll, {"TickDivision=16"}));
        Require(tickRoll.Succeeded(), "TickRoll must create a playable session.");
        Require(tickRoll.session->FindNotePresentation(1)->tickTimes.size() == 9,
                "TickRoll presentation must expose every authored tick to the scene.");
        const rhythm::NoteProcessResult first = tickRoll.session->ProcessInput(
            'F', rhythm::InputEdge::Pressed, rhythm::RhythmTime::zero());
        const rhythm::NoteProcessResult duplicate = tickRoll.session->ProcessInput(
            'D', rhythm::InputEdge::Pressed, rhythm::RhythmTime::zero());
        const rhythm::NoteProcessResult outsideGood = tickRoll.session->ProcessInput(
            'F', rhythm::InputEdge::Pressed, rhythm::RhythmTime{70'000});
        const rhythm::NoteProcessResult insideGood = tickRoll.session->ProcessInput(
            'D', rhythm::InputEdge::Pressed, rhythm::RhythmTime{71'000});
        Require(HasEvent(first, rhythm::NoteEventType::TickAccepted) &&
                    !HasEvent(duplicate, rhythm::NoteEventType::TickAccepted) &&
                    !HasEvent(outsideGood, rhythm::NoteEventType::TickAccepted) &&
                    HasEvent(insideGood, rhythm::NoteEventType::TickAccepted),
                "TickRoll must cap each tick to one hit and use the Good window.");

        Require(tickRoll.session->Gear().Lanes().front()->Notes().front()->ProcessedTickCount() == 2,
                "An accepted early tick must count as processed before its scheduled time.");
        const auto tickSnapshot =
            tickRoll.session->Gear().BuildSnapshot(rhythm::RhythmTime{71'000}, rhythm::RhythmDuration{2'000'000});
        Require(tickSnapshot.notes.front().processedTicks == 2,
                "Rendering must receive the rule's processed tick prefix.");
        tickRoll.session->Reset();
        Require(tickRoll.session->Gear().Lanes().front()->Notes().front()->ProcessedTickCount() == 0,
                "Reset must restore all tick markers.");

        mode::ModeLoadResult bigTickRoll = taiko.CreateSession(
            MakeLongPattern(mode::TaikoNoteType::BigTickRoll, {"TickDivision=16"}));
        mode::ModeLoadResult bigRoll =
            taiko.CreateSession(MakeLongPattern(mode::TaikoNoteType::BigRoll));
        Require(bigRoll.Succeeded() &&
                    bigRoll.session->FindNotePresentation(1)->visualKind ==
                        finger_drum::mode::NoteVisualKind::BigRoll &&
                    bigTickRoll.Succeeded() &&
                    bigTickRoll.session->FindNotePresentation(1)->visualKind ==
                        finger_drum::mode::NoteVisualKind::BigRoll,
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
        const auto parsedBareBalloon =
            chart::ChartParser{}.ParsePattern(BareBalloonPattern, "bare-balloon.ymp");
        mode::ModeLoadResult parsedBalloon = taiko.CreateSession(parsedBareBalloon.document);
        Require(parsedBareBalloon.Succeeded() && parsedBalloon.Succeeded() &&
                    parsedBalloon.session->Gear().Lanes().front()->Notes().front()->Progress() ==
                        rhythm::NoteProgress{0, 8},
                "The YMP parser and Taiko mode must preserve a bare Balloon "
                "ExtraData count.");

        mode::ModeLoadResult balloon =
            taiko.CreateSession(MakeLongPattern(mode::TaikoNoteType::Balloon, {"3"}));
        Require(balloon.Succeeded() &&
                    balloon.session->Gear().Lanes().front()->Notes().front()->Progress() ==
                        rhythm::NoteProgress{0, 3},
                "Balloon must accept a bare ExtraData hit count.");
        const rhythm::ScrollGearSnapshot initialBalloonSnapshot =
            balloon.session->Gear().BuildSnapshot(rhythm::RhythmTime::zero(),
                                                  rhythm::RhythmDuration{2'000'000});
        Require(initialBalloonSnapshot.notes.size() == 1 &&
                    initialBalloonSnapshot.notes.front().progress == rhythm::NoteProgress{0, 3},
                "The scroll snapshot must expose Balloon progress to presentation.");
        const rhythm::NoteProcessResult wrongBalloon = balloon.session->ProcessInput(
            'D', rhythm::InputEdge::Pressed, rhythm::RhythmTime{10'000});
        static_cast<void>(balloon.session->ProcessInput('F', rhythm::InputEdge::Pressed,
                                                        rhythm::RhythmTime{20'000}));
        static_cast<void>(balloon.session->ProcessInput('J', rhythm::InputEdge::Pressed,
                                                        rhythm::RhythmTime{30'000}));
        const rhythm::ScrollGearSnapshot activeBalloonSnapshot =
            balloon.session->Gear().BuildSnapshot(rhythm::RhythmTime{30'000},
                                                  rhythm::RhythmDuration{2'000'000});
        Require(activeBalloonSnapshot.notes.size() == 1 &&
                    activeBalloonSnapshot.notes.front().progress == rhythm::NoteProgress{2, 3},
                "The scroll snapshot must update Balloon progress after hits.");
        const rhythm::NoteProcessResult popped = balloon.session->ProcessInput(
            'F', rhythm::InputEdge::Pressed, rhythm::RhythmTime{40'000});
        Require(!HasEvent(wrongBalloon, rhythm::NoteEventType::HitAccepted) &&
                    HasEvent(popped, rhythm::NoteEventType::Completed) &&
                    std::ranges::any_of(popped.audioCues,
                                        [](const rhythm::AudioCueRequest &cue) {
                                            return cue.sound == "Taiko.Balloon.Pop";
                                        }),
                "Balloon must count only Don and play its pop cue on completion.");

        chart::PatternDocument defaultBalloonPattern =
            MakeLongPattern(mode::TaikoNoteType::Balloon);
        defaultBalloonPattern.notes.back().position.fraction = chart::Rational{1, 4};
        mode::ModeLoadResult defaultBalloon = taiko.CreateSession(defaultBalloonPattern);
        Require(defaultBalloon.Succeeded() &&
                    defaultBalloon.session->Gear().Lanes().front()->Notes().front()->Progress() ==
                        rhythm::NoteProgress{0, 3},
                "An unspecified quarter-note Balloon must default to "
                "ceil(1/4 * 12) = 3 hits.");
        static_cast<void>(defaultBalloon.session->ProcessInput('F', rhythm::InputEdge::Pressed,
                                                               rhythm::RhythmTime{10'000}));
        static_cast<void>(defaultBalloon.session->ProcessInput('J', rhythm::InputEdge::Pressed,
                                                               rhythm::RhythmTime{20'000}));
        const rhythm::NoteProcessResult defaultPopped = defaultBalloon.session->ProcessInput(
            'F', rhythm::InputEdge::Pressed, rhythm::RhythmTime{30'000});
        Require(HasEvent(defaultPopped, rhythm::NoteEventType::Completed),
                "The computed Balloon hit count must drive completion.");

        mode::ModeLoadResult dengDeng =
            taiko.CreateSession(MakeLongPattern(mode::TaikoNoteType::DengDeng, {"HitCount=4"}));
        Require(dengDeng.Succeeded(), "DengDeng must create a playable session.");
        static_cast<void>(dengDeng.session->ProcessInput('F', rhythm::InputEdge::Pressed,
                                                         rhythm::RhythmTime{10'000}));
        const rhythm::NoteProcessResult repeatedDon = dengDeng.session->ProcessInput(
            'J', rhythm::InputEdge::Pressed, rhythm::RhythmTime{20'000});
        static_cast<void>(dengDeng.session->ProcessInput('D', rhythm::InputEdge::Pressed,
                                                         rhythm::RhythmTime{30'000}));
        static_cast<void>(dengDeng.session->ProcessInput('F', rhythm::InputEdge::Pressed,
                                                         rhythm::RhythmTime{40'000}));
        const rhythm::NoteProcessResult alternated = dengDeng.session->ProcessInput(
            'K', rhythm::InputEdge::Pressed, rhythm::RhythmTime{50'000});
        Require(!HasEvent(repeatedDon, rhythm::NoteEventType::HitAccepted) &&
                    HasEvent(alternated, rhythm::NoteEventType::Completed),
                "DengDeng must require a Don/Kat alternating sequence.");

        mode::ModeLoadResult donBuzz = taiko.CreateSession(
            MakeLongPattern(mode::TaikoNoteType::Buzz, {"Action=Don", "TickDivision=16"}));
        Require(donBuzz.Succeeded(), "Don Buzz must create a playable session.");
        static_cast<void>(donBuzz.session->ProcessInput('F', rhythm::InputEdge::Pressed,
                                                        rhythm::RhythmTime::zero()));
        const std::array<rhythm::PhysicalKey, 1> heldDon{'F'};
        const rhythm::NoteProcessResult donTick =
            donBuzz.session->Update(rhythm::RhythmTime{125'000}, heldDon);
        const rhythm::NoteProcessResult releasedTick =
            donBuzz.session->Update(rhythm::RhythmTime{250'000});
        Require(HasEvent(donTick, rhythm::NoteEventType::TickAccepted) &&
                    donTick.audioCues.size() == 1 &&
                    donTick.audioCues.front().sound == "Taiko.Don.Hit" &&
                    HasEvent(releasedTick, rhythm::NoteEventType::TickMissed),
                "Buzz must sound only authored ticks while its action is held.");

        mode::ModeLoadResult katBuzz = taiko.CreateSession(
            MakeLongPattern(mode::TaikoNoteType::Buzz, {"Action=Kat", "TickDivision=16"}));
        Require(katBuzz.Succeeded() && katBuzz.session->FindNotePresentation(1)->visualKind ==
                                           finger_drum::mode::NoteVisualKind::KatBuzz,
                "Buzz must preserve the selected Don/Kat presentation.");

        mode::ModeLoadResult invalidBuzz =
            taiko.CreateSession(MakeLongPattern(mode::TaikoNoteType::Buzz, {"Action=Center"}));
        Require(!invalidBuzz.Succeeded(), "Buzz must reject an action other than Don or Kat.");
    }

    void TestMusicalSubdivisionTicksFollowTempo()
    {
        chart::PatternDocument pattern;
        pattern.baseBpm = 120.0;
        pattern.timing.push_back(chart::TimingDirective{.position = {0, chart::Rational{1, 4}},
                                                        .type = chart::TimingDirectiveType::Bpm,
                                                        .value = 240.0});
        const chart::MusicalTimeline timeline(pattern);
        const std::vector<rhythm::RhythmTime> ticks =
            timeline.CompileSubdivisions({0, chart::Rational{0, 1}}, {0, chart::Rational{1, 2}}, 4);
        Require(ticks == std::vector<rhythm::RhythmTime>{rhythm::RhythmTime{0}, rhythm::RhythmTime{500'000},
                                                         rhythm::RhythmTime{750'000}},
                "Musical subdivision ticks must follow BPM changes and include an aligned tail.");
        Require(timeline.CompileSubdivisions({0, {}}, {0, {3, 8}}, 4) ==
                    std::vector<rhythm::RhythmTime>{rhythm::RhythmTime{}, rhythm::RhythmTime{500'000}},
                "An unaligned tail must not add a partial-interval tick.");
        const chart::MusicalPosition roundedTail{0, {49'999'999'999, 100'000'000'000}};
        Require(timeline.Compile(roundedTail) == ticks.back() &&
                    timeline.CompileSubdivisions({0, {}}, roundedTail, 4).size() == 2,
                "Microsecond rounding must not turn an unaligned tail into a scheduled tick.");
        const chart::MusicalPosition head{0, {1, 10}}, tail{0, {7, 20}};
        const auto shiftedTicks = timeline.CompileSubdivisions(head, tail, 16);
        Require(shiftedTicks.size() == 5 && shiftedTicks.front() == timeline.Compile(head) &&
                    shiftedTicks.back() == timeline.Compile(tail),
                "Tail alignment must be relative to the head, independently of the global grid.");
        Require(timeline.CompileSubdivisions(tail, head, 16).empty() &&
                    timeline.CompileSubdivisions(head, tail, 0).empty() &&
                    timeline.CompileSubdivisions(head, head, 16) ==
                        std::vector<rhythm::RhythmTime>{timeline.Compile(head)},
                "Closed subdivision ranges must handle reversed, disabled and singleton intervals.");
        Require(timeline.CountSubdivisions({0, chart::Rational{0, 1}}, {0, chart::Rational{1, 8}},
                                           12) == 2,
                "Subdivision counts must use exact rational ceil(length * divisions).");
        Require(timeline.CountSubdivisions({0, {}}, {0, {1, 2}}, 4) == 2,
                "An aligned tail tick must not change automatic hit-count targets.");

        pattern.timing = {{{0, {}}, chart::TimingDirectiveType::MeasureLength, 0, {3, 4}},
                          {{1, {}}, chart::TimingDirectiveType::DelayMilliseconds, 100}};
        const chart::MusicalTimeline measures(pattern);
        const auto crossBarTicks = measures.CompileSubdivisions({0, {1, 2}}, {1, {1, 4}}, 4);
        Require(crossBarTicks == std::vector<rhythm::RhythmTime>{rhythm::RhythmTime{1'000'000},
                                                                 rhythm::RhythmTime{1'600'000},
                                                                 rhythm::RhythmTime{2'100'000}},
                "A tail tick must survive a changed measure length and positive delay across a bar.");
        pattern.timing = {{{0, {1, 2}}, chart::TimingDirectiveType::DelayMilliseconds, -750}};
        const chart::MusicalTimeline cut(pattern);
        Require(!cut.IsPositionActive({0, {1, 4}}) &&
                    cut.CompileSubdivisions({0, {}}, {0, {1, 4}}, 16) ==
                        std::vector<rhythm::RhythmTime>{rhythm::RhythmTime{}, rhythm::RhythmTime{125'000}},
                "A masked aligned tail must not be restored as a scheduled tick.");
        const auto boundaryTicks = cut.CompileSubdivisions({0, {}}, {0, {1, 2}}, 16);
        Require(boundaryTicks.size() == 3 && boundaryTicks.back() == cut.Compile({0, {1, 2}}),
                "The active negative-delay boundary must still include its aligned tail tick.");
    }

    void TestTaikoClosedTailTicks()
    {
        for (const auto type :
             {mode::TaikoNoteType::TickRoll, mode::TaikoNoteType::BigTickRoll, mode::TaikoNoteType::Buzz})
            for (const bool missTail : {false, true})
            {
                auto loaded = mode::TaikoMode{}.CreateSession(MakeLongPattern(type, {"TickDivision=16", "Action=Don"}));
                Require(loaded.Succeeded(), "Closed-tail fixture must load.");
                auto &session = *loaded.session;
                const auto &note = *session.Gear().Lanes().front()->Notes().front();
                const auto &view = *session.FindNotePresentation(note.Id());
                const bool buzz = type == mode::TaikoNoteType::Buzz;
                Require(view.tickTimes.size() == (buzz ? 8 : 9) && view.tickTimes.back() == view.endTime,
                        "TickRoll/BigTickRoll/Buzz must expose the aligned tail, with Buzz head excluded.");
                rhythm::NoteProcessResult log;
                if (buzz)
                {
                    log.Append(session.ProcessInput('F', rhythm::InputEdge::Pressed, note.Timing()));
                    if (missTail)
                        log.Append(session.ProcessInput('F', rhythm::InputEdge::Released, view.endTime));
                    const std::array<rhythm::PhysicalKey, 1> held{'F'};
                    log.Append(session.Update(view.endTime, missTail ? std::span<const rhythm::PhysicalKey>{} : held));
                }
                else
                {
                    for (std::size_t i = 0; i < view.tickTimes.size() - (missTail ? 1 : 0); ++i)
                    {
                        log.Append(session.Update(view.tickTimes[i] - rhythm::RhythmDuration{1}));
                        log.Append(session.ProcessInput('F', rhythm::InputEdge::Pressed, view.tickTimes[i]));
                    }
                    log.Append(session.Update(view.endTime));
                }
                const auto tailEvent = std::ranges::find_if(
                    log.events,
                    [&](const auto &event)
                    {
                        return event.tickIndex == view.tickTimes.size() - 1 && event.eventTime == view.endTime &&
                               event.type ==
                                   (missTail ? rhythm::NoteEventType::TickMissed : rhythm::NoteEventType::TickAccepted);
                    });
                Require(tailEvent != log.events.end() && note.ProcessedTickCount() == view.tickTimes.size() &&
                            note.Accuracy().acceptedTicks == view.tickTimes.size() - (missTail ? 1 : 0) &&
                            log.finalizedAccuracies.size() == 1,
                        "Tail ticks must be accepted/missed and included in finalized accuracy exactly once.");
                Require(session.Update(view.endTime + rhythm::RhythmDuration{1}).events.empty(),
                        "An already finalized tail must not emit another tick or completion.");
            }
    }

    void TestLongNoteKeepsFirstHead()
    {
        chart::PatternDocument pattern;
        pattern.mode = "Taiko";
        pattern.baseBpm = 120.0;
        const auto type = static_cast<int>(mode::TaikoNoteType::TickRoll);
        const auto start = static_cast<int>(mode::TaikoPatternAction::LongNoteStart);
        const auto end = static_cast<int>(mode::TaikoPatternAction::LongNoteEnd);
        pattern.notes.push_back(chart::PatternNote{
            .position = {0, chart::Rational{0, 1}}, .keyType = type, .actionType = start});
        pattern.notes.push_back(chart::PatternNote{
            .position = {0, chart::Rational{1, 4}}, .keyType = type, .actionType = start});
        pattern.notes.push_back(chart::PatternNote{
            .position = {0, chart::Rational{2, 4}}, .keyType = type, .actionType = end});

        mode::TaikoMode taiko;
        mode::ModeLoadResult loaded = taiko.CreateSession(pattern);
        Require(loaded.Succeeded(), "Taiko mode must create the long-note session.");
        const auto &notes = loaded.session->Gear().Lanes().front()->Notes();
        Require(notes.size() == 1 && notes.front()->Timing() == rhythm::RhythmTime::zero(),
                "A duplicate LNStart must not replace the first active head.");
        Require(loaded.diagnostics.size() == 1 && loaded.diagnostics.front().severity == chart::DiagnosticSeverity::Warning,
                "Every ignored duplicate head must produce a nonfatal diagnostic.");
        pattern.notes.push_back({{0, {3, 4}}, type, end, {}, {}, {{}, 40, 1}});
        pattern.notes.push_back({{0, {1, 8}}, 1, 0, {}, {}, {{}, 50, 1}});
        pattern.notes.push_back({{0, {3, 8}}, 11, end, {}, {}, {{}, 60, 1}});
        pattern.notes.push_back({{1, {}}, type, start, {}, {}, {{}, 70, 1}});
        const auto malformed = taiko.CreateSession(pattern);
        Require(malformed.Succeeded() && malformed.diagnostics.size() == 5 &&
                    malformed.session->Gear().Lanes().front()->Notes().size() == 1,
                "Orphan/mismatched ends, swallowed taps and unclosed heads must each be diagnosed without changing "
                "play policy.");
        for (const auto line : {40u, 50u, 60u, 70u})
            Require(std::ranges::any_of(malformed.diagnostics, [line](const auto &d) { return d.location.line == line; }),
                    "Discard warnings must retain the authored source location.");
    }

    void TestTaikoUsesOneLaneForEveryNoteType()
    {
        chart::PatternDocument pattern;
        pattern.mode = "Taiko";
        pattern.baseBpm = 120.0;
        const auto down = static_cast<int>(mode::TaikoPatternAction::Down);
        const auto start = static_cast<int>(mode::TaikoPatternAction::LongNoteStart);
        const auto end = static_cast<int>(mode::TaikoPatternAction::LongNoteEnd);
        const auto add = [&pattern](const std::int64_t numerator, const mode::TaikoNoteType type,
                                    const int action) {
            pattern.notes.push_back(
                chart::PatternNote{.position = {0, chart::Rational{numerator, 8}},
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
        Require(loaded.session->Gear().LaneCount() == 1 &&
                    loaded.session->Gear().Lanes().front()->Notes().size() == 5,
                "Don, Kat, large notes and rolls must share one ordered Lane.");
        Require(loaded.session->FindNotePresentation(1)->visualKind ==
                        finger_drum::mode::NoteVisualKind::Don &&
                    loaded.session->FindNotePresentation(4)->visualKind ==
                        finger_drum::mode::NoteVisualKind::BigKat &&
                    loaded.session->FindNotePresentation(5)->visualKind ==
                        finger_drum::mode::NoteVisualKind::Roll &&
                    loaded.session->FindNotePresentation(5)->hasEndTime,
                "Every logical note must expose its mode-owned visual identity.");
    }

    void TestTaikoInputBindings()
    {
        {
            mode::PlaySession session;
            chart::EffectCommand volume;
            volume.type = chart::EffectCommandType::BusVolume;
            volume.target = "Music";
            volume.beginValue = volume.endValue = .25;
            auto ramp = volume;
            ramp.beginValue = .5;
            ramp.endValue = 1;
            ramp.curve = chart::AutomationCurve::Linear;
            auto other = volume;
            other.target = "HitSound";
            other.beginValue = other.endValue = .8;
            session.SetEffects({{volume, rhythm::RhythmTime{0}, {}},
                                {ramp, rhythm::RhythmTime{100'000}, rhythm::RhythmDuration{100'000}},
                                {other, rhythm::RhythmTime{100'000}, {}}});
            const auto &values = session.EvaluateAutomation(rhythm::RhythmTime{150'000});
            Require(values.size() == 2 && std::abs(values[0].value - .75) < 1e-9 &&
                        std::abs(values[1].value - .8) < 1e-9 &&
                        session.EvaluateAutomation(rhythm::RhythmTime{150'000}).data() == values.data(),
                    "Automation must select the latest command per target, interpolate and reuse same-time results.");
            Require(session.EvaluateAutomation(rhythm::RhythmTime{50'000}).size() == 1 &&
                        session.EvaluateAutomation(rhythm::RhythmTime{50'000})[0].value == .25 &&
                        session.EvaluateAutomation(rhythm::RhythmTime{-1}).empty(),
                    "Automation lookup must remain correct after backward seeks and before the first command.");
        }
        Require(mode::TaikoInputBindings.size() == 4,
                "Taiko must expose one input binding for every playfield key.");

        mode::TaikoMode taiko;
        for (const std::string id : {"", "unsupported"})
        {
            chart::PatternDocument invalid;
            invalid.mode = id;
            const auto rejected = taiko.CreateSession(invalid);
            Require(!rejected.Succeeded() && !rejected.session && !rejected.diagnostics.empty(),
                    "Taiko session construction must require an explicitly supported mode.");
        }
        for (std::size_t index = 0; index < mode::TaikoInputBindings.size(); ++index)
        {
            const mode::TaikoInputBinding &binding = mode::TaikoInputBindings[index];
            const std::array physicalKeys{binding.primaryKey, binding.secondaryKeys[0],
                                          binding.secondaryKeys[1]};
            Require(binding.primaryKey != 0 && binding.IsPrimary(binding.primaryKey),
                    "Each configured layout must have a primary input key.");

            chart::PatternDocument pattern;
            pattern.mode = "Taiko";
            pattern.baseBpm = 120.0;
            pattern.notes.push_back(chart::PatternNote{
                .position = {0, chart::Rational{0, 1}},
                .keyType = static_cast<int>(binding.action == mode::TaikoAction::Don
                                                ? mode::TaikoNoteType::Don
                                                : mode::TaikoNoteType::Kat),
                .actionType = static_cast<int>(mode::TaikoPatternAction::Down)});
            mode::ModeLoadResult loaded = taiko.CreateSession(pattern);
            Require(loaded.Succeeded(), "Taiko input bindings require a playable session.");
            for (const rhythm::PhysicalKey physicalKey : physicalKeys)
            {
                if (physicalKey == 0)
                {
                    continue; // Unassigned secondary slot in a customized layout.
                }
                loaded.session->Reset();
                Require(
                    HasEvent(loaded.session->ProcessInput(physicalKey, rhythm::InputEdge::Pressed,
                                                          rhythm::RhythmTime::zero()),
                             rhythm::NoteEventType::HitAccepted),
                    "Every Taiko primary and secondary key must trigger its mapped action.");
            }
        }
    }
} // namespace finger_drum::tests
