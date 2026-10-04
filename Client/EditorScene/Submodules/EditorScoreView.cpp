#include "EditorView.h"
#include <algorithm>
#include <sstream>
using namespace editor_ui;

void EditorView::DrawScore()
{
    const auto &text = Texts();
    DrawTools();
    state_.mode->DrawChart(*this, state_);
    if (state_.mode->HasPendingPlacement())
        Text({180, 740, 1550, 32}, std::wstring(text.finishLongNote), 22, Blue);
    DrawTimeline();
    state_.mode->DrawToolMenu(*this, state_, texts_.CurrentLanguage());
}

void EditorView::DrawTools()
{
    const auto &text = Texts();
    Button(
        {112, 76, 180, 32}, std::wstring(text.overview),
        [this] {
            state_.realtime = false;
            state_.rebuild = true;
        },
        !state_.realtime);
    Button(
        {292, 76, 180, 32}, std::wstring(text.realtime),
        [this] {
            state_.realtime = true;
            state_.rebuild = true;
        },
        state_.realtime);
    Text({1080, 5, 150, 24}, std::wstring(text.beatDivider), 17);
    Text({1080, 29, 150, 26}, L"1/" + std::to_wstring(state_.division), 18, Blue);
    for (int d = 1; d <= 16; ++d)
    {
        const float x = DivisionSliderRect.x + 8 + (DivisionSliderRect.width - 16) * (d - 1) / 15.0F;
        Box({x - 1, 32, 2, 5}, Ink);
        Text({x - 19, 38, 38, 18}, L"1/" + std::to_wstring(d), 12, state_.division == d ? Blue : Ink);
    }
    Button({1740, 8, 150, 40}, std::wstring(text.directInput), [this] {
        if (auto s = EditText(std::wstring(Texts().beatDivisionDialog), std::to_string(state_.division), Texts()))
        {
            const auto d = Integer(*s);
            if (d < 1 || d > 1024)
                throw std::invalid_argument("Division must be 1..1024.");
            state_.division = static_cast<int>(d);
            state_.rebuild = true;
        }
    });
    state_.mode->DrawTools(*this, state_, texts_.CurrentLanguage());
}
