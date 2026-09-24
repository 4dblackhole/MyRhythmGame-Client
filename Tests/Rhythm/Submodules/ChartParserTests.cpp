#include "TestSupport.h"

namespace finger_drum::tests
{
    [[nodiscard]] chart::ParseResult<chart::PatternDocument> ParseTimingBody(
        const std::string_view timingBody)
    {
        const std::string pattern = "[Metadata]\nBase BPM: 120\n"
                                    "[Time Signature]\n" +
                                    std::string(timingBody) + "\n[Pattern]\n0/1,1,0\n";
        return chart::ChartParser{}.ParsePattern(pattern, "memory.ymp");
    }

    void TestRationalNumberUsesExactOrderingAndArithmetic()
    {
        Require(chart::RationalNumber{3, 8} < chart::RationalNumber{1, 2} &&
                    chart::RationalNumber{4, 8} == chart::RationalNumber{1, 2} &&
                    chart::RationalNumber{1, 6} + chart::RationalNumber{1, 3} ==
                        chart::RationalNumber{1, 2} &&
                    chart::RationalNumber{-1, 2} < chart::RationalNumber{-3, 8},
                "RationalNumber must compare and add reduced fractions exactly.");

        bool rejectedZeroDenominator = false;
        try
        {
            static_cast<void>(chart::RationalNumber{1, 0});
        }
        catch (const std::invalid_argument &)
        {
            rejectedZeroDenominator = true;
        }
        Require(rejectedZeroDenominator, "RationalNumber must reject a zero denominator.");
    }

    void TestTimingCommandWhitespaceGrammar()
    {
        Require(ParseTimingBody("1/2, #bpm 150").Succeeded() &&
                    ParseTimingBody("1/2,        #bpm   150").Succeeded() &&
                    ParseTimingBody("#measure 4/4").Succeeded() &&
                    ParseTimingBody("#measure C").Succeeded(),
                "Timing grammar must allow whitespace after commas and between a command and its "
                "value.");

        Require(!ParseTimingBody("1/ 2, #bpm 150").Succeeded() &&
                    !ParseTimingBody("1/2, #bpm150").Succeeded() &&
                    !ParseTimingBody("#measure4/4").Succeeded() &&
                    !ParseTimingBody("# measure 4/4").Succeeded() &&
                    !ParseTimingBody("#measure 4/  4").Succeeded(),
                "Timing grammar must reject whitespace inside fractions and commands without a "
                "separating space.");
    }

    void TestAbsoluteMeasurePositionsAndTempoAnchors()
    {
        constexpr std::string_view Pattern = R"(
[Metadata]
Base BPM: 140
[Time Signature]
#measure 3/4
1/2,        #bpm   150
--
#measure 4/4
0/1, #bpm 160
[Pattern]
1/2,1,0
--
0/1,2,0
1/4,1,0
)";
        const auto parsed = chart::ChartParser{}.ParsePattern(Pattern, "tempo-example.ymp");
        Require(parsed.Succeeded() && parsed.document.notes.size() == 3,
                "The 3/4 to 4/4 tempo example must parse without errors.");

        const chart::MusicalTimeline timeline(parsed.document);
        const auto compiled = timeline.CompileNotes(parsed.document);
        Require(compiled[0].timing == rhythm::RhythmTime{857'143} &&
                    compiled[1].timing == rhythm::RhythmTime{1'257'143} &&
                    compiled[2].timing == rhythm::RhythmTime{1'632'143},
                "N/D must be an absolute whole-note position inside its measure and BPM anchors "
                "must accumulate from the previous anchor.");
    }

