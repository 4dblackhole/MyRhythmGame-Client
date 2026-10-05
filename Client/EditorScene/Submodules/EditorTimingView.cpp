#include "EditorView.h"
#include <algorithm>
#include <sstream>
using namespace editor_ui;

void EditorView::DrawTiming()
{
    const auto &text = Texts();
    Text({136, 105, 1000, 40}, std::wstring(text.timingTitle), 28);
    Box({132, 173, 920, 650}, White, 12);
    Box({1080, 173, 780, 520}, White, 12);
    Text({156, 198, 830, 38}, std::wstring(text.timingColumns), 22);
    const auto &timing = state_.Document().Timing();
    for (std::size_t i = state_.listOffset; i < timing.size() && i < state_.listOffset + 11; ++i)
    {
        const auto &t = timing[i];
        const float y = 250 + static_cast<float>(i - state_.listOffset) * 46;
        const auto value =
            t.type == chart::TimingDirectiveType::MeasureLength ? Fraction(t.ratio) : std::to_string(t.value);
        Button({156, y, 715, 40},
               std::to_wstring(t.position.measure + 1) + L"    " + Wide(Fraction(t.position.fraction)) + L"    " +
                   (t.type == chart::TimingDirectiveType::Bpm             ? L"BPM"
                    : t.type == chart::TimingDirectiveType::MeasureLength ? std::wstring(text.measureType)
                                                                          : std::wstring(text.delayType)) +
                   L"    " + Wide(value),
               [this, t] {
                   state_.timingForm.measure = std::to_string(t.position.measure + 1);
                   state_.timingForm.fraction = Fraction(t.position.fraction);
                   if (t.type == chart::TimingDirectiveType::Bpm)
                       state_.timingForm.bpm = std::to_string(t.value);
                   if (t.type == chart::TimingDirectiveType::MeasureLength)
                       state_.timingForm.measureLength = Fraction(t.ratio);
                   state_.RequestRebuild();
               });
        Button({883, y, 125, 40}, std::wstring(text.remove), [this, i] { state_.RemoveTiming(i); });
    }
    Field({1104, 270, 180, 42}, text.startMeasure.data(), state_.timingForm.measure);
    Field({1310, 270, 210, 42}, text.changePosition.data(), state_.timingForm.fraction);
    Field({1550, 270, 275, 42}, L"BPM", state_.timingForm.bpm);
    Button({1540, 350, 285, 42}, std::wstring(text.addOrUpdateBpm), [this] { state_.ApplyBpm(); }, true);
    Field({1104, 460, 320, 42}, text.measureLength.data(), state_.timingForm.measureLength);
    Button({1470, 460, 355, 42}, std::wstring(text.applyMeasureLength), [this] { state_.ApplyMeasureLength(); }, true);
    Text({1104, 545, 715, 120}, std::wstring(text.timingHelp), 19);
}
