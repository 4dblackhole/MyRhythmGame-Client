#include "TaikoResultGraphs.h"
#include "Judgement/AccuracyRange.h"
#include "TaikoResultLayout.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <map>
namespace taiko_result_ui
{
    namespace v = mrg::visual2d;
    namespace
    {
        constexpr std::size_t GradeCount = finger_drum::rhythm::AccuracyRange::BandCount;
        struct HistogramBin
        {
            std::array<std::size_t, GradeCount + 1> grades{};
            std::size_t total{};
        };
        using HistogramBins = std::map<std::int64_t, HistogramBin>;

        std::size_t GraphColorIndex(finger_drum::rhythm::JudgementGrade grade) noexcept
        {
            const auto index = static_cast<std::size_t>(grade);
            return index < GradeCount ? index : GradeCount;
        }

        v::Color GraphColor(std::size_t index) noexcept
        {
            return index < GradeCount ? layout::Markers[index] : layout::Markers[6];
        }

        std::int64_t HistogramMillisecond(finger_drum::rhythm::RhythmDuration error) noexcept
        {
            // Round symmetrically around zero without overflowing the us value
            // or losing its sub-millisecond precision in a floating conversion.
            const auto remainder = error.count() % 1000;
            return error.count() / 1000 + (remainder >= 500 ? 1 : remainder <= -500 ? -1 : 0);
        }

        void AppendHistogram(GraphData &data, const HistogramBins &bins, v::Rect histogram, float minimumBarWidth)
        {
            if (data.maximumBinCount == 0)
                return;
            const double binWidth = histogram.width / (2 * data.halfRangeMilliseconds);
            // Widen only the footprint at subpixel scales, never the 1ms
            // aggregation interval or its total. Keep edge bins inside the plot.
            const double barWidth = std::min<double>(histogram.width, std::max<double>(binWidth * .9, minimumBarWidth));
            const double inversePeak = 1.0 / static_cast<double>(data.maximumBinCount);
            for (const auto &[millisecond, bin] : bins)
            {
                const double center =
                    (static_cast<double>(millisecond) / data.halfRangeMilliseconds + 1) * .5 * histogram.width;
                const auto left =
                    static_cast<float>(std::clamp(center - barWidth / 2, 0.0, histogram.width - barWidth));
                std::size_t cumulativeCount{};
                // Preserve every recorded grade, including mixed judgement
                // windows in one bin. Peak scaling uses the whole bin total.
                for (std::size_t grade = 0; grade < bin.grades.size(); ++grade)
                {
                    const auto count = bin.grades[grade];
                    if (count == 0)
                        continue;
                    const double bottom = static_cast<double>(cumulativeCount) * inversePeak;
                    cumulativeCount += count;
                    const double top = static_cast<double>(cumulativeCount) * inversePeak;
                    data.rectangles.push_back(
                        {{histogram.x + left, histogram.y + static_cast<float>(histogram.height * bottom),
                          static_cast<float>(barWidth), static_cast<float>(histogram.height * (top - bottom))},
                         GraphColor(grade),
                         0});
                }
            }
        }
    } // namespace
    GraphData BuildGraphs(const finger_drum::mode::TaikoPlayResult &result, v::Rect scatter, v::Rect histogram,
                          float minimumBarWidth)
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
        const v::Color grid = layout::Color(0x4F9FDF, .65F);
        data.rectangles.reserve(result.timingErrors.size() + 50);
        // The view owns decorative backgrounds/grid. These zero references
        // are part of interpreting the data range, followed by points/bins.
        data.rectangles.push_back({{scatter.x, scatter.y + scatter.height / 2 - 1, scatter.width, 2}, grid, 0});
        data.rectangles.push_back({{histogram.x + histogram.width / 2 - 1, histogram.y, 2, histogram.height}, grid, 0});
        // Store occupied 1ms bins only, so an outlier never allocates a dense
        // array proportional to the entire displayed error range.
        HistogramBins bins;
        for (const auto &sample : result.timingErrors)
        {
            const double seconds = static_cast<double>(sample.inputTime.count()) / 1'000'000;
            const double error = static_cast<double>(sample.error.count()) / 1000;
            const float x = static_cast<float>((seconds - data.startSeconds) / (data.endSeconds - data.startSeconds));
            const float y = static_cast<float>((error / data.halfRangeMilliseconds + 1) / 2);
            const auto gradeIndex = GraphColorIndex(sample.grade);
            const auto color = GraphColor(gradeIndex);
            data.rectangles.push_back(
                {{scatter.x + x * (scatter.width - 3), scatter.y + y * (scatter.height - 3), 3, 3}, color, 0});
            auto &bin = bins[HistogramMillisecond(sample.error)];
            ++bin.grades[gradeIndex];
            data.maximumBinCount = std::max(data.maximumBinCount, ++bin.total);
        }
        AppendHistogram(data, bins, histogram, minimumBarWidth);
        return data;
    }
} // namespace taiko_result_ui
