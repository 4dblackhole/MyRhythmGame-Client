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
            compiled[0].timing == rhythm::RhythmTime{250'000} &&
            compiled[1].timing == rhythm::RhythmTime{1'000'000},
            "Legacy #measure must scale positions to exact microsecond timestamps.");
        Require(
            std::ranges::any_of(
                parsed.diagnostics,
                [](const chart::Diagnostic& diagnostic)
                {
                    return diagnostic.severity ==
                        chart::DiagnosticSeverity::Warning;
                }),
            "Legacy visual directives must remain loadable and report migration warnings.");

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

int main()
{
    try
    {
        TestJudgementScalingAndInterpolation();
        TestRhythmTimerClockMapping();
        TestLaneFocusRules();
        TestLargeNoteSoundState();
        TestHoldTicksAreExactlyOnce();
        TestLongNoteKeepsFirstHead();
        TestLegacyParsingAndMicroseconds();
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
