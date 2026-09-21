#include "EditorView.h"
#include "EditorTool.h"
#include <algorithm>
#include <sstream>
using namespace editor_ui;

void EditorView::DrawScore()
{
    DrawTools();
    DrawChartContent();
    if (state_.pending)
        Text({180, 740, 1550, 32}, L"롱노트 끝 위치를 클릭하세요 · 도구 변경/Escape: 취소", 22,
             Blue);
    DrawAudio();
    DrawVariantMenu();
}

void EditorView::DrawTools()
{
    Button(
        {112, 76, 180, 32}, L"전체 악보 뷰",
        [this] {
            state_.realtime = false;
            state_.rebuild = true;
        },
        !state_.realtime);
    Button(
        {292, 76, 180, 32}, L"실시간 뷰",
        [this] {
            state_.realtime = true;
            state_.rebuild = true;
        },
        state_.realtime);
    Text({1080, 12, 160, 32}, L"박자 디바이더", 18);
    for (int i = 0; i < 6; ++i)
    {
        const int d = 4 << i;
        Button(
            {1240.0F + i * 80, 8, 72, 40}, L"1/" + std::to_wstring(d),
            [this, d] {
                state_.division = d;
                state_.rebuild = true;
            },
            state_.division == d);
    }
    Button({1740, 8, 150, 40}, L"직접 입력", [this] {
        if (auto s = EditText(L"박자 분할 (1/N)", std::to_string(state_.division)))
        {
            const auto d = Integer(*s);
            if (d < 1 || d > 1024)
                throw std::invalid_argument("Division must be 1..1024.");
            state_.division = static_cast<int>(d);
            state_.rebuild = true;
        }
    });
    Box({24, 76, 70, 918}, Paper, 18);
    Text({38, 86, 55, 25}, L"도구", 16);
    const std::array<std::wstring, 7> names{L"선택",
                                            state_.smallTool == 2 ? L"캇" : L"동",
                                            state_.bigTool == 5   ? L"보라노트"
                                            : state_.bigTool == 4 ? L"큰 캇"
                                                                  : L"큰 동",
                                            state_.rollTool >= 17 ? L"버즈"
                                            : state_.rollTool == 12 || state_.rollTool == 14
                                                ? L"틱롤"
                                                : L"롤노트",
                                            state_.focusTool == 16 ? L"뎅뎅" : L"풍선",
                                            L"BPM",
                                            L"마디"};
    const std::array<int, 7> ids{
        0, state_.smallTool, state_.bigTool, state_.rollTool, state_.focusTool, -2, -3};
    for (std::size_t i = 0; i < ids.size(); ++i)
    {
        const float y = 122 + static_cast<float>(i) * 80;
        Button(
            {35, y, 48, 48}, i < 5 ? L"" : names[i], [this, id = ids[i]] { state_.SelectTool(id); },
            state_.tool == ids[i]);
        if (i > 0 && i < 5)
            Circle(59, y + 24, ids[i], i == 2 ? 15.0F : 10.0F);
        Text({27, y + 49, 67, 25}, names[i], 13);
    }
}

