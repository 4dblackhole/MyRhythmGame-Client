#include "EditorView.h"
#include <algorithm>
#include <iomanip>
#include <sstream>
using namespace editor_ui;

namespace
{
    std::wstring TimeLabel(const double milliseconds)
    {
        const auto rounded = static_cast<std::int64_t>(std::llround(milliseconds));
        const auto absolute = rounded < 0 ? -rounded : rounded;
        std::wostringstream text;
        if (rounded < 0)
            text << L'−';
        text << absolute / 60'000 << L':' << std::setfill(L'0') << std::setw(2) << absolute / 1000 % 60 << L'.'
             << std::setw(3) << absolute % 1000;
        return text.str();
    }
} // namespace

void EditorView::SyncTimelineSlider()
{
    const auto [begin, end] = state_.TimelineRangeMilliseconds();
    timelineSliderNode_->GetComponent<v::SliderBehaviorComponent>()->SetValue(
        static_cast<float>(std::clamp((state_.TimeMilliseconds() - begin) / (end - begin), 0.0, 1.0)));
}

void EditorView::DrawTimeline()
{
    const auto &text = Texts();
    const float top = state_.Tab() == EditorTab::Pattern ? 806.0F : 884.0F;
    Box({112, top, 1784, 1026 - top}, Paper, 8);
    Text({132, top + 9, 700, 28}, std::wstring(text.timeline), 20);
    Text({850, top + 9, 700, 28}, TimeLabel(state_.TimeMilliseconds()), 20, Blue);
    DrawTimeInput({1600, top + 3, 280, 32});

    const auto [begin, end] = state_.TimelineRangeMilliseconds();
    constexpr float left = TimelineSliderRect.x + 8;
    constexpr float trackWidth = TimelineSliderRect.width - 16;
    for (int tick = 0; tick <= 8; ++tick)
    {
        const float x = left + trackWidth * tick / 8;
        Box({x, 962, 1, 8}, Ink);
        Text({std::clamp(x - 62, left, left + trackWidth - 125), 975, 125, 24},
             TimeLabel(std::lerp(begin, end, tick / 8.0)), 15);
    }
}

void EditorView::DrawTimeInput(v::Rect rect)
{
    Button(rect, std::wstring(Texts().currentTime), [this] {
        if (auto value =
                EditText(std::wstring(Texts().currentTime), std::to_string(state_.TimeMilliseconds()), Texts()))
        {
            state_.Seek(Number(*value));
        }
    });
}
