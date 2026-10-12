#include "TaikoResultGraphs.h"
#include "Presentation/LaneKeyBeam.h"
#include <algorithm>
#include <array>
#include <cmath>
namespace taiko_result_ui
{
    namespace v = mrg::visual2d;
    GraphData BuildGraphs(const finger_drum::mode::TaikoPlayResult &result, v::Rect scatter, v::Rect histogram)
    {
        GraphData data;
        auto first = result.summary.initialTime;
        auto last = result.summary.endTime;
        for (const auto &sample : result.timingErrors)
        {
            first = std::min(first, sample.inputTime);
            last = std::max(last, sample.inputTime);
            data.halfRangeMilliseconds =
                std::max(data.halfRangeMilliseconds, std::abs(static_cast<double>(sample.error.count()) / 1000));
        }
        data.halfRangeMilliseconds = std::ceil(data.halfRangeMilliseconds / 10) * 10;
        data.startSeconds = static_cast<double>(first.count()) / 1'000'000;
        data.endSeconds = std::max(data.startSeconds + .001, static_cast<double>(last.count()) / 1'000'000);
        const v::Color grid{.3F, .38F, .48F, 1};
        const v::Color background{.06F, .08F, .12F, 1};
        data.rectangles.reserve(result.timingErrors.size() + 50);
        data.rectangles.push_back({scatter, background, 0});
        data.rectangles.push_back({histogram, background, 0});
        data.rectangles.push_back({{scatter.x, scatter.y + scatter.height / 2, scatter.width, 1}, grid, 0});
        data.rectangles.push_back({{histogram.x + histogram.width / 2, histogram.y, 1, histogram.height}, grid, 0});
        constexpr std::size_t BinCount = 41;
        std::array<std::size_t, BinCount> bins{};
        for (const auto &sample : result.timingErrors)
        {
            const double seconds = static_cast<double>(sample.inputTime.count()) / 1'000'000;
            const double error = static_cast<double>(sample.error.count()) / 1000;
            const float x = static_cast<float>((seconds - data.startSeconds) / (data.endSeconds - data.startSeconds));
            const float y = static_cast<float>((error / data.halfRangeMilliseconds + 1) / 2);
            const auto color = finger_drum::presentation::LaneKeyBeam::GradeColor(sample.grade);
            data.rectangles.push_back(
                {{scatter.x + x * (scatter.width - 3), scatter.y + y * (scatter.height - 3), 3, 3}, color, 0});
            const auto bin = static_cast<std::size_t>(std::clamp(
                (error / data.halfRangeMilliseconds + 1) * .5 * BinCount, 0.0, static_cast<double>(BinCount - 1)));
            data.maximumBinCount = std::max(data.maximumBinCount, ++bins[bin]);
        }
        if (data.maximumBinCount == 0)
            return data;
        const float binWidth = histogram.width / BinCount;
        for (std::size_t i = 0; i < bins.size(); ++i)
        {
            if (bins[i] == 0)
                continue;
            const float height =
                histogram.height * static_cast<float>(bins[i]) / static_cast<float>(data.maximumBinCount);
            data.rectangles.push_back(
                {{histogram.x + static_cast<float>(i) * binWidth, histogram.y, std::max(1.0F, binWidth - 1), height},
                 {.2F, .72F, 1, 1},
                 0});
        }
        return data;
    }
} // namespace taiko_result_ui
