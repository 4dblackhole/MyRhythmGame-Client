#include "TaikoEditorLayout.h"
#include "TaikoEditorMode.h"
#include <algorithm>
#include <cmath>

namespace chart = finger_drum::chart;
namespace v = mrg::visual2d;
using namespace taiko_editor_layout;

std::optional<chart::MusicalPosition> TaikoEditorMode::SnapPosition(IEditorContext &state, v::Point point) const
{
    if (state.Score().realtime)
    {
        if (realtimeGrid_.empty())
            return {};
        return std::ranges::min_element(realtimeGrid_, {},
                                        [point](const auto &grid) { return std::abs(grid.first - point.x); })
            ->second;
    }
    if (overviewGrid_.empty())
        return {};
    const auto x = std::clamp(point.x, ScoreLeft, ScoreRight - .01F);
    const auto y = std::clamp(point.y, RowTop, RowTop + RowHeight * VisibleRows - .01F);
    const auto measure = state.Score().firstMeasure +
                         static_cast<std::int64_t>((y - RowTop) / RowHeight) * MeasuresPerRow +
                         static_cast<std::int64_t>((x - ScoreLeft) / MeasureWidth);
    const auto begin =
        std::ranges::lower_bound(overviewGrid_, measure, {}, [](const auto &g) { return g.second.measure; });
    const auto end =
        std::ranges::upper_bound(overviewGrid_, measure, {}, [](const auto &g) { return g.second.measure; });
    if (begin == end)
        return {};
    return std::min_element(begin, end, [x](const auto &a, const auto &b)
                            { return std::abs(a.first.x - x) < std::abs(b.first.x - x); })
        ->second;
}

bool TaikoEditorMode::BeginScoreSelection(IEditorContext &state, v::Point point)
{
    if (tool_ != 0 || !(state.Score().realtime ? RealtimeLane : OverviewBounds).Contains(point))
        return false;
    const auto position = SnapPosition(state, point);
    if (!position)
        return false;
    EditScore(state, point, false);
    selectionAnchor_ = position;
    selectionStart_ = point;
    selecting_ = false;
    return true;
}

bool TaikoEditorMode::UpdateScoreSelection(IEditorContext &state, v::Point point, bool released)
{
    if (!selectionAnchor_)
        return false;
    selecting_ |= std::hypot(point.x - selectionStart_.x, point.y - selectionStart_.y) >= 4;
    if (selecting_)
        if (const auto end = SnapPosition(state, point))
        {
            inspector_.Clear();
            state.SelectNoteRange(*selectionAnchor_, *end);
        }
    if (released)
    {
        selectionAnchor_.reset();
        selecting_ = false;
    }
    return true;
}
