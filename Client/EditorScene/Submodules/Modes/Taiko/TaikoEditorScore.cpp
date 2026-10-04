#include "../../EditorSupport.h"
#include "../../EditorWorkspace.h"
#include "TaikoEditorMode.h"
#include <algorithm>
using namespace editor_ui;

namespace
{
    constexpr v::Color Don{1, .33F, .43F, 1}, Kat{.10F, .76F, .82F, 1}, Gold{.96F, .80F, .29F, 1};
}

void TaikoEditorMode::ScrollScore(EditorWorkspace &state, int direction)
{
    state.firstMeasure = std::max<std::int64_t>(0, state.firstMeasure + direction * 4);
}

void TaikoEditorMode::DrawChart(IEditorModeCanvas &canvas, EditorWorkspace &state)
{
    canvas.Box({112, 116, 1784, 678}, Paper);
    noteHits_.clear();
    realtimeGrid_.clear();
    const auto &timeline = state.editor->Timeline();
    const int subdivisionsPerWholeNote = state.division * 4;
    const auto xAt = [&](chart::MusicalPosition p) {
        return 205.0F + static_cast<float>((p.measure - state.firstMeasure) % 4) * 401 +
               static_cast<float>(p.fraction.Value() / timeline.MeasureLength(p.measure).Value()) * 401;
    };
    if (!state.realtime)
    {
        // Rational score grid, followed by note intervals and BPM labels.
        for (int row = 0; row < 6; ++row)
        {
            const float y = 150.0F + row * 100;
            canvas.Box({164, y, 1684, 48}, {.957F, .984F, 1, 1}, 3);
            for (int col = 0; col < 4; ++col)
            {
                const auto m = state.firstMeasure + row * 4 + col;
                const float x = 205.0F + col * 401;
                canvas.Text({x, y - 25, 140, 22},
                            std::to_wstring(m + 1) + L"  " + Wide(Fraction(timeline.MeasureLength(m))), 13);
                const auto count = static_cast<int>(
                    std::min(4096.0L, std::ceil(timeline.MeasureLength(m).Value() * subdivisionsPerWholeNote)));
                for (int i = 0; i < count; ++i)
                {
                    const float gx = x + static_cast<float>(chart::Rational{i, subdivisionsPerWholeNote}.Value() /
                                                            timeline.MeasureLength(m).Value()) *
                                             401;
                    canvas.Box({gx, y, i == 0 ? 2.0F : 1.0F, 48}, i == 0 ? Blue : Pale);
                }
            }
        }
        std::optional<chart::PatternNote> head;
        for (const auto &note : state.editor->Notes())
        {
            const auto &n = note.note;
            if (n.actionType == 1 && !head)
                head = n;
            if (n.actionType == 2 && head && head->keyType == n.keyType)
            {
                for (int row = 0; row < 6; ++row)
                {
                    const auto begin = state.firstMeasure + row * 4, end = begin + 4;
                    if (n.position.measure < begin || head->position.measure >= end)
                        continue;
                    const float left = head->position.measure < begin ? 205 : xAt(head->position),
                                right = n.position.measure >= end ? 1809 : xAt(n.position);
                    canvas.Box({left, 166.0F + row * 100, std::max(0.0F, right - left), 16}, Gold, 8);
                }
                head.reset();
            }
            if (n.position.measure < state.firstMeasure || n.position.measure >= state.firstMeasure + 24)
                continue;
            const float x = xAt(n.position),
                        y = 174 + static_cast<float>((n.position.measure - state.firstMeasure) / 4) * 100;
            Circle(canvas, x, y, n.keyType, (n.keyType >= 3 && n.keyType <= 5) ? 15.0F : 10.0F);
            noteHits_.push_back({{x, y}, n.sourceOrder});
        }
        for (const auto &t : state.editor->Pattern().timing)
            if (t.type == chart::TimingDirectiveType::Bpm && t.position.measure >= state.firstMeasure &&
                t.position.measure < state.firstMeasure + 24)
                canvas.Text({xAt(t.position),
                             200 + static_cast<float>((t.position.measure - state.firstMeasure) / 4) * 100, 140, 24},
                            L"BPM " + std::to_wstring(static_cast<int>(t.value)), 15, Blue);
    }
    else
    {
        // At Base BPM, a sixteenth note spans 85% of the normal head diameter.
        // Compiled note times preserve the actual spacing through BPM changes.
        canvas.Box({164, 351, 1684, 228}, {.055F, .060F, .12F, 1});
        canvas.Box({230, 351, 3, 228}, White);
        constexpr float normalHeadRadius = 36.0F;
        constexpr float pixelsPerWholeNote = normalHeadRadius * 2.0F * 16.0F * .85F;
        const auto currentTime = finger_drum::rhythm::RhythmTime{
            static_cast<finger_drum::rhythm::RhythmTime::rep>(std::llround(state.timeMs * 1000.0))};
        const auto xAtTime = [currentTime, pixelsPerWholeNote, baseBpm = timeline.BaseBpm()](const auto timing,
                                                                                             const double speed) {
            return 230.0F + static_cast<float>(timing.count() - currentTime.count()) *
                                static_cast<float>(baseBpm / 240'000'000.0) * pixelsPerWholeNote *
                                static_cast<float>(speed);
        };
        const auto currentMeasure = MeasureNearTime(timeline, state.timeMs);
        const auto gridBegin = std::max<std::int64_t>(0, currentMeasure - 1);
        for (std::int64_t m = gridBegin; m < currentMeasure + 32; ++m)
        {
            const int count = static_cast<int>(
                std::min(4096.0L, std::ceil(timeline.MeasureLength(m).Value() * subdivisionsPerWholeNote)));
            for (int i = 0; i < count; ++i)
            {
                chart::MusicalPosition p{m, {i, subdivisionsPerWholeNote}};
                const auto speed =
                    timeline.EffectValueAt(state.editor->Effects(), chart::EffectCommandType::NoteSpeed, p) *
                    timeline.EffectValueAt(state.editor->Effects(), chart::EffectCommandType::ScrollSpeed, p);
                const float x = xAtTime(timeline.Compile(p), speed);
                if (x < 164 || x > 1848)
                    continue;
                if (i != 0 || timeline.EffectValueAt(state.editor->Effects(),
                                                     chart::EffectCommandType::MeasureLineVisible, p) >= .5)
                    canvas.Box({x, 361, i == 0 ? 3.0F : 1.0F, 202}, {.3F, .34F, .42F, i == 0 ? 1.0F : .45F});
                realtimeGrid_.push_back({x, p});
            }
        }
        std::optional<float> headX;
        double headSpeed = 1;
        for (const auto &n : state.editor->Notes())
        {
            const auto speed = n.note.actionType == 2 && headX ? headSpeed : n.scrollMultiplier;
            const float x = xAtTime(n.timing, speed);
            if (n.note.actionType == 1)
            {
                headX = x;
                headSpeed = n.scrollMultiplier;
            }
            if (n.note.actionType == 2 && headX)
            {
                const float left = std::max(*headX, 164.0F), right = std::min(x, 1848.0F);
                if (right > left)
                    canvas.Box({left, 449, right - left, 32}, Gold, 16);
                headX.reset();
            }
            if (x < 200 || x > 1810)
                continue;
            Circle(canvas, x, 465, n.note.keyType,
                   (n.note.keyType >= 3 && n.note.keyType <= 5) ? 52.0F : normalHeadRadius);
            noteHits_.push_back({{x, 465}, n.note.sourceOrder});
        }
        for (const auto &t : state.editor->Pattern().timing)
        {
            if (t.type != chart::TimingDirectiveType::Bpm)
                continue;
            const float x = xAtTime(timeline.Compile(t.position), 1.0);
            if (x >= 164 && x <= 1710)
                canvas.Text({x, 320, 140, 28}, L"BPM " + std::to_wstring(static_cast<int>(t.value)), 18, Blue);
        }
    }
}

