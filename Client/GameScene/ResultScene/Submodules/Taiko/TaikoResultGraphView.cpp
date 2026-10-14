#include "TaikoResultGraphs.h"
#include "TaikoResultLayout.h"
#include "TaikoResultView.h"
#include "Texts/GameScene/ResultScene/ResultTexts.h"
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
    const auto &text = finger_drum::texts::Result(texts_.CurrentLanguage());
    Panel(root, {1152, 104, 720, 352}, Surface, 24, 3);
    Panel(root, {1152, 476, 720, 416}, Surface, 24, 3);
    // Outlines/grid are UI decoration. The graph builder owns range/bin/point
    // derivation from the immutable mode result.
    Panel(root, Histogram, Plot, 8, 1, Keyline);
    Panel(root, Scatter, Plot, 8, 1, Keyline);
    auto data = taiko_result_ui::BuildGraphs(result_, Bounds(Scatter), Bounds(Histogram));
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
        Label(root, {1248, 264, 576, 32}, std::wstring(text.noTimingData), 16, 500, v::TextAlignment::Center, Muted);
        Label(root, {1262, 674, 562, 30}, std::wstring(text.noTimingData), 16, 500, v::TextAlignment::Center, Muted);
    }
}
void TaikoResultView::BuildHistogramLabels(v::Visual2DNode &root, const taiko_result_ui::GraphData &data)
{
    const auto &text = finger_drum::texts::Result(texts_.CurrentLanguage());
    Label(root, {1180, 122, 400, 34}, std::wstring(text.histogram), 22, 700);
    Label(root, {1590, 127, 254, 26}, Format(text.peakFormat, data.maximumBinCount), 13, 500,
          v::TextAlignment::Trailing, Muted);
    Label(root, {1180, 156, 100, 18}, std::wstring(text.frequency), 11, 600, v::TextAlignment::Leading, Muted);
    Label(root, {1180, 374, 34, 20}, L"0", 11, 500, v::TextAlignment::Trailing, Muted);
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
    Label(root, {1228, 424, 164, 20}, std::wstring(text.early), 12, 700, v::TextAlignment::Leading, Accent);
    Label(root, {1446, 424, 180, 20}, std::wstring(text.errorAxis), 12, 500, v::TextAlignment::Center, Muted);
    Label(root, {1680, 424, 164, 20}, std::wstring(text.late), 12, 700, v::TextAlignment::Trailing, Accent);
}
void TaikoResultView::BuildScatterLabels(v::Visual2DNode &root, const taiko_result_ui::GraphData &data)
{
    const auto &text = finger_drum::texts::Result(texts_.CurrentLanguage());
    Label(root, {1180, 494, 620, 34}, std::wstring(text.scatter), 22, 700);
    const std::array<std::wstring_view, 3> names{text.mean, text.standardDeviation, text.variance};
    const std::array<std::optional<double>, 3> values{
        result_.meanErrorMilliseconds, result_.standardDeviationMilliseconds, result_.varianceMillisecondsSquared};
    for (std::size_t i = 0; i < 3; ++i)
    {
        const float x = 1180 + static_cast<float>(i) * 222;
        Label(root, {x, 540, 218, 20}, std::wstring(names[i]), 11, 600, v::TextAlignment::Leading, Muted);
        Label(root, {x, 562, 218, 30},
              values[i] ? Format(i == 2 ? L"{:.3f} ms²" : L"{:.3f} ms", *values[i]) : (i == 2 ? L"— ms²" : L"— ms"), 20,
              700);
        if (i > 0)
            Panel(root, {x - 22, 540, 1, 46}, Keyline, 0);
    }
    Label(root, {1180, 612, 48, 20}, std::wstring(text.late), 10, 600, v::TextAlignment::Trailing, Muted);
    Label(root, {1180, 634, 48, 20}, Format(L"+{:.0f}", data.halfRangeMilliseconds), 11, 500,
          v::TextAlignment::Trailing, Muted);
    Label(root, {1180, 702, 48, 24}, L"0 ms", 11, 600, v::TextAlignment::Trailing, Muted);
    Label(root, {1180, 768, 48, 20}, std::wstring(text.early), 10, 600, v::TextAlignment::Trailing, Muted);
    Label(root, {1180, 790, 48, 20}, Format(L"−{:.0f}", data.halfRangeMilliseconds), 11, 500,
          v::TextAlignment::Trailing, Muted);
    Label(root, {1242, 819, 120, 22}, Format(L"{:.2f} s", data.startSeconds), 12, 500, v::TextAlignment::Leading,
          Muted);
    Label(root, {1433, 819, 220, 22}, std::wstring(text.timeAxis), 12, 500, v::TextAlignment::Center, Muted);
    Label(root, {1724, 819, 120, 22}, Format(L"{:.2f} s", data.endSeconds), 12, 500, v::TextAlignment::Trailing, Muted);
}
void TaikoResultView::BuildGraphLegend(v::Visual2DNode &root)
{
    const std::array<std::wstring_view, 5> grades{L"MAX", L"PERFECT", L"GREAT", L"GOOD", L"BAD"};
    for (std::size_t i = 0; i < grades.size(); ++i)
    {
        const float x = 1242 + static_cast<float>(i) * 120;
        Panel(root, {x, 858, 10, 10}, Markers[i], 3, i == 0 ? 1.0F : 0.0F, Color(0x96B9D1));
        Label(root, {x + 16, 852, 100, 24}, std::wstring(grades[i]), 10, 600, v::TextAlignment::Leading, Muted);
    }
}
