#pragma once
#include "PhantomCore.h"

namespace taiko_editor_layout
{
    inline constexpr Rect RealtimeLane{164, 351, 1324, 228};
    inline constexpr float ScoreLeft = 205, MeasureWidth = 315, RowTop = 150, RowHeight = 100, StaffHeight = 48;
    inline constexpr int MeasuresPerRow = 4, VisibleRows = 6;
    inline constexpr float ScoreRight = ScoreLeft + MeasureWidth * MeasuresPerRow;
    inline constexpr Rect OverviewBounds{ScoreLeft, RowTop, MeasuresPerRow * MeasureWidth,
                                                      VisibleRows * RowHeight};
} // namespace taiko_editor_layout