void TaikoEditorMode::EditScore(EditorWorkspace &state, v::Point point, bool erase)
{
    if (erase)
    {
        const auto nearest = std::ranges::min_element(
            noteHits_, {}, [point](const NoteHit &n) { return std::hypot(n.point.x - point.x, n.point.y - point.y); });
        if (nearest != noteHits_.end() &&
            std::hypot(nearest->point.x - point.x, nearest->point.y - point.y) < (state.realtime ? 55 : 24))
            state.editor->DeleteNote(nearest->order);
        state.rebuild = true;
        return;
    }
    chart::MusicalPosition p;
    if (state.realtime)
    {
        if (point.y < 351 || point.y > 579 || realtimeGrid_.empty())
            return;
        p = std::ranges::min_element(realtimeGrid_, {}, [point](const auto &g) {
                return std::abs(g.first - point.x);
            })->second;
    }
    else
    {
        const int row = static_cast<int>((point.y - 150) / 100), col = static_cast<int>((point.x - 205) / 401);
        if (point.x < 205 || point.x >= 1809 || point.y < 150 || row >= 6 || point.y - (150 + row * 100) > 48)
            return;
        p.measure = state.firstMeasure + row * 4 + col;
        const auto length = state.editor->Timeline().MeasureLength(p.measure);
        const int subdivisionsPerWholeNote = state.division * 4;
        const auto tick = static_cast<std::int64_t>(
            std::llround((point.x - 205 - col * 401) / 401.0 * length.Value() * subdivisionsPerWholeNote));
        p.fraction = chart::Rational{tick, subdivisionsPerWholeNote};
        if (p.fraction >= length)
            p.fraction = chart::Rational{std::max<std::int64_t>(0, tick - 1), subdivisionsPerWholeNote};
    }
    state.PlaceNote(p);
}

void TaikoEditorMode::Circle(IEditorModeCanvas &canvas, float x, float y, int type, float radius)
{
    const auto color = type == 2 || type == 4 ? Kat : type == 5 ? v::Color{.65F, .30F, .85F, 1} : type > 5 ? Gold : Don;
    canvas.Box({x - radius - 2, y - radius - 2, radius * 2 + 4, radius * 2 + 4}, White, radius + 2);
    canvas.Box({x - radius, y - radius, radius * 2, radius * 2}, color, radius);
}
