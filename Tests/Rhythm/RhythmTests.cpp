#include "Submodules/TestCases.h"
#include <cstdlib>
#include <cstdio>
#include <exception>
#include <iostream>
#include <string_view>

using namespace finger_drum::tests;

int main(const int argumentCount, char *arguments[])
{
    try
    {
        TestYmeSyntax();
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
        TestDenseBuzzLateGoodHead();
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
        TestScrollBeatCoordinatesAcrossTempoAndDelay();
        TestYmpSystemBreaksAdvanceAndRemainAvailable();
        TestOutOfMeasureEntriesAreIgnored();
        TestLegacyParsingAndMicroseconds();
        TestInvalidNumericFieldsReportDiagnostics();
        TestNumericFieldsRejectEmbeddedNulls();
        TestSongCatalogIsolatesInvalidFiles();
        if (argumentCount == 3 && std::string_view(arguments[1]) == "--catalog-root")
        {
            TestSongCatalog(std::filesystem::path(arguments[2]));
            TestYmeEffects(std::filesystem::path(arguments[2]));
            TestAngelDreamAllNotesReplay(std::filesystem::path(arguments[2]));
            TestEditorAudioAnalysis(std::filesystem::path(arguments[2]));
        }
        std::cout << "FingerDrum rhythm tests passed.\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception &exception)
    {
        std::fprintf(stderr, "FingerDrum rhythm test failure: %s\n", exception.what());
        return EXIT_FAILURE;
    }
    catch (...)
    {
        std::fputs("FingerDrum rhythm test failure: unknown exception\n", stderr);
        return EXIT_FAILURE;
    }
}
