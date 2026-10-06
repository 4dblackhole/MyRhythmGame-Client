#include "EditorView.h"
#include <algorithm>
#include <sstream>
using namespace editor_ui;

void EditorView::DrawScore()
{
    const auto &text = Texts();
    DrawTools();
    state_.Mode().DrawChart(*this, state_);
    if (state_.Mode().HasPendingPlacement())
        Text({180, 740, 1550, 32}, std::wstring(text.finishLongNote), 22, Blue);
    DrawTimeline();
    state_.Mode().DrawToolMenu(*this, state_, texts_.CurrentLanguage());
}

void EditorView::DrawTools()
{
    const auto &text = Texts();
    Button(
        {112, 76, 180, 32}, std::wstring(text.overview), [this] { state_.SetRealtime(false); },
        !state_.Score().realtime);
    Button(
        {292, 76, 180, 32}, std::wstring(text.realtime), [this] { state_.SetRealtime(true); }, state_.Score().realtime);
    Text({1080, 5 + EditorMenuBar::Height, 150, 24}, std::wstring(text.beatDivider), 17);
    Text({1080, 29 + EditorMenuBar::Height, 150, 26}, L"1/" + std::to_wstring(state_.Score().division), 18, Blue);
    for (int d = 1; d <= 16; ++d)
    {
        const float x = DivisionSliderRect.x + 8 + (DivisionSliderRect.width - 16) * (d - 1) / 15.0F;
        Box({x - 1, 32 + EditorMenuBar::Height, 2, 5}, Ink);
        Text({x - 19, 38 + EditorMenuBar::Height, 38, 18}, L"1/" + std::to_wstring(d), 12,
             state_.Score().division == d ? Blue : Ink);
    }
    Button({1740, 8 + EditorMenuBar::Height, 150, 40}, std::wstring(text.directInput), [this] {
        if (auto s =
                EditText(std::wstring(Texts().beatDivisionDialog), std::to_string(state_.Score().division), Texts()))
        {
            state_.SetDivision(Integer(*s));
        }
    });
    state_.Mode().DrawTools(*this, state_, texts_.CurrentLanguage());
}
