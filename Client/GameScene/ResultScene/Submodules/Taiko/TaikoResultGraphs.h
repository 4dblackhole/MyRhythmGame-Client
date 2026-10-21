#pragma once
#include "PhantomCore.h"
#include "Taiko/Results/TaikoPlayResult.h"
namespace taiko_result_ui
{
    struct GraphData
    {
        std::vector<RectanglePrimitive> rectangles;
        double halfRangeMilliseconds{100};
        double startSeconds{}, endSeconds{};
        std::size_t maximumBinCount{};
    };
    GraphData BuildGraphs(const TaikoPlayResult &result, Rect scatter,
                          Rect histogram, float minimumBarWidth = 1.0F);
} // namespace taiko_result_ui