    void TestScrollBeatCoordinatesAcrossTempoAndDelay()
    {
        chart::PatternDocument pattern;
        pattern.baseBpm = 180.0;
        pattern.timing.push_back({{0, {1, 2}}, chart::TimingDirectiveType::Bpm, 90.0});
        pattern.timing.push_back({{0, {3, 4}}, chart::TimingDirectiveType::DelayMilliseconds,
                                  500.0});
        const chart::MusicalTimeline timeline(pattern);
        const auto beatAt = [&timeline](const chart::Rational fraction)
        {
            return timeline.WholeNotesAtTime(timeline.Compile({0, fraction}));
        };
        constexpr long double tolerance = 0.000001L;
        Require(std::abs((beatAt({1, 4}) - beatAt({3, 16})) - 0.0625L) < tolerance &&
                    std::abs((beatAt({9, 16}) - beatAt({1, 2})) - 0.0625L) < tolerance,
                "Sixteenth-note scroll distance must be constant across BPM changes.");
        const auto delayedBeatTime = timeline.Compile({0, {3, 4}});
        Require(std::abs(timeline.WholeNotesAtTime(delayedBeatTime -
                         rhythm::RhythmDuration{250'000}) - 0.75L) < tolerance &&
                    std::abs(timeline.WholeNotesAtTime(delayedBeatTime) - 0.75L) < tolerance,
                "A positive chart delay must hold scroll position until its beat is reached.");
        Require(timeline.TimeAtWholeNotes(0.5625L) == timeline.Compile({0, {9, 16}}) &&
                    std::abs(timeline.WholeNotesAtTime(
                        timeline.TimeAtWholeNotes(-0.25L)) + 0.25L) < tolerance,
                "Scroll coordinates must map to chart time and pre-roll time.");
    }

    void TestYmpSystemBreaksAdvanceAndRemainAvailable()
    {
        constexpr std::string_view Pattern = R"(
[Metadata]
Base BPM: 120
[Time Signature]
---
#measure 3/4
[Pattern]
0/4,1,0
---
0/4,2,0
--
0/4,1,0
---
0/4,2,0
)";
        const auto parsed = chart::ChartParser{}.ParsePattern(Pattern, "system-breaks.ymp");
        Require(parsed.Succeeded() && parsed.document.notes.size() == 4 &&
                    parsed.document.notes[0].position.measure == 0 &&
                    parsed.document.notes[1].position.measure == 1 &&
                    parsed.document.notes[2].position.measure == 2 &&
                    parsed.document.notes[3].position.measure == 3,
                "Both YMP measure separators must advance the current measure.");
        Require(parsed.document.systemBreakMeasures == std::vector<std::int64_t>{1, 3},
                "The parser must preserve each unique measure that starts after "
                "a triple-dash system break.");
    }

    void TestOutOfMeasureEntriesAreIgnored()
    {
        constexpr std::string_view Pattern = R"(
[Metadata]
Base BPM: 120
[Time Signature]
#measure 2/4
3/4, #bpm 180
[Pattern]
1/4,1,0
3/4,2,0
)";
        const auto parsed = chart::ChartParser{}.ParsePattern(Pattern, "out-of-measure.ymp");
        Require(parsed.Succeeded() && parsed.document.notes.size() == 1 &&
                    std::ranges::none_of(parsed.document.timing,
                                         [](const chart::TimingDirective &directive) {
                                             return directive.type ==
                                                    chart::TimingDirectiveType::Bpm;
                                         }) &&
                    std::ranges::count_if(parsed.diagnostics,
                                          [](const chart::Diagnostic &diagnostic) {
                                              return diagnostic.severity ==
                                                     chart::DiagnosticSeverity::Warning;
                                          }) == 2,
                "Notes and timing directives at or beyond the current measure length must be "
                "ignored with warnings.");
    }

    void TestInvalidNumericFieldsReportDiagnostics()
    {
        chart::ChartParser parser;
        const auto pattern =
            parser.ParsePattern("Version: nope\nBase BPM: 120\nJudgeLevel: 0\n", "invalid.ymp");
        Require(!pattern.Succeeded() && pattern.diagnostics.size() >= 2,
                "Invalid YMP integer fields must produce parse diagnostics.");

        const auto effects = parser.ParseEffect("Version: nope\n[AudioAutomation]\n"
                                                "0/1,#BusVolume,HitSound,invalid,0.5,-1,Linear\n",
                                                "invalid.yme");
        Require(!effects.Succeeded() && effects.document.commands.empty(),
                "Invalid YME numeric fields must be diagnosed and excluded.");
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
        Require(compiled[0].timing == rhythm::RhythmTime{500'000} &&
                    compiled[1].timing == rhythm::RhythmTime{1'000'000},
                "Legacy #measure must preserve absolute in-measure positions and exact microsecond "
                "timestamps.");
        Require(std::ranges::any_of(parsed.diagnostics,
                                    [](const chart::Diagnostic &diagnostic) {
                                        return diagnostic.severity ==
                                               chart::DiagnosticSeverity::Warning;
                                    }),
                "Legacy visual directives must remain loadable and report migration warnings.");

        chart::MusicalPosition extendedPosition;
        Require(chart::TryParseMusicalPosition("18/8", 3, extendedPosition) &&
                    extendedPosition.fraction == chart::Rational{18, 8},
                "Legacy extended measures must accept positions beyond one whole.");

        constexpr std::string_view Effects = R"(
Version: 1
[AudioAutomation]
0/1,#BusVolume,HitSound,1.0,0.5,500,Linear
1/2,#ReverbSend,HitSound,0.0,0.8,1000,Smoothstep
)";
        const chart::ParseResult<chart::EffectDocument> effects =
            parser.ParseEffect(Effects, "memory.yme");
        Require(effects.Succeeded() && effects.document.commands.size() == 2 &&
                    effects.document.commands[1].type == chart::EffectCommandType::ReverbSend,
                "YME must parse real-time audio automation independently of YMP.");
    }
} // namespace finger_drum::tests
