#include "TaikoResultLayout.h"
#include "TaikoResultView.h"
#include "Texts/GameScene/ResultScene/ResultTexts.h"
#include <array>
#include <climits>
namespace v = mrg::visual2d;
using namespace taiko_result_ui::layout;
namespace
{
    std::wstring Decode(std::string_view text)
    {
        if (text.empty())
            return L"—";
        if (text.size() > static_cast<std::size_t>(INT_MAX))
            return L"\uFFFD";
        const auto size = static_cast<int>(text.size());
        const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), size, nullptr, 0);
        if (length == 0)
            return L"\uFFFD";
        std::wstring value(static_cast<std::size_t>(length), L'\0');
        if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), size, value.data(), length) != length)
            return L"\uFFFD";
        return value;
    }
} // namespace
void TaikoResultView::BuildHpGraph(v::Visual2DNode &root)
{
    const auto &text = finger_drum::texts::Result(texts_.CurrentLanguage());
    Label(root, {76, 124, 310, 30}, std::wstring(text.hpGraph), 20, 700);
    Panel(root, {438, 128, 110, 26}, Plot, 13, 1, Keyline);
    Label(root, {438, 128, 110, 26}, std::wstring(text.hpFuture), 11, 600, v::TextAlignment::Center, Muted);
    Panel(root, {122, 170, 426, 104}, Plot, 6);
    for (const float y : {196.0F, 222.0F, 248.0F})
        Panel(root, {122, y, 426, 1}, Keyline, 0);
    for (const float x : {228.5F, 335.0F, 441.5F})
        Panel(root, {x, 170, 1, 104}, Keyline, 0);
    Panel(root, {121, 170, 1, 105}, Color(0x5783A0, .6F), 0);
    Panel(root, {122, 274, 426, 1}, Color(0x5783A0, .6F), 0);
    Label(root, {76, 161, 38, 20}, L"HP", 12, 600, v::TextAlignment::Leading, Muted);
    Label(root, {154, 205, 362, 30}, std::wstring(text.noHpData), 14, 500, v::TextAlignment::Center, Muted);
    Label(root, {122, 284, 90, 22}, std::wstring(text.start), 11, 500, v::TextAlignment::Leading, Muted);
    Label(root, {245, 284, 180, 22}, std::wstring(text.timeAxis), 11, 500, v::TextAlignment::Center, Muted);
    Label(root, {458, 284, 90, 22}, std::wstring(text.end), 11, 500, v::TextAlignment::Trailing, Muted);
}
void TaikoResultView::BuildJudgements(v::Visual2DNode &root)
{
    const auto &text = finger_drum::texts::Result(texts_.CurrentLanguage());
    Label(root, {76, 510, 472, 30}, std::wstring(text.judgements), 20, 700);
    const std::array<std::wstring_view, 8> names{L"MAX", L"PERFECT", L"GREAT", L"GOOD",
                                                 L"BAD", L"OK",      L"MISS",  L"POOR"};
    const std::array<std::size_t, 8> counts{result_.judgements[0], result_.judgements[1], result_.judgements[2],
                                            result_.judgements[3], result_.judgements[4], result_.okCount,
                                            result_.judgements[5], result_.poorCount};
    for (std::size_t i = 0; i < names.size(); ++i)
    {
        const float y = 548 + static_cast<float>(i) * 26;
        Panel(root, {76, y + 6, 14, 14}, Markers[i], 4, i == 0 ? 1.0F : 0.0F, Color(0x96B9D1));
        Label(root, {104, y, 310, 26}, std::wstring(names[i]), 16, 600);
        Label(root, {444, y, 102, 26}, std::to_wstring(counts[i]), 17, 700, v::TextAlignment::Trailing);
    }
    Panel(root, {76, 764, 472, 1}, Keyline, 0);
    Label(root, {76, 774, 230, 28}, std::wstring(text.ticks), 15, 500, v::TextAlignment::Leading, Muted);
    Label(root, {316, 774, 230, 28}, Format(text.tickFormat, result_.acceptedTicks, result_.missedTicks), 18, 600,
          v::TextAlignment::Trailing);
}
void TaikoResultView::BuildGroups(v::Visual2DNode &root)
{
    const auto &text = finger_drum::texts::Result(texts_.CurrentLanguage());
    Label(root, {76, 828, 472, 30}, std::wstring(text.groupTitle), 20, 700);
    for (std::size_t i = 0; i < result_.groups.size(); ++i)
    {
        const float y = 860 + static_cast<float>(i) * 26;
        Label(root, {76, y, 260, 26}, std::wstring(text.groups[i]), 15, 500, v::TextAlignment::Leading, Muted);
        const auto &group = result_.groups[i];
        Label(root, {340, y, 206, 26}, group.total ? Format(text.percent, group.accuracy.Percent()) : L"— %", 17, 700,
              v::TextAlignment::Trailing);
    }
    const auto &summary = result_.summary;
    Label(root, {76, 946, 472, 18},
          Format(text.noteCounts, summary.totalNotes, summary.processedNotes, summary.unprocessedNotes), 11, 500,
          v::TextAlignment::Leading, Muted);
}
void TaikoResultView::BuildSummary(v::Visual2DNode &root)
{
    const auto &text = finger_drum::texts::Result(texts_.CurrentLanguage());
    Panel(root, {48, 104, 528, 872}, Surface, 24, 3);
    BuildHpGraph(root);
    for (const float y : {320.0F, 494.0F, 818.0F})
        Panel(root, {76, y, 472, 1}, Keyline, 0);
    Label(root, {76, 332, 472, 24}, std::wstring(text.score), 15, 700, v::TextAlignment::Leading, Muted);
    Label(root, {76, 358, 472, 80}, L"—", 52, 800, v::TextAlignment::Leading, Accent);
    Label(root, {76, 442, 140, 40}, std::wstring(text.accuracy), 18, 600);
    Label(root, {214, 440, 332, 46}, Format(text.percent, result_.summary.accuracy.Percent()), 30, 700,
          v::TextAlignment::Trailing);
    BuildJudgements(root);
    BuildGroups(root);
}
void TaikoResultView::BuildMetadata(v::Visual2DNode &root)
{
    const auto &text = finger_drum::texts::Result(texts_.CurrentLanguage());
    static_cast<void>(v::CreateSprite(root, Bounds({609.75F, 234, 508.5F, 336}), rankFrame_, "Result.RankFrame"));
    Label(root, {764, 290, 200, 24}, std::wstring(text.rank), 13, 700, v::TextAlignment::Center, Muted);
    Label(root, {728, 316, 272, 166}, L"—", 104, 800, v::TextAlignment::Center, Accent);
    Label(root, {660, 616, 408, 22}, std::wstring(text.song), 13, 500, v::TextAlignment::Center, Muted);
    Label(root, {648, 644, 432, 46}, context_.songTitle.empty() ? L"—" : context_.songTitle, 28, 700,
          v::TextAlignment::Center);
    Label(root, {660, 714, 408, 22}, std::wstring(text.pattern), 13, 700, v::TextAlignment::Center, Muted);
    Label(root, {648, 740, 432, 42}, Decode(result_.summary.patternName), 22, 700, v::TextAlignment::Center);
    Label(root, {660, 804, 408, 22}, std::wstring(text.creator), 13, 500, v::TextAlignment::Center, Muted);
    Label(root, {648, 832, 432, 36}, context_.patternCreator.empty() ? L"—" : context_.patternCreator, 19, 600,
          v::TextAlignment::Center);
}
void TaikoResultView::BuildActions(v::Visual2DNode &root)
{
    const auto &text = finger_drum::texts::Result(texts_.CurrentLanguage());
    auto &replay = Button(root, {1152, 914, 352, 62}, "Result.Replay", std::wstring(text.replay), true);
    replay.SetEnabled(false);
    replay.Children().back()->SetEnabled(false);
    auto &retry = Button(root, {1520, 914, 352, 62}, "Result.Retry", std::wstring(text.retry), false);
    retry.SetEnabled(context_.canRetry);
    retry.Children().back()->SetEnabled(context_.canRetry);
    retryId_ = retry.Id();
    // Back remains reachable at the bottom-left even when the result layout
    // scales down to fit a portrait viewport.
    auto &back = Button(canvas_->AnchorNode(v::Anchor::BottomLeft), {48, 1006, 192, 62}, "Result.Back", L"", false);
    auto &caption = *back.GetComponent<v::TextVisualComponent>();
    caption.SetText(std::wstring(text.back));
    caption.SetFont(font_);
    caption.SetFontSize(21);
    caption.SetFontWeight(800);
    caption.SetHorizontalAlignment(v::TextAlignment::Center);
    caption.SetTextColor(Color(0xFFFFFF));
    caption.SetContentBounds({0, 27, 192, 30});
    auto &shortcut = v::CreateLabel(back, {0, 7, 192, 19}, std::wstring(text.backShortcut), "Result.BackShortcut");
    auto &hint = *shortcut.GetComponent<v::TextVisualComponent>();
    hint.SetFont(font_);
    hint.SetFontSize(10);
    hint.SetFontWeight(600);
    hint.SetHorizontalAlignment(v::TextAlignment::Center);
    hint.SetTextColor(Color(0xFFFFFF));
    backId_ = back.Id();
}
