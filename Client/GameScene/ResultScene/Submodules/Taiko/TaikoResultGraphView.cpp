#include "TaikoResultGraphs.h"
#include "TaikoResultLayout.h"
#include "TaikoResultView.h"
#include "Texts/GameScene/ResultScene/ResultTexts.h"
#include <algorithm>
#include <cmath>
namespace v = mrg::visual2d;
using namespace taiko_result_ui::layout;
namespace
{
    // Result graph geometry is built on entry/resize/language changes; packets
    // share it rather than creating a Canvas node for each recorded input.
    class GraphVisual final : public v::Visual2DComponent
    {
      public:
        explicit GraphVisual(std::vector<v::RectanglePrimitive> rectangles)
            : rectangles_(std::make_shared<const std::vector<v::RectanglePrimitive>>(std::move(rectangles)))
        {
        }
        void AppendDrawPackets(std::vector<v::DrawPacket> &output) const override
        {
            v::DrawPacket packet;
            packet.type = v::DrawPacketType::RectangleBatch;
            packet.rectangles = rectangles_;
            output.push_back(std::move(packet));
        }

      private:
        std::shared_ptr<const std::vector<v::RectanglePrimitive>> rectangles_;
    };
} // namespace
void TaikoResultView::BuildGraphs(v::Visual2DNode &root)
{
    const auto &text = finger_drum::texts::Result(texts_);
    Panel(root, HistogramPanel, Surface, 24, 3);
    Panel(root, ScatterPanel, Surface, 24, 3);
    // Outlines/grid are UI decoration. The graph builder owns range/bin/point
    // derivation from the immutable mode result.
    Panel(root, Histogram, Plot, 8, 1, Keyline);
    Panel(root, Scatter, Plot, 8, 1, Keyline);
    // Content fits both viewport dimensions. A one-pixel minimum keeps isolated
    // 1ms bins visible after that scaling without merging their input counts.
    const float pixelsPerUnit = std::min(static_cast<float>(width_) / Width, static_cast<float>(height_) / Height);
    auto data = taiko_result_ui::BuildGraphs(result_, Bounds(Scatter), Bounds(Histogram),
                                             pixelsPerUnit > 0 ? 1.0F / pixelsPerUnit : 1.0F);
    for (const float portion : {.25F, .5F, .75F})
    {
        Panel(root, {Histogram.x, Histogram.y + Histogram.height * portion, Histogram.width, 1}, Keyline, 0);
        Panel(root, {Histogram.x + Histogram.width * portion, Histogram.y, 1, Histogram.height}, Keyline, 0);
        Panel(root, {Scatter.x, Scatter.y + Scatter.height * portion, Scatter.width, 1}, Keyline, 0);
        Panel(root, {Scatter.x + Scatter.width * portion, Scatter.y, 1, Scatter.height}, Keyline, 0);
    }
    auto &graphs = root.CreateChild("Result.Graphs");
    graphs.SetSize({Width, Height});
    graphs.AddComponent<GraphVisual>(std::move(data.rectangles));
    BuildHistogramLabels(root, data);
    BuildScatterLabels(root, data);
    BuildGraphLegend(root);
    if (result_.timingErrors.empty())
    {
        Label(root, {1440, 264, 384, 32}, std::wstring(text.noTimingData), 16, 500, v::TextAlignment::Center, Muted);
        Label(root, {1454, 674, 370, 30}, std::wstring(text.noTimingData), 16, 500, v::TextAlignment::Center, Muted);
    }
}
void TaikoResultView::BuildHistogramLabels(v::Visual2DNode &root, const taiko_result_ui::GraphData &data)
{
    const auto &text = finger_drum::texts::Result(texts_);
    Label(root, {1372, 122, 244, 34}, std::wstring(text.histogram), 22, 700);
    Label(root, {1630, 127, 214, 26}, Format(text.peakFormat, data.maximumBinCount), 13, 500,
          v::TextAlignment::Trailing, Muted);
    Label(root, {1372, 156, 100, 18}, std::wstring(text.frequency), 11, 600, v::TextAlignment::Leading, Muted);
    Label(root, {1372, 374, 34, 20}, L"0", 11, 500, v::TextAlignment::Trailing, Muted);
    for (std::size_t i = 0; i < 5; ++i)
    {
        const float x = Histogram.x + Histogram.width * static_cast<float>(i) / 4;
        const auto align = i == 0   ? v::TextAlignment::Leading
                           : i == 4 ? v::TextAlignment::Trailing
                                    : v::TextAlignment::Center;
        Label(root,
              {x - (i == 0   ? 0.0F
                    : i == 4 ? 60.0F
                             : 30.0F),
               395, 60, 20},
              Format(i < 2   ? L"−{:.0f}"
                     : i > 2 ? L"+{:.0f}"
                             : L"{:.0f}",
                     std::abs((static_cast<double>(i) / 2 - 1) * data.halfRangeMilliseconds)),
              12, 500, align, Muted);
    }
    Label(root, {1420, 424, 100, 20}, std::wstring(text.early), 12, 700, v::TextAlignment::Leading, Accent);
    Label(root, {1542, 424, 180, 20}, std::wstring(text.errorAxis), 12, 500, v::TextAlignment::Center, Muted);
    Label(root, {1744, 424, 100, 20}, std::wstring(text.late), 12, 700, v::TextAlignment::Trailing, Accent);
}
void TaikoResultView::BuildScatterLabels(v::Visual2DNode &root, const taiko_result_ui::GraphData &data)
{
    const auto &text = finger_drum::texts::Result(texts_);
    Label(root, {1372, 494, 472, 34}, std::wstring(text.scatter), 22, 700);
    const std::array<std::wstring_view, 3> names{text.mean, text.standardDeviation, text.variance};
    const std::array<std::optional<double>, 3> values{
        result_.meanErrorMilliseconds, result_.standardDeviationMilliseconds, result_.varianceMillisecondsSquared};
    for (std::size_t i = 0; i < 3; ++i)
    {
        const float x = 1372 + static_cast<float>(i) * 164;
        Label(root, {x, 540, 144, 20}, std::wstring(names[i]), 11, 600, v::TextAlignment::Leading, Muted);
        Label(root, {x, 562, 144, 30},
              values[i] ? Format(i == 2 ? std::wstring(texts_.Get("Result.varianceFormat"))
                                        : std::wstring(texts_.Get("Result.millisecondsFormat")),
                                 *values[i])
                        : (i == 2 ? std::wstring(texts_.Get("Result.emptyVariance"))
                                  : std::wstring(texts_.Get("Result.emptyMilliseconds"))),
              20, 700);
        if (i > 0)
            Panel(root, {x - 16, 540, 1, 46}, Keyline, 0);
    }
    Label(root, {1372, 612, 48, 20}, std::wstring(text.late), 10, 600, v::TextAlignment::Trailing, Muted);
    Label(root, {1372, 634, 48, 20}, Format(L"+{:.0f}", data.halfRangeMilliseconds), 11, 500,
          v::TextAlignment::Trailing, Muted);
    Label(root, {1372, 702, 48, 24}, std::wstring(texts_.Get("Result.zeroMilliseconds")), 11, 600,
          v::TextAlignment::Trailing, Muted);
    Label(root, {1372, 768, 48, 20}, std::wstring(text.early), 10, 600, v::TextAlignment::Trailing, Muted);
    Label(root, {1372, 790, 48, 20}, Format(L"−{:.0f}", data.halfRangeMilliseconds), 11, 500,
          v::TextAlignment::Trailing, Muted);
    Label(root, {1434, 819, 90, 22}, Format(std::wstring(texts_.Get("Result.secondsFormat")), data.startSeconds), 12,
          500, v::TextAlignment::Leading, Muted);
    Label(root, {1549, 819, 180, 22}, std::wstring(text.timeAxis), 12, 500, v::TextAlignment::Center, Muted);
    Label(root, {1754, 819, 90, 22}, Format(std::wstring(texts_.Get("Result.secondsFormat")), data.endSeconds), 12, 500,
          v::TextAlignment::Trailing, Muted);
}
void TaikoResultView::BuildGraphLegend(v::Visual2DNode &root)
{
    const std::array<std::wstring_view, 5> grades{texts_.Get("Judgements.MAX"), texts_.Get("Judgements.PERFECT"),
                                                  texts_.Get("Judgements.GREAT"), texts_.Get("Judgements.GOOD"),
                                                  texts_.Get("Judgements.BAD")};
    for (std::size_t i = 0; i < grades.size(); ++i)
    {
        const float x = 1434 + static_cast<float>(i) * 82;
        Panel(root, {x, 858, 10, 10}, Markers[i], 3, i == 0 ? 1.0F : 0.0F, Color(0x96B9D1));
        Label(root, {x + 15, 852, 65, 24}, std::wstring(grades[i]), 10, 600, v::TextAlignment::Leading, Muted);
    }
}
