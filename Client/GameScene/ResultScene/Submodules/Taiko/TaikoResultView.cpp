#include "TaikoResultView.h"
#include "TaikoResultGraphs.h"
#include "Texts/GameScene/ResultScene/ResultTexts.h"
#include <format>
#include <stdexcept>
namespace v = mrg::visual2d;
namespace
{
    // One immutable batch for both graphs; no Canvas node per input sample.
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
    template <typename... Args> std::wstring Format(std::wstring_view text, Args &&...args)
    {
        return std::vformat(text, std::make_wformat_args(args...));
    }
    std::wstring WideUtf8(std::string_view text)
    {
        if (text.empty())
            return {};
        if (text.size() > static_cast<std::size_t>(INT_MAX))
            return L"\uFFFD";
        const auto size = static_cast<int>(text.size());
        const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), size, nullptr, 0);
        if (length == 0)
            return L"\uFFFD";
        std::wstring result(static_cast<std::size_t>(length), L'\0');
        if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), size, result.data(), length) != length)
            return L"\uFFFD";
        return result;
    }
} // namespace
TaikoResultView::TaikoResultView(const finger_drum::mode::TaikoPlayResult &result, v::ScreenVisual2DManager &visuals,
                                 finger_drum::texts::TextCatalog &texts)
    : result_(result), visuals_(visuals), texts_(texts)
{
}
TaikoResultView::~TaikoResultView()
{
    Shutdown();
}
void TaikoResultView::Initialize(const mrg::EngineServices &services)
{
    width_ = services.windowWidth;
    height_ = services.windowHeight;
    canvasHandle_ = visuals_.CreateOwnedCanvas({{1280, 720}, v::CanvasScaleMode::FixedHeight});
    canvas_ = canvasHandle_.Get();
    if (!canvas_)
        throw std::runtime_error("Failed to create result Canvas.");
    canvas_->SetViewportSize({static_cast<float>(width_), static_cast<float>(height_)});
    Build();
}
void TaikoResultView::Label(v::Visual2DNode &root, v::Rect bounds, std::wstring text, float size)
{
    auto &label = v::CreateLabel(root, bounds, std::move(text), "Result.Label");
    auto &component = *label.GetComponent<v::TextVisualComponent>();
    component.SetFontSize(size);
    component.SetTextColor({.92F, .95F, 1, 1});
    texts_.ApplyFont(component);
}
void TaikoResultView::BuildSummary(v::Visual2DNode &root, float width)
{
    const auto &text = finger_drum::texts::Result(texts_.CurrentLanguage());
    const auto &summary = result_.summary;
    Label(root, {24, 616, width - 48, 42}, std::wstring(text.title), 32);
    Label(root, {24, 580, width - 48, 32},
          WideUtf8(summary.patternName) + L"  ·  " +
              std::wstring(summary.endReason == finger_drum::mode::PlayEndReason::Completed ? text.completed
                                                                                            : text.interrupted),
          22);
    Label(root, {24, 528, width - 48, 46}, Format(text.totalAccuracy, summary.accuracy.Percent()), 30);
    Label(root, {24, 492, width - 48, 30},
          Format(text.noteCounts, summary.totalNotes, summary.processedNotes, summary.unprocessedNotes));
    Label(root, {24, 457, width - 48, 30},
          std::format(L"MAX {}   PERFECT {}   GREAT {}   GOOD {}   BAD {}   MISS {}   OK {}   POOR {}",
                      result_.judgements[0], result_.judgements[1], result_.judgements[2], result_.judgements[3],
                      result_.judgements[4], result_.judgements[5], result_.okCount, result_.poorCount));
    Label(root, {24, 434, width - 48, 24}, std::wstring(text.judgementHelp), 14);
    for (std::size_t i = 0; i < result_.groups.size(); ++i)
    {
        const auto &group = result_.groups[i];
        Label(root, {24, 397 - static_cast<float>(i) * 33, width - 48, 30},
              group.total
                  ? Format(text.groupFormat, text.groups[i], group.accuracy.Percent(), group.processed, group.total)
                  : std::wstring(text.groups[i]) + L"   — (0/0)");
    }
    Label(root, {24, 298, width - 48, 30},
          result_.meanErrorMilliseconds
              ? Format(text.timingStatistics, *result_.meanErrorMilliseconds, *result_.varianceMillisecondsSquared,
                       *result_.standardDeviationMilliseconds)
              : std::wstring(text.noTimingData));
    Label(root, {24, 270, width - 48, 28},
          Format(text.tickCounts, result_.acceptedTicks, result_.missedTicks) + L"   ·   " +
              Format(text.inputCount, summary.inputs.size()),
          16);
}
void TaikoResultView::BuildGraphs(v::Visual2DNode &root, float width)
{
    const auto &text = finger_drum::texts::Result(texts_.CurrentLanguage());
    const float graphWidth = (width - 96) / 2;
    const v::Rect scatter{24, 91, graphWidth, 148}, histogram{width / 2 + 24, 91, graphWidth, 148};
    auto data = taiko_result_ui::BuildGraphs(result_, scatter, histogram);
    auto &graphs = root.CreateChild("Result.Graphs");
    graphs.SetSize({width, 670});
    graphs.AddComponent<GraphVisual>(std::move(data.rectangles));
    Label(root, {scatter.x, 241, graphWidth, 28}, std::wstring(text.timeGraph), 18);
    Label(root, {histogram.x, 241, graphWidth, 28},
          std::wstring(text.histogram) + L"  (max " + std::to_wstring(data.maximumBinCount) + L")", 18);
    Label(root, {scatter.x, 61, graphWidth, 28}, Format(text.timeAxis, data.startSeconds, data.endSeconds), 14);
    Label(root, {histogram.x, 61, graphWidth, 28},
          Format(text.errorAxis, data.halfRangeMilliseconds, data.halfRangeMilliseconds), 14);
    Label(root, {scatter.x + 6, 213, graphWidth - 12, 24}, Format(L"+{:.1f}ms", data.halfRangeMilliseconds), 12);
    Label(root, {scatter.x + 6, 145, graphWidth - 12, 24}, L"0ms", 12);
    Label(root, {scatter.x + 6, 93, graphWidth - 12, 24}, Format(L"−{:.1f}ms", data.halfRangeMilliseconds), 12);
    if (result_.timingErrors.empty())
    {
        Label(root, {scatter.x + 12, 167, graphWidth - 24, 28}, std::wstring(text.noTimingData), 16);
        Label(root, {histogram.x + 12, 167, graphWidth - 24, 28}, std::wstring(text.noTimingData), 16);
    }
}
void TaikoResultView::Build()
{
    router_.Reset(*canvas_);
    if (contentId_)
        static_cast<void>(canvas_->RemoveNode(contentId_));
    constexpr float width = 1200;
    auto &root = canvas_->CreateNode(v::Anchor::Center, "Result.Content");
    root.SetBounds({0, 0, width, 670});
    root.SetPivot({.5F, .5F});
    const float scale = std::clamp((canvas_->LogicalSize().width - 48) / width, .1F, 1.0F);
    root.Transform().SetScale(scale, scale, 1);
    contentId_ = root.Id();
    auto &background = v::CreatePanel(root, {0, 0, width, 670}, "Result.Background");
    background.GetComponent<v::SpriteVisualComponent>()->SetTint({.09F, .13F, .2F, 1});
    BuildSummary(root, width);
    BuildGraphs(root, width);
    auto &back =
        v::CreateButton(root, {width / 2 - 160, 8, 320, 40},
                        std::wstring(finger_drum::texts::Result(texts_.CurrentLanguage()).back), "Result.Back");
    texts_.ApplyFont(*back.GetComponent<v::TextVisualComponent>());
    backId_ = back.Id();
    textRevision_ = texts_.Revision();
    router_.InvalidateHitTest();
}
void TaikoResultView::SetVisible(bool visible) noexcept
{
    static_cast<void>(canvasHandle_.SetVisible(visible));
}
void TaikoResultView::ProcessPointer(const mrg::platform::InputState &input)
{
    const auto position = input.IsMouseInsideWindow()
                              ? v::MapScreenPointer({static_cast<float>(input.MousePositionX()),
                                                     static_cast<float>(input.MousePositionY())},
                                                    {static_cast<float>(width_), static_cast<float>(height_)}, *canvas_)
                              : std::nullopt;
    v::PointerInput pointer;
    pointer.available = position.has_value();
    pointer.position = position.value_or(v::Point{});
    pointer.leftButtonDown = input.IsMouseButtonDown(mrg::platform::MouseButton::Left);
    pointer.leftButtonPressed = input.WasMouseButtonPressed(mrg::platform::MouseButton::Left);
    pointer.leftButtonReleased = input.WasMouseButtonReleased(mrg::platform::MouseButton::Left);
    router_.Process(*canvas_, pointer);
}
bool TaikoResultView::Update(const mrg::platform::InputState &input)
{
    if (!canvas_)
        return false;
    if (textRevision_ != texts_.Revision())
        Build();
    ProcessPointer(input);
    for (const auto &action : canvas_->TakeActions())
        if (action.type == v::ActionType::Clicked && action.source == backId_)
            return true;
    return input.WasKeyPressed(VK_ESCAPE) || input.WasKeyPressed(VK_RETURN);
}
void TaikoResultView::OnResize(std::uint32_t width, std::uint32_t height)
{
    width_ = width;
    height_ = height;
    if (canvas_)
    {
        canvas_->SetViewportSize({static_cast<float>(width), static_cast<float>(height)});
        Build();
    }
}
void TaikoResultView::Shutdown() noexcept
{
    if (canvas_)
        router_.Reset(*canvas_);
    contentId_ = backId_ = 0;
    canvas_ = nullptr;
    canvasHandle_.Reset();
}
