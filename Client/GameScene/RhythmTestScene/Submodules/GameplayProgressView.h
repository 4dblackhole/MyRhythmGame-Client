#pragma once

#include "Common/RhythmTypes.h"
#include "PhantomCore.h"
#include "Texts/TextCatalog.h"

class PlaySession;

// Caches the last compiled note/tail time and owns only progress presentation.
// Canvas owns the nodes; Shutdown releases observers before Canvas destruction.
class GameplayProgressView final
{
  public:
    void Initialize(ScreenVisual2DManager &visuals, Visual2DNode &parent,
                    const PlaySession &session, TextCatalog &texts);
    [[nodiscard]] float Height() const noexcept
    {
        return height_;
    }
    void SetLayout(Rect bounds);
    void Update(RhythmTime time);
    void Shutdown() noexcept;

  private:
    void CacheLastNoteTime(const PlaySession &session);
    void CreateTimeLabels(TextCatalog &texts);
    void UpdateFillLayout();

    Visual2DNode *root_{};
    Visual2DNode *track_{};
    Visual2DNode *fill_{};
    Visual2DNode *marker_{};
    Visual2DNode *currentLabel_{};
    Visual2DNode *totalLabel_{};
    RhythmTime endTime_{};
    std::int64_t displayedSeconds_{-1};
    float height_{};
    float trackHeight_{};
    Size markerSize_{};
    float labelWidth_{};
    float fraction_{};
    bool hasNotes_{};
};
