#include "EditorView.h"
#include <algorithm>
#include <sstream>
using namespace editor_ui;

void EditorView::DrawTiming()
{
    Text({136, 105, 1000, 40}, L"박자표 · BPM / 마디 길이", 28);
    Box({132, 173, 920, 650}, White, 12);
    Box({1080, 173, 780, 520}, White, 12);
    Text({156, 198, 830, 38}, L"마디     위치          종류            값", 22);
    const auto &timing = state_.editor->Pattern().timing;
    for (std::size_t i = state_.listOffset; i < timing.size() && i < state_.listOffset + 11; ++i)
    {
        const auto &t = timing[i];
        const float y = 250 + static_cast<float>(i - state_.listOffset) * 46;
        const auto value = t.type == chart::TimingDirectiveType::MeasureLength
                               ? Fraction(t.ratio)
                               : std::to_string(t.value);
        Button({156, y, 715, 40},
               std::to_wstring(t.position.measure + 1) + L"    " +
                   Wide(Fraction(t.position.fraction)) + L"    " +
                   (t.type == chart::TimingDirectiveType::Bpm             ? L"BPM"
                    : t.type == chart::TimingDirectiveType::MeasureLength ? L"Measure"
                                                                          : L"Delay") +
                   L"    " + Wide(value),
               [this, t] {
                   state_.timingFields[0] = std::to_string(t.position.measure + 1);
                   state_.timingFields[1] = Fraction(t.position.fraction);
                   if (t.type == chart::TimingDirectiveType::Bpm)
                       state_.timingFields[2] = std::to_string(t.value);
                   if (t.type == chart::TimingDirectiveType::MeasureLength)
                       state_.timingFields[3] = Fraction(t.ratio);
                   state_.rebuild = true;
               });
        Button({883, y, 125, 40}, L"삭제", [this, i] {
            auto p = state_.editor->Pattern();
            p.timing.erase(p.timing.begin() + i);
            state_.editor->Replace(p, state_.editor->Effects());
            state_.rebuild = true;
        });
    }
    Field({1104, 270, 180, 42}, L"시작 마디", state_.timingFields[0]);
    Field({1310, 270, 210, 42}, L"변경 위치 N/D", state_.timingFields[1]);
    Field({1550, 270, 275, 42}, L"BPM", state_.timingFields[2]);
    Button(
        {1540, 350, 285, 42}, L"BPM 추가 / 수정",
        [this] {
            auto p = state_.editor->Pattern();
            const auto pos = Position(state_.timingFields[0], state_.timingFields[1]);
            const auto bpm = Number(state_.timingFields[2]);
            if (bpm <= 0)
                throw std::invalid_argument("BPM must be positive.");
            std::erase_if(p.timing, [pos](const auto &d) {
                return d.position == pos && d.type == chart::TimingDirectiveType::Bpm;
            });
            p.timing.push_back({pos, chart::TimingDirectiveType::Bpm, bpm});
            state_.editor->Replace(p, state_.editor->Effects());
            state_.rebuild = true;
        },
        true);
    Field({1104, 460, 320, 42}, L"마디 길이 N/D", state_.timingFields[3]);
    Button(
        {1470, 460, 355, 42}, L"마디 길이 적용",
        [this] {
            state_.editor->SetMeasureLength(Integer(state_.timingFields[0]) - 1,
                                            Position("1", state_.timingFields[3]).fraction);
            state_.rebuild = true;
        },
        true);
    Text({1104, 545, 715, 120},
         L"넘친 노트는 초과 박자를 유지해 다음 마디로 이동합니다.\n마디 밖의 BPM/이펙트는 먼저 "
         L"이동해 주세요.",
         19);
}