void EditorView::DrawChartContent()
{
    Box({112, 116, 1784, 678}, Paper);
    noteHits.clear();
    realtimeGrid.clear();
    const auto &timeline = state_.editor->Timeline();
    const auto xAt = [&](chart::MusicalPosition p) {
        return 205.0F + static_cast<float>((p.measure - state_.firstMeasure) % 4) * 401 +
               static_cast<float>(p.fraction.Value() / timeline.MeasureLength(p.measure).Value()) *
                   401;
    };
    if (!state_.realtime)
    {
        // Rational score grid, followed by note intervals and BPM labels.
        for (int row = 0; row < 6; ++row)
        {
            const float y = 150.0F + row * 100;
            Box({164, y, 1684, 48}, {.957F, .984F, 1, 1}, 3);
            for (int col = 0; col < 4; ++col)
            {
                const auto m = state_.firstMeasure + row * 4 + col;
                const float x = 205.0F + col * 401;
                Text({x, y - 25, 140, 22},
                     std::to_wstring(m + 1) + L"  " + Wide(Fraction(timeline.MeasureLength(m))),
                     13);
                const auto count = static_cast<int>(std::min(
                    4096.0L, std::ceil(timeline.MeasureLength(m).Value() * state_.division)));
                for (int i = 0; i < count; ++i)
                {
                    const float gx =
                        x + static_cast<float>(chart::Rational{i, state_.division}.Value() /
                                               timeline.MeasureLength(m).Value()) *
                                401;
                    Box({gx, y, i == 0 ? 2.0F : 1.0F, 48}, i == 0 ? Blue : Pale);
                }
            }
        }
        std::optional<chart::PatternNote> head;
        for (const auto &note : state_.editor->Notes())
        {
            const auto &n = note.note;
            if (n.actionType == 1 && !head)
                head = n;
            if (n.actionType == 2 && head && head->keyType == n.keyType)
            {
                for (int row = 0; row < 6; ++row)
                {
                    const auto begin = state_.firstMeasure + row * 4, end = begin + 4;
                    if (n.position.measure < begin || head->position.measure >= end)
                        continue;
                    const float left = head->position.measure < begin ? 205 : xAt(head->position),
                                right = n.position.measure >= end ? 1809 : xAt(n.position);
                    Box({left, 166.0F + row * 100, std::max(0.0F, right - left), 16}, Gold, 8);
                }
                head.reset();
            }
            if (n.position.measure < state_.firstMeasure ||
                n.position.measure >= state_.firstMeasure + 24)
                continue;
            const float x = xAt(n.position),
                        y = 174 +
                            static_cast<float>((n.position.measure - state_.firstMeasure) / 4) *
                                100;
            Circle(x, y, n.keyType, (n.keyType >= 3 && n.keyType <= 5) ? 15.0F : 10.0F);
            noteHits.push_back({{x, y}, n.sourceOrder});
        }
        for (const auto &t : state_.editor->Pattern().timing)
            if (t.type == chart::TimingDirectiveType::Bpm &&
                t.position.measure >= state_.firstMeasure &&
                t.position.measure < state_.firstMeasure + 24)
                Text(
                    {xAt(t.position),
                     200 + static_cast<float>((t.position.measure - state_.firstMeasure) / 4) * 100,
                     140, 24},
                    L"BPM " + std::to_wstring(static_cast<int>(t.value)), 15, Blue);
    }
    else
    {
        // The realtime view projects the same cached note times around the
        // selected time. A long note uses its head's speed for both endpoints.
        Box({164, 351, 1684, 228}, {.055F, .060F, .12F, 1});
        Box({230, 351, 3, 228}, White);
        constexpr double pixelsPerMs = .30;
        const auto currentMeasure = MeasureNearTime(timeline, state_.timeMs);
        const auto gridBegin = std::max<std::int64_t>(0, currentMeasure - 1);
        for (std::int64_t m = gridBegin; m < currentMeasure + 32; ++m)
        {
            const int count = static_cast<int>(
                std::min(4096.0L, std::ceil(timeline.MeasureLength(m).Value() * state_.division)));
            for (int i = 0; i < count; ++i)
            {
                chart::MusicalPosition p{m, {i, state_.division}};
                const auto speed = timeline.EffectValueAt(state_.editor->Effects(),
                                                          chart::EffectCommandType::NoteSpeed, p) *
                                   timeline.EffectValueAt(state_.editor->Effects(),
                                                          chart::EffectCommandType::ScrollSpeed, p);
                const float x = 230 + static_cast<float>(
                                          (timeline.Compile(p).count() / 1000.0 - state_.timeMs) *
                                          pixelsPerMs * speed);
                if (x < 164 || x > 1848)
                    continue;
                if (i != 0 ||
                    timeline.EffectValueAt(state_.editor->Effects(),
                                           chart::EffectCommandType::MeasureLineVisible, p) >= .5)
                    Box({x, 361, i == 0 ? 2.0F : 1.0F, 202},
                        {.3F, .34F, .42F, i == 0 ? 1.0F : .45F});
                realtimeGrid.push_back({x, p});
            }
        }
        std::optional<float> headX;
        double headSpeed = 1;
        for (const auto &n : state_.editor->Notes())
        {
            const auto speed = n.note.actionType == 2 && headX ? headSpeed : n.scrollMultiplier;
            const float x = 230 + static_cast<float>((n.timing.count() / 1000.0 - state_.timeMs) *
                                                     pixelsPerMs * speed);
            if (n.note.actionType == 1)
            {
                headX = x;
                headSpeed = n.scrollMultiplier;
            }
            if (n.note.actionType == 2 && headX)
            {
                const float left = std::max(*headX, 164.0F), right = std::min(x, 1848.0F);
                if (right > left)
                    Box({left, 449, right - left, 32}, Gold, 16);
                headX.reset();
            }
            if (x < 200 || x > 1810)
                continue;
            Circle(x, 465, n.note.keyType,
                   (n.note.keyType >= 3 && n.note.keyType <= 5) ? 52.0F : 36.0F);
            noteHits.push_back({{x, 465}, n.note.sourceOrder});
        }
        for (const auto &t : state_.editor->Pattern().timing)
        {
            if (t.type != chart::TimingDirectiveType::Bpm)
                continue;
            const float x =
                230 +
                static_cast<float>((timeline.Compile(t.position).count() / 1000.0 - state_.timeMs) *
                                   pixelsPerMs);
            if (x >= 164 && x <= 1710)
                Text({x, 320, 140, 28}, L"BPM " + std::to_wstring(static_cast<int>(t.value)), 18,
                     Blue);
        }
    }
}

void EditorView::DrawVariantMenu()
{
    if (state_.popup >= 0)
    {
        std::vector<std::pair<std::wstring, int>> choices;
        for (const auto &tool : editor_tools::Tools)
        {
            using Group = editor_tools::Group;
            const bool matches = state_.popup == 1 ? tool.group == Group::Small
                                 : state_.popup == 2
                                     ? tool.group == Group::Big
                                     : tool.group == Group::Roll || tool.group == Group::Focus;
            if (matches)
                choices.emplace_back(tool.label, tool.id);
        }
        for (std::size_t i = 0; i < choices.size(); ++i)
            Button({105, 200 + static_cast<float>(i) * 40, 220, 38}, choices[i].first,
                   [this, id = choices[i].second] { state_.SelectTool(id); });
    }
}
