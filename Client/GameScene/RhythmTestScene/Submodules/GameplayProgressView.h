#pragma once

#include "Common/RhythmTypes.h"
#include "MRG_Core.h"
#include "Texts/TextCatalog.h"

namespace finger_drum::mode
{
    class PlaySession;
}

// Caches the last compiled note/tail time and owns only progress presentation.
// Canvas owns the nodes; Shutdown releases observers before Canvas destruction.
class GameplayProgressView final
{
  public:
    void Initialize(mrg::visual2d::ScreenVisual2DManager &visuals, mrg::visual2d::Visual2DNode &parent,
                    const finger_drum::mode::PlaySession &session, finger_drum::texts::TextCatalog &texts);
    [[nodiscard]] float Height() const noexcept
    {
        return height_;
    }
    void SetLayout(mrg::visual2d::Rect bounds);
    void Update(finger_drum::rhythm::RhythmTime time);
    void Shutdown() noexcept;

  private:
    void CacheLastNoteTime(const finger_drum::mode::PlaySession &session);
    void CreateTimeLabels(finger_drum::texts::TextCatalog &texts);
    void UpdateFillLayout();

    mrg::visual2d::Visual2DNode *root_{};
    mrg::visual2d::Visual2DNode *track_{};
    mrg::visual2d::Visual2DNode *fill_{};
    mrg::visual2d::Visual2DNode *marker_{};
    mrg::visual2d::Visual2DNode *currentLabel_{};
    mrg::visual2d::Visual2DNode *totalLabel_{};
    finger_drum::rhythm::RhythmTime endTime_{};
    std::int64_t displayedSeconds_{-1};
    float height_{};
    float trackHeight_{};
    mrg::visual2d::Size markerSize_{};
    float labelWidth_{};
    float fraction_{};
    bool hasNotes_{};
};
