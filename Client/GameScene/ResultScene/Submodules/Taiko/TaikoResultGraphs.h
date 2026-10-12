#pragma once
#include "MRG_Core.h"
#include "Taiko/Results/TaikoPlayResult.h"
namespace taiko_result_ui
{
    struct GraphData
    {
        std::vector<mrg::visual2d::RectanglePrimitive> rectangles;
        double halfRangeMilliseconds{100};
        double startSeconds{}, endSeconds{};
        std::size_t maximumBinCount{};
    };
    GraphData BuildGraphs(const finger_drum::mode::TaikoPlayResult &result, mrg::visual2d::Rect scatter,
                          mrg::visual2d::Rect histogram);
} // namespace taiko_result_ui
