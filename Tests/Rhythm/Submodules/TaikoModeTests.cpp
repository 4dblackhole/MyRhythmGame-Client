#include "TestSupport.h"

namespace finger_drum::tests
{
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
        Require(tickRoll.session->FindNotePresentation(1)->tickTimes.size() == 8,
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
        Require(ticks == std::vector<rhythm::RhythmTime>{rhythm::RhythmTime{0},
                                                         rhythm::RhythmTime{500'000}},
                "Musical subdivision ticks must follow BPM changes.");
        Require(timeline.CountSubdivisions({0, chart::Rational{0, 1}}, {0, chart::Rational{1, 8}},
                                           12) == 2,
                "Subdivision counts must use exact rational ceil(length * divisions).");
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
        Require(mode::TaikoInputBindings.size() == 4,
                "Taiko must expose one input binding for every playfield key.");

        mode::TaikoMode taiko;
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
