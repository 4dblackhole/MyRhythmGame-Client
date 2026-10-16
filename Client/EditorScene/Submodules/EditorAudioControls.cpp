#include "EditorView.h"
#include <algorithm>
#include <format>

using namespace editor_ui;

void EditorView::DrawAudioControls()
{
    const auto offset = state_.Document().Pattern().patternOffsetMilliseconds;
    Button({132, 1000, 340, 26}, std::format(L"{}: {:.3f}", Texts().offsetMilliseconds, offset),
           [this]
           {
               if (auto value =
                       EditText(std::wstring(Texts().offsetMilliseconds),
                                std::to_string(state_.Document().Pattern().patternOffsetMilliseconds), Texts()))
                   state_.SetPatternOffset(Number(*value));
           });
    Button({480, 1000, 72, 26}, std::wstring(texts_.Get("Editor.offsetDecrease")),
           [this] { state_.SetPatternOffset(state_.Document().Pattern().patternOffsetMilliseconds - 1); });
    Button({560, 1000, 72, 26}, std::wstring(texts_.Get("Editor.offsetIncrease")),
           [this] { state_.SetPatternOffset(state_.Document().Pattern().patternOffsetMilliseconds + 1); });
    Button(
        {656, 1000, 280, 26},
        std::wstring(texts_.Get(state_.Metronome().Enabled() ? "Editor.metronomeOn" : "Editor.metronomeOff")),
        [this] { state_.ToggleMetronome(); }, state_.Metronome().Enabled());
    Text({956, 1000, 920, 26}, std::wstring(texts_.Get("Editor.offsetAudioHelp")), 15);
}

void EditorView::DrawAudioAlignment(double begin)
{
    constexpr float left = 205, trackWidth = 1565;
    const double end = begin + state_.audioWindow;
    const auto xAt = [&](double seconds)
    { return left + static_cast<float>((seconds - begin) / state_.audioWindow) * trackWidth; };
    const auto &beats = state_.Metronome().Beats();
    for (auto beat = std::ranges::lower_bound(beats, begin, {}, &EditorMetronome::Beat::seconds);
         beat != beats.end() && beat->seconds < end; ++beat)
    {
        const float x = xAt(beat->seconds);
        Box({x, 149, beat->accent ? 2.0F : 1.0F, 4}, Blue);
        Box({x, 153, 1, 166}, {Blue.red, Blue.green, Blue.blue, beat->accent ? .5F : .2F});
    }
    const auto &markers = state_.Analysis().Markers();
    for (auto marker = std::ranges::lower_bound(markers, begin, {}, &AudioMarker::seconds);
         marker != markers.end() && marker->seconds < end; ++marker)
        Box({xAt(marker->seconds), 635, 2, 15}, Blue);
}
