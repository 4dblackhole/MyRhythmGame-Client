#include "Submodules/TestCases.h"
#include <cstdlib>
#include <exception>
#include <functional>
#include <iostream>
#include <optional>
#include <string_view>
#include <vector>

using namespace finger_drum::tests;

int main(const int argumentCount, char *arguments[])
{
    std::optional<std::filesystem::path> catalogRoot;
    std::string_view filter;
    bool listOnly = false;
    for (int i = 1; i < argumentCount; ++i)
    {
        const std::string_view argument = arguments[i];
        if (argument == "--catalog-root" && i + 1 < argumentCount)
            catalogRoot = std::filesystem::path(arguments[++i]);
        else if (argument == "--filter" && i + 1 < argumentCount)
            filter = arguments[++i];
        else if (argument == "--list")
            listOnly = true;
        else
        {
            std::cerr << "Usage: FingerDrum.Rhythm.Tests [--catalog-root path] [--filter substring] [--list]\n";
            return EXIT_FAILURE;
        }
    }
    struct TestCase
    {
        std::string_view name;
        std::function<void()> run;
    };
    std::vector<TestCase> tests{
        {"TestYmeSyntax", TestYmeSyntax},
        {"TestSceneStateBoundaries", TestSceneStateBoundaries},
        {"TestJudgementScalingAndInterpolation", TestJudgementScalingAndInterpolation},
        {"TestRhythmTimerClockMapping", TestRhythmTimerClockMapping},
        {"TestLaneFocusRules", TestLaneFocusRules},
        {"TestLargeNoteSoundState", TestLargeNoteSoundState},
        {"TestHoldTicksAreExactlyOnce", TestHoldTicksAreExactlyOnce},
        {"TestLongNoteRemainsInScrollSnapshotUntilItsTail", TestLongNoteRemainsInScrollSnapshotUntilItsTail},
        {"TestOverlappingLongTrailIsNotHiddenByShorterNote", TestOverlappingLongTrailIsNotHiddenByShorterNote},
        {"TestTaikoRollAndTickRollRules", TestTaikoRollAndTickRollRules},
        {"TestBalloonDengDengAndBuzzRules", TestBalloonDengDengAndBuzzRules},
        {"TestTimingAccuracyAndSessionAggregation", TestTimingAccuracyAndSessionAggregation},
        {"TestPurpleNoteBothOrders", TestPurpleNoteBothOrders},
        {"TestCountedLongNoteAccuracyAndSyntax", TestCountedLongNoteAccuracyAndSyntax},
        {"TestTickAndHoldAccuracy", TestTickAndHoldAccuracy},
        {"TestDenseBuzzLateGoodHead", TestDenseBuzzLateGoodHead},
        {"TestMusicalSubdivisionTicksFollowTempo", TestMusicalSubdivisionTicksFollowTempo},
        {"TestLongNoteKeepsFirstHead", TestLongNoteKeepsFirstHead},
        {"TestTaikoUsesOneLaneForEveryNoteType", TestTaikoUsesOneLaneForEveryNoteType},
        {"TestTaikoInputBindings", TestTaikoInputBindings},
        {"TestIndexedHitSoundChanges", TestIndexedHitSoundChanges},
        {"TestSpecialNoteSoundOverridePriority", TestSpecialNoteSoundOverridePriority},
        {"TestChartEditingAndSave", TestChartEditingAndSave},
        {"TestChartEditorIndices", TestChartEditorIndices},
        {"TestRationalNumberUsesExactOrderingAndArithmetic", TestRationalNumberUsesExactOrderingAndArithmetic},
        {"TestTimingCommandWhitespaceGrammar", TestTimingCommandWhitespaceGrammar},
        {"TestAbsoluteMeasurePositionsAndTempoAnchors", TestAbsoluteMeasurePositionsAndTempoAnchors},
        {"TestScrollBeatCoordinatesAcrossTempoAndDelay", TestScrollBeatCoordinatesAcrossTempoAndDelay},
        {"TestYmpSystemBreaksAdvanceAndRemainAvailable", TestYmpSystemBreaksAdvanceAndRemainAvailable},
        {"TestOutOfMeasureEntriesAreIgnored", TestOutOfMeasureEntriesAreIgnored},
        {"TestTimingMicrosecondsAndEffects", TestTimingMicrosecondsAndEffects},
        {"TestInvalidNumericFieldsReportDiagnostics", TestInvalidNumericFieldsReportDiagnostics},
        {"TestNumericFieldsRejectEmbeddedNulls", TestNumericFieldsRejectEmbeddedNulls},
        {"TestSongCatalogIsolatesInvalidFiles", TestSongCatalogIsolatesInvalidFiles},
        {"TestChartSaveFailureRecovery", TestChartSaveFailureRecovery},
        {"TestChartParserBoundaries", TestChartParserBoundaries},
        {"TestChartPartialEdits", TestChartPartialEdits},
    };
    if (catalogRoot)
    {
        tests.push_back({"TestSongCatalog", [&] { TestSongCatalog(*catalogRoot); }});
        tests.push_back({"TestYmeEffects", [&] { TestYmeEffects(*catalogRoot); }});
        tests.push_back({"TestAngelDreamAllNotesReplay", [&] { TestAngelDreamAllNotesReplay(*catalogRoot); }});
        tests.push_back({"TestEditorAudioAnalysis", [&] { TestEditorAudioAnalysis(*catalogRoot); }});
    }
    std::size_t selected = 0, failures = 0;
    for (const auto &test : tests)
    {
        if (test.name.find(filter) == std::string_view::npos)
            continue;
        ++selected;
        if (listOnly)
        {
            std::cout << test.name << '\n';
            continue;
        }
        try
        {
            test.run();
            std::cout << "PASS " << test.name << '\n';
        }
        catch (const std::exception &error)
        {
            ++failures;
            std::cerr << "FAIL " << test.name << ": " << error.what() << '\n';
        }
        catch (...)
        {
            ++failures;
            std::cerr << "FAIL " << test.name << ": unknown exception\n";
        }
    }
    if (selected == 0)
    {
        std::cerr << "No tests matched.\n";
        return EXIT_FAILURE;
    }
    if (!listOnly)
        std::cout << selected << " tests, " << failures << " failures.\n";
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
