#include "Submodules/TestCases.h"
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string_view>

using namespace finger_drum::tests;

int main(const int argumentCount, char *arguments[])
{
    try
    {
        TestSceneStateBoundaries();
        TestJudgementScalingAndInterpolation();
        TestRhythmTimerClockMapping();
        TestLaneFocusRules();
        TestLargeNoteSoundState();
        TestHoldTicksAreExactlyOnce();
        TestLongNoteRemainsInScrollSnapshotUntilItsTail();
        TestOverlappingLongTrailIsNotHiddenByShorterNote();
        TestTaikoRollAndTickRollRules();
        TestBalloonDengDengAndBuzzRules();
        TestTimingAccuracyAndSessionAggregation();
        TestPurpleNoteBothOrders();
        TestCountedLongNoteAccuracyAndSyntax();
        TestTickAndHoldAccuracy();
        TestMusicalSubdivisionTicksFollowTempo();
        TestLongNoteKeepsFirstHead();
        TestTaikoUsesOneLaneForEveryNoteType();
        TestTaikoInputBindings();
        TestIndexedHitSoundChanges();
        TestSpecialNoteSoundOverridePriority();
        TestChartEditingAndSave();
        TestRationalNumberUsesExactOrderingAndArithmetic();
        TestTimingCommandWhitespaceGrammar();
        TestAbsoluteMeasurePositionsAndTempoAnchors();
        TestYmpSystemBreaksAdvanceAndRemainAvailable();
        TestOutOfMeasureEntriesAreIgnored();
        TestLegacyParsingAndMicroseconds();
        TestInvalidNumericFieldsReportDiagnostics();
        TestSongCatalogIsolatesInvalidFiles();
        if (argumentCount == 3 && std::string_view(arguments[1]) == "--catalog-root")
        {
            TestSongCatalog(std::filesystem::path(arguments[2]));
            TestEditorAudioAnalysis(std::filesystem::path(arguments[2]));
        }
        std::cout << "FingerDrum rhythm tests passed.\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception &exception)
    {
        std::cerr << "FingerDrum rhythm test failure: " << exception.what() << '\n';
        return EXIT_FAILURE;
    }
}
