#pragma once
#include <filesystem>

namespace finger_drum::tests
{
    void TestYmeSyntax();
    void TestYmeEffects(const std::filesystem::path &songsRoot);
    void TestSceneStateBoundaries();
    void TestJudgementScalingAndInterpolation();
    void TestRhythmTimerClockMapping();
    void TestLaneFocusRules();
    void TestLargeNoteSoundState();
    void TestHoldTicksAreExactlyOnce();
    void TestLongNoteRemainsInScrollSnapshotUntilItsTail();
    void TestOverlappingLongTrailIsNotHiddenByShorterNote();
    void TestTaikoRollAndTickRollRules();
    void TestBalloonDengDengAndBuzzRules();
    void TestTimingAccuracyAndSessionAggregation();
    void TestPurpleNoteBothOrders();
    void TestCountedLongNoteAccuracyAndSyntax();
    void TestTickAndHoldAccuracy();
    void TestDenseBuzzLateGoodHead();
    void TestAngelDreamAllNotesReplay(const std::filesystem::path &songsRoot);
    void TestMusicalSubdivisionTicksFollowTempo();
    void TestLongNoteKeepsFirstHead();
    void TestTaikoUsesOneLaneForEveryNoteType();
    void TestTaikoInputBindings();
    void TestRationalNumberUsesExactOrderingAndArithmetic();
    void TestTimingCommandWhitespaceGrammar();
    void TestAbsoluteMeasurePositionsAndTempoAnchors();
    void TestScrollBeatCoordinatesAcrossTempoAndDelay();
    void TestYmpSystemBreaksAdvanceAndRemainAvailable();
    void TestOutOfMeasureEntriesAreIgnored();
    void TestSongCatalog(const std::filesystem::path &songsRoot);
    void TestSongCatalogIsolatesInvalidFiles();
    void TestInvalidNumericFieldsReportDiagnostics();
    void TestNumericFieldsRejectEmbeddedNulls();
    void TestLegacyParsingAndMicroseconds();
    void TestIndexedHitSoundChanges();
    void TestSpecialNoteSoundOverridePriority();
    void TestChartEditingAndSave();
    void TestEditorAudioAnalysis(const std::filesystem::path &songsRoot);
} // namespace finger_drum::tests
