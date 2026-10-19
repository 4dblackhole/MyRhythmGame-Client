#pragma once

#include "MRG_Core.h"
#include "Note/Submodules/NoteTypes.h"
#include "Texts/TextCatalog.h"
#include <array>

// Owns the timing-guide and judgement-overlay presentation state. Canvas owns
// the nodes; Shutdown must run before Canvas destruction. No gameplay mutation.
class GameplayJudgementView final
{
  public:
    void Initialize(mrg::visual2d::ScreenVisual2DManager &visuals, mrg::visual2d::Visual2DCanvas &canvas,
                    mrg::visual2d::Visual2DNode &judgementCircle, const finger_drum::rhythm::AccuracyRange &baseRange,
                    finger_drum::texts::TextCatalog &texts);
    void OnResize(float logicalWidth);
    void Present(const finger_drum::rhythm::NoteEvent &event);
    void PresentCompletion(bool success);
    void Update(double deltaSeconds);
    void Reset() noexcept;
    void Shutdown() noexcept;

  private:
    static constexpr double FeedbackSeconds = 0.2;
    void CreateTimingGuide(mrg::visual2d::Visual2DNode &root, finger_drum::texts::TextCatalog &texts);
    void CreateTimingBands();
    void CreateTimingCaptions(finger_drum::texts::TextCatalog &texts);
    void CreateTimingMarker();
    void CreateJudgementImages(mrg::visual2d::ScreenVisual2DManager &visuals,
                               mrg::visual2d::Visual2DNode &judgementCircle);
    struct TimingMarker
    {
        mrg::visual2d::Visual2DNode *node{}, *fill{};
        finger_drum::rhythm::RhythmDuration error{};
        double fadeElapsed{FeedbackSeconds};
    };
    void UpdateMarkerPositions();
    void AddMarker(finger_drum::rhythm::RhythmDuration);
    void PositionMarker(TimingMarker &);
    void TintMarker(TimingMarker &, float alpha);
    void ShowImage(std::size_t);

    std::array<finger_drum::rhythm::RhythmDuration, finger_drum::rhythm::AccuracyRange::BandCount> halfWindows_{};
    finger_drum::rhythm::RhythmDuration displayHalfWindow_{};
    float pixelsPerMicrosecond_{};
    mrg::visual2d::Visual2DNode *guide_{};
    mrg::visual2d::Visual2DNode *margin_{};
    std::array<mrg::visual2d::Visual2DNode *, finger_drum::rhythm::AccuracyRange::BandCount> bands_{};
    std::array<mrg::visual2d::Visual2DNode *, 3> labels_{};
    // Fixed circular buffers: 32 retained hits plus at most 8 retiring hits.
    std::array<TimingMarker, 32> markers_{};
    std::array<TimingMarker, 8> retiringMarkers_{};
    std::size_t nextMarker_{}, markerCount_{}, nextRetiring_{};
    std::array<mrg::visual2d::Visual2DNode *, 8> judgementImages_{};
    mrg::visual2d::Visual2DNode *activeJudgement_{};
    double judgementElapsedSeconds_{FeedbackSeconds};
};
