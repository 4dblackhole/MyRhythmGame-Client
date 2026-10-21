#pragma once

#include "PhantomCore.h"
#include "Note/Submodules/NoteTypes.h"
#include "Texts/TextCatalog.h"
#include <array>

// Owns the timing-guide and judgement-overlay presentation state. Canvas owns
// the nodes; Shutdown must run before Canvas destruction. No gameplay mutation.
class GameplayJudgementView final
{
  public:
    void Initialize(ScreenVisual2DManager &visuals, Visual2DCanvas &canvas,
                    Visual2DNode &judgementCircle, const AccuracyRange &baseRange,
                    TextCatalog &texts);
    void OnResize(float logicalWidth);
    void Present(const NoteEvent &event);
    void PresentCompletion(bool success);
    void Update(double deltaSeconds);
    void Reset() noexcept;
    void Shutdown() noexcept;

  private:
    static constexpr double FeedbackSeconds = 0.2;
    void CreateTimingGuide(Visual2DNode &root, TextCatalog &texts);
    void CreateTimingBands();
    void CreateTimingCaptions(TextCatalog &texts);
    void CreateTimingMarker();
    void CreateJudgementImages(ScreenVisual2DManager &visuals,
                               Visual2DNode &judgementCircle);
    struct TimingMarker
    {
        Visual2DNode *node{}, *fill{};
        RhythmDuration error{};
        double fadeElapsed{FeedbackSeconds};
    };
    void UpdateMarkerPositions();
    void AddMarker(RhythmDuration);
    void PositionMarker(TimingMarker &);
    void TintMarker(TimingMarker &, float alpha);
    void ShowImage(std::size_t);

    std::array<RhythmDuration, AccuracyRange::BandCount> halfWindows_{};
    RhythmDuration displayHalfWindow_{};
    float pixelsPerMicrosecond_{};
    Visual2DNode *guide_{};
    Visual2DNode *margin_{};
    std::array<Visual2DNode *, AccuracyRange::BandCount> bands_{};
    std::array<Visual2DNode *, 3> labels_{};
    // Fixed circular buffers: 32 retained hits plus at most 8 retiring hits.
    std::array<TimingMarker, 32> markers_{};
    std::array<TimingMarker, 8> retiringMarkers_{};
    std::size_t nextMarker_{}, markerCount_{}, nextRetiring_{};
    std::array<Visual2DNode *, 8> judgementImages_{};
    Visual2DNode *activeJudgement_{};
    double judgementElapsedSeconds_{FeedbackSeconds};
};
