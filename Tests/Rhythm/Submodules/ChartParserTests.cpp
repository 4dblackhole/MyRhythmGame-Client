#include "Taiko/Submodules/TaikoAudioPreview.h"
#include "Taiko/Submodules/TaikoChartNotes.h"
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

    void TestDelayTimeAnchors()
    {
        chart::PatternDocument pattern;
        pattern.mode = "Taiko";
        pattern.baseBpm = 180;
        pattern.notes = {{{0, {}}, 1}, {{1, {}}, 2}, {{2, {}}, 1}};
        for (const double delay : {-300, 300})
        {
            pattern.timing = {{{1, {}}, chart::TimingDirectiveType::DelayMilliseconds, delay}};
            const chart::MusicalTimeline timeline(pattern);
            const auto starts = timeline.CompileMeasureStarts();
            Require(starts[1] == rhythm::RhythmTime{static_cast<long long>(1'333'333 + delay * 1000)} &&
                        starts[2] == rhythm::RhythmTime{static_cast<long long>(2'666'667 + delay * 1000)} &&
                        timeline.MeasureLength(0) == chart::Rational{1, 1} &&
                        timeline.PositionToWholeNotes({1, {}}) == chart::Rational{1, 1},
                    "Boundary delay must change only the preceding measure's elapsed duration, never source beats.");
            const auto loaded = mode::TaikoMode{}.CreateSession(pattern);
            Require(loaded.Succeeded() && loaded.session->Timeline().Compile({1, {}}) == starts[1],
                    "Gameplay and the shared measure prefix must use the same delayed microseconds.");
        }
        const auto closed = chart::ChartParser{}.ParsePattern(R"([Metadata]
Base BPM: 180
Mode: Taiko
[Time Signature]
1/1,#delay -300
--
#measure 3/4
[Pattern]
0/1,1,0
--
0/1,2,0
)");
        Require(closed.Succeeded() && closed.document.timing.size() == 2 &&
                    chart::MusicalTimeline(closed.document).Compile({1, {}}) == rhythm::RhythmTime{1'033'333},
                "The closed measure end and next measure start must denote the same delay boundary.");
        chart::ChartEditor editor(closed.document);
        const auto saved = chart::ChartParser{}.ParsePattern(chart::ChartEditor::WritePattern(editor.Pattern()));
        Require(saved.Succeeded() && saved.document.timing.front().position.fraction == chart::Rational{1, 1} &&
                    saved.document.notes[1].position == chart::MusicalPosition{1, {}},
                "Saving must preserve authored musical delay and note coordinates.");
    }

    void TestNegativeDelayOverwritesEvents()
    {
        chart::PatternDocument pattern;
        pattern.mode = "Taiko";
        pattern.baseBpm = 120;
        pattern.notes = {{{0, {1, 4}}, 1}, {{0, {1, 2}}, 1}, {{0, {3, 4}}, 1},
                         {{1, {}}, 2},     {{1, {1, 4}}, 1}, {{2, {}}, 1}};
        for (std::size_t i = 0; i < pattern.notes.size(); ++i)
            pattern.notes[i].sourceOrder = i;
        pattern.timing = {{{1, {1, 4}}, chart::TimingDirectiveType::DelayMilliseconds, -1500}};
        chart::ChartEditor editor(pattern);
        const auto &timeline = editor.Timeline();
        const auto first = timeline.MeasureSpan(0), second = timeline.MeasureSpan(1), third = timeline.MeasureSpan(2);
        Require(first.begin == rhythm::RhythmTime{} && first.end == rhythm::RhythmTime{1'000'000} &&
                    first.wholeNotes == .5L && second.begin == first.end &&
                    second.end == rhythm::RhythmTime{2'500'000} && second.wholeNotes == .75L && third.wholeNotes == 1,
                "A -3/4 delay at measure 2's 1/4 must leave measure lengths 2/4, 3/4, then 4/4.");
        Require(timeline.Compile({1, {1, 4}}) == second.begin &&
                    second.sourceBegin == chart::MusicalPosition{1, {1, 4}} &&
                    timeline.PositionToWholeNotes({1, {1, 4}}) == chart::Rational{5, 4},
                "The delayed note must appear at the effective measure start without changing its beat.");
        const auto playable = mode::taiko_notes::PlayableInSourceOrder(editor.Notes());
        auto play = mode::TaikoMode{}.CreateSession(editor.Pattern());
        Require(play.Succeeded() && playable.size() == 3 && play.session->Gear().Lanes().front()->Notes().size() == 3 &&
                    !timeline.IsPositionActive({0, {1, 2}}) && !timeline.IsPositionActive({1, {}}) &&
                    timeline.IsPositionActive({1, {1, 4}}),
                "The overwritten interval includes its earlier boundary, excludes its new endpoint, and removes taps.");
        const auto ticks = timeline.CompileSubdivisions({0, {}}, {2, {}}, 16);
        std::vector<rhythm::RhythmTime> expected;
        for (int i = 0; i < 32; ++i)
        {
            const chart::MusicalPosition position{i / 16, {i % 16, 16}};
            if (timeline.IsPositionActive(position))
                expected.push_back(timeline.Compile(position));
        }
        Require(ticks == expected && timeline.CountSubdivisions({0, {}}, {2, {}}, 16) == ticks.size(),
                "Tick generation and counts without timestamp allocation must skip exactly the overwritten grid "
                "positions.");
        const auto restored = chart::ChartParser{}.ParsePattern(chart::ChartEditor::WritePattern(editor.Pattern()));
        Require(restored.Succeeded() && restored.document.notes.size() == 6 &&
                    restored.document.notes[4].position == chart::MusicalPosition{1, {1, 4}} &&
                    restored.document.timing.front().value == -1500,
                "Saving must retain overwritten notes, delays and original beat positions.");
        const auto id = pattern.notes[2].sourceOrder;
        Require(editor.FindNote(id) && !editor.FindNote(id)->head.positionActive,
                "An inactive source node must retain its stable ID and historical timestamp.");
        editor.AddNote({0, {7, 8}}, 1);
        editor.ReplaceTiming({});
        Require(std::ranges::all_of(editor.Notes(), &chart::CompiledPatternNote::positionActive),
                "Removing the delay must restore source nodes and editor time indices.");
        editor.ReplaceTiming(pattern.timing);
        Require(!editor.FindNote(id)->head.positionActive,
                "Timing replacement must rebuild visibility without replacing source ownership.");

        // A later cut removes an earlier BPM and negative delay, restoring the
        // Base BPM instead of using the discarded tempo for subsequent notes.
        pattern.notes = {{{2, {}}, 1}};
        pattern.timing = {{{0, {1, 2}}, chart::TimingDirectiveType::Bpm, 240},
                          {{0, {3, 4}}, chart::TimingDirectiveType::DelayMilliseconds, -100},
                          {{1, {1, 4}}, chart::TimingDirectiveType::DelayMilliseconds, -1250}};
        const chart::MusicalTimeline nested(pattern);
        Require(nested.Compile({1, {1, 4}}) == rhythm::RhythmTime{400'000} &&
                    nested.Compile({2, {}}) == rhythm::RhythmTime{1'900'000} && !nested.IsPositionActive({0, {1, 2}}) &&
                    !nested.IsPositionActive({0, {3, 4}}) &&
                    std::abs(nested.WholeNotesAtTime(rhythm::RhythmTime{400'000}) - 1.25L) < 1e-12,
                "Nested overwrites must discard earlier tempo/delay state and support inverse seeking.");
        const auto nestedTicks = nested.CompileSubdivisions({0, {}}, {2, {}}, 16);
        expected.clear();
        for (int i = 0; i < 32; ++i)
        {
            const chart::MusicalPosition p{i / 16, {i % 16, 16}};
            if (nested.IsPositionActive(p))
                expected.push_back(nested.Compile(p));
        }
        Require(nestedTicks == expected, "Nested cuts must share the same source visibility test as grid ticks.");

        pattern.timing = {{{0, {1, 2}}, chart::TimingDirectiveType::Bpm, 240},
                          {{0, {1, 2}}, chart::TimingDirectiveType::DelayMilliseconds, 1000},
                          {{0, {3, 4}}, chart::TimingDirectiveType::DelayMilliseconds, -750}};
        const chart::MusicalTimeline gap(pattern);
        Require(!gap.IsPositionActive({0, {1, 2}}) && gap.Compile({0, {3, 4}}) == rhythm::RhythmTime{1'500'000} &&
                    gap.Compile({1, {}}) == rhythm::RhythmTime{2'000'000} && gap.MeasureSpan(0).wholeNotes == 1,
                "A cut inside a positive gap must retain elapsed remainder and restore the BPM before the gap.");
        // Same-position BPM is a new event at the surviving cut endpoint.
        pattern.timing.push_back({{0, {3, 4}}, chart::TimingDirectiveType::Bpm, 180});
        const chart::MusicalTimeline simultaneous(pattern);
        Require(simultaneous.Compile({1, {}}) == rhythm::RhythmTime{1'833'333},
                "BPM at the negative delay's own position must remain active after prior tempo state is removed.");

        pattern.timing = {{{0, {1, 2}}, chart::TimingDirectiveType::Bpm, 240},
                          {{0, {1, 2}}, chart::TimingDirectiveType::DelayMilliseconds, 1000},
                          {{1, {1, 4}}, chart::TimingDirectiveType::DelayMilliseconds, -1250}};
        const chart::MusicalTimeline gapAcrossBar(pattern);
        Require(gapAcrossBar.MeasureSpan(0).wholeNotes == .75L && gapAcrossBar.MeasureSpan(1).wholeNotes == .75L &&
                    gapAcrossBar.MeasureSpan(0).end == rhythm::RhythmTime{1'500'000} &&
                    gapAcrossBar.MeasureSpan(1).end == rhythm::RhythmTime{3'000'000},
                "A remaining positive gap belongs to its original closed-end measure even after a cross-bar cut.");

        // Entirely removed measures have zero visible duration, no duplicate
        // bar line, and remain authored 4/4 in the score file.
        pattern.timing = {{{2, {1, 4}}, chart::TimingDirectiveType::DelayMilliseconds, -4000}};
        pattern.notes = {{{0, {}}, 1}, {{1, {}}, 1}, {{2, {1, 4}}, 1}, {{3, {}}, 1}};
        const chart::MusicalTimeline large(pattern);
        Require(large.MeasureSpan(0).wholeNotes == .25L && large.MeasureSpan(1).wholeNotes == 0 &&
                    large.MeasureSpan(2).wholeNotes == .75L && large.CompileMeasureStarts().size() == 3 &&
                    large.MeasureLength(1) == chart::Rational{1, 1},
                "A cross-measure overwrite must collapse complete bars without modifying source signatures.");
        pattern.timing = {{{0, {1, 4}}, chart::TimingDirectiveType::DelayMilliseconds, -750}};
        const chart::MusicalTimeline negativeStart(pattern);
        Require(negativeStart.Compile({0, {1, 4}}) == rhythm::RhythmTime{-250'000} &&
                    std::abs(negativeStart.WholeNotesAtTime(rhythm::RhythmTime{-250'000}) - .25L) < 1e-12,
                "A jump before the chart origin must retain a valid clock and latest source anchor.");
        pattern.timing = {{{0, {1, 2}}, chart::TimingDirectiveType::Bpm, 240},
                          {{1, {1, 4}}, chart::TimingDirectiveType::DelayMilliseconds, -2000}};
        const chart::MusicalTimeline cutBeforeOrigin(pattern);
        Require(cutBeforeOrigin.Compile({1, {1, 4}}) == rhythm::RhythmTime{-250'000} &&
                    std::abs(cutBeforeOrigin.WholeNotesAtTime(rhythm::RhythmTime{-500'000}) + .25L) < 1e-12 &&
                    cutBeforeOrigin.TimeAtWholeNotes(-.25L) == rhythm::RhythmTime{-500'000},
                "Pre-roll seeking must use Base BPM after a large cut discards all authored tempo anchors.");
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

    void TestNumericFieldsRejectEmbeddedNulls()
    {
        const chart::ChartParser parser;
        const std::filesystem::path source{"numeric-fields.ymp"};
        for (const std::string &number : {std::string{"120garbage"}, std::string{""},
                                        std::string{"120\0garbage", 11}, std::string{"120\0", 4},
                                        std::string(1, '\0') + "120"})
        {
            for (const std::string_view prefix : {"[Metadata]\nBase BPM: ", "[Metadata]\nPattern Offset: ",
                                                  "[Time Signature]\n0/4,#bpm ", "[Time Signature]\n0/4,#delay "})
            {
                const auto parsed = parser.ParsePattern(std::string(prefix) + number + "\n", source);
                Require(!parsed.Succeeded() && !parsed.diagnostics.empty() && parsed.document.sourcePath == source &&
                            parsed.diagnostics.front().location.file == source,
                        "Invalid numeric fields must retain their source and report a diagnostic, including embedded NULs.");
            }
            const auto effect = parser.ParseEffect("[Sounds]\n1,0/4,#Volume HitSound,Value=" + number + "\n", "numeric-fields.yme");
            Require(!effect.Succeeded() && effect.document.commands.empty(),
                    "YME numeric values must reject the complete invalid field, not just a prefix before NUL.");
        }
        for (const std::string_view number : {"120", "+120", "1.2e2", "0x1.ep6", " \t120 \t"})
        {
            const auto parsed = parser.ParsePattern("[Metadata]\nBase BPM: " + std::string(number) + "\n", source);
            Require(parsed.Succeeded() && parsed.document.baseBpm == 120 && parsed.document.sourcePath == source,
                    "Numeric validation must preserve decimal, sign, exponent, hex and surrounding-whitespace syntax.");
        }
    }

    void TestTimingMicrosecondsAndEffects()
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
[Pattern]
1/4,1,0
--
0/1,2,0
)";
        chart::ChartParser parser;
        const chart::ParseResult<chart::PatternDocument> parsed =
            parser.ParsePattern(Pattern, "memory.ymp");
        Require(parsed.Succeeded() && parsed.document.notes.size() == 2,
                "YMP notes and measure separators must parse.");
        chart::MusicalTimeline timeline(parsed.document);
        const auto compiled = timeline.CompileNotes(parsed.document);
        Require(compiled[0].timing == rhythm::RhythmTime{500'000} &&
                    compiled[1].timing == rhythm::RhythmTime{1'000'000},
                "#measure must preserve absolute in-measure positions and exact microsecond "
                "timestamps.");
        for (const std::string_view command : {"#measureLineVisible OFF", "0/4,#measureLineVisible OFF",
                                               "#scroll 2", "0/4,#volume 0.5"})
        {
            const auto unsupported = parser.ParsePattern("[Time Signature]\n" + std::string(command), "unsupported.ymp");
            Require(!unsupported.Succeeded() && !unsupported.diagnostics.empty() &&
                        unsupported.diagnostics.front().severity == chart::DiagnosticSeverity::Error &&
                        unsupported.diagnostics.front().location.line == 2,
                    "Unsupported YMP effects must fail rather than silently discard automation.");
        }

        chart::MusicalPosition extendedPosition;
        Require(chart::TryParseMusicalPosition("18/8", 3, extendedPosition) &&
                    extendedPosition.fraction == chart::Rational{18, 8},
                "Extended measures must accept positions beyond one whole.");

        constexpr std::string_view Effects = R"(
Version: 1
[Sounds]
1, 0/4, Area, 1, 1/4, #Volume HitSound, From=1, To=0.5, Curve=Linear
1, 1/2, Area, 2, 0/4, #ReverbSend HitSound, From=0, To=0.8, Curve=Linear
)";
        const chart::ParseResult<chart::EffectDocument> effects =
            parser.ParseEffect(Effects, "memory.yme");
        Require(effects.Succeeded() && effects.document.commands.size() == 2 &&
                    effects.document.commands[1].type == chart::EffectCommandType::ReverbSend,
                "YME must parse real-time audio automation independently of YMP.");
    }
} // namespace finger_drum::tests
