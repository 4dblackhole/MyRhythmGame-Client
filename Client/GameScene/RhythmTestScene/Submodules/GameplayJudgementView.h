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
    void Initialize(mrg::visual2d::ScreenVisual2DManager &visuals,
                    mrg::visual2d::Visual2DCanvas &canvas,
                    mrg::visual2d::Visual2DNode &judgementCircle,
                    const finger_drum::rhythm::AccuracyRange &baseRange,
                    finger_drum::texts::TextCatalog &texts);
    void OnResize(float logicalWidth);
    void Present(const finger_drum::rhythm::NoteEvent &event);
    void Update(double deltaSeconds);
    void Reset() noexcept;
    void Shutdown() noexcept;

  private:
    static constexpr double FeedbackSeconds = 0.2;
    void CreateTimingGuide(mrg::visual2d::Visual2DNode &root,
                           finger_drum::texts::TextCatalog &texts);
    void CreateTimingBands();
    void CreateTimingCaptions(finger_drum::texts::TextCatalog &texts);
    void CreateTimingMarker();
    void CreateJudgementImages(mrg::visual2d::ScreenVisual2DManager &visuals,
                               mrg::visual2d::Visual2DNode &judgementCircle);
    void UpdateMarkerPosition();

    std::array<finger_drum::rhythm::RhythmDuration,
               finger_drum::rhythm::AccuracyRange::BandCount> halfWindows_{};
    finger_drum::rhythm::RhythmDuration displayHalfWindow_{};
    finger_drum::rhythm::RhythmDuration markerError_{};
    float pixelsPerMicrosecond_{};
    mrg::visual2d::Visual2DNode *guide_{};
    mrg::visual2d::Visual2DNode *margin_{};
    std::array<mrg::visual2d::Visual2DNode *,
               finger_drum::rhythm::AccuracyRange::BandCount> bands_{};
    std::array<mrg::visual2d::Visual2DNode *, 3> labels_{};
    mrg::visual2d::Visual2DNode *marker_{};
    std::array<mrg::visual2d::Visual2DNode *, 7> judgementImages_{};
    mrg::visual2d::Visual2DNode *activeJudgement_{};
    double markerElapsedSeconds_{FeedbackSeconds};
    double judgementElapsedSeconds_{FeedbackSeconds};
};
