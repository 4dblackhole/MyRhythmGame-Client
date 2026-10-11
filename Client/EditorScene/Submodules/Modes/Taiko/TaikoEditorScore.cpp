#include "../../EditorSupport.h"
#include "../../EditorTime.h"
#include "TaikoEditorMode.h"
#include <algorithm>
#include <limits>
using namespace editor_ui;

namespace
{
    constexpr v::Rect RealtimeLane{164, 351, 1684, 228};
    constexpr float ScoreLeft = 205, MeasureWidth = 401, RowTop = 150, RowHeight = 100, StaffHeight = 48;
    constexpr int MeasuresPerRow = 4, VisibleRows = 6;
    constexpr float ScoreRight = ScoreLeft + MeasureWidth * MeasuresPerRow;
    constexpr v::Color Don{1, .33F, .43F, 1}, Kat{.10F, .76F, .82F, 1}, Gold{.96F, .80F, .29F, 1};

    std::pair<finger_drum::rhythm::RhythmTime, finger_drum::rhythm::RhythmTime> GridTimeRange(
        const chart::IEditorDocument &document, std::int64_t measure, int subdivisions, int count)
    {
        const auto at = [&](int index) { return document.Timeline().Compile({measure, {index, subdivisions}}); };
        auto low = at(0), high = at(count - 1);
        if (low > high)
            std::swap(low, high);
        // Tempo/delay discontinuities can lie inside a measure, including negative delays.
        for (const auto &directive : document.Timing())
            if (directive.position.measure == measure)
            {
                const auto nearestIndex = static_cast<int>(std::clamp(
                    std::floor(directive.position.fraction.Value() * subdivisions), 0.0L,
                    static_cast<long double>(count - 1)));
                for (int index = std::max(0, nearestIndex - 1); index <= std::min(count - 1, nearestIndex + 1); ++index)
                {
                    const auto time = at(index);
                    low = std::min(low, time);
                    high = std::max(high, time);
                }
            }
        return {low, high};
    }

    finger_drum::rhythm::RhythmTime BoundedTime(long double value)
    {
        using Time = finger_drum::rhythm::RhythmTime;
        if (value <= static_cast<long double>(std::numeric_limits<Time::rep>::min()))
            return Time::min();
        if (value >= static_cast<long double>(std::numeric_limits<Time::rep>::max()))
            return Time::max();
        return Time{static_cast<Time::rep>(value)};
    }
} // namespace

void TaikoEditorMode::ScrollScore(IEditorContext &state, int direction)
{
    state.SetFirstMeasure(std::clamp<std::int64_t>(state.Score().firstMeasure + direction * MeasuresPerRow,
                                                 0, chart::MaximumMeasureCount - 1));
}

void TaikoEditorMode::DrawChart(IEditorModeCanvas &canvas, IEditorContext &state)
{
    canvas.Box({112, 116, 1784, 678}, Paper);
    noteHits_.clear();
    realtimeGrid_.clear();
    if (state.Score().realtime)
        DrawRealtime(canvas, state);
    else
        DrawOverview(canvas, state);
}

void TaikoEditorMode::DrawOverview(IEditorModeCanvas &canvas, IEditorContext &state)
{
    const auto &timeline = state.Document().Timeline();
    const int subdivisionsPerWholeNote = state.Score().division * 4;
    const auto xAt = [&](chart::MusicalPosition p) {
        return ScoreLeft + static_cast<float>((p.measure - state.Score().firstMeasure) % MeasuresPerRow) * MeasureWidth +
               static_cast<float>(p.fraction.Value() / timeline.MeasureLength(p.measure).Value()) * MeasureWidth;
    };
    // Rational score grid, followed by note intervals and BPM labels.
    for (int row = 0; row < VisibleRows; ++row)
    {
        const float y = RowTop + row * RowHeight;
        canvas.Box({164, y, 1684, StaffHeight}, {.957F, .984F, 1, 1}, 3);
        for (int col = 0; col < MeasuresPerRow; ++col)
        {
            const auto m = state.Score().firstMeasure + row * MeasuresPerRow + col;
            if (m >= chart::MaximumMeasureCount)
                break;
            const float x = ScoreLeft + col * MeasureWidth;
            canvas.Text({x, y - 25, 140, 22},
                        std::to_wstring(m + 1) + L"  " + Wide(Fraction(timeline.MeasureLength(m))), 13);
            const auto count = static_cast<int>(
                std::min(4096.0L, std::ceil(timeline.MeasureLength(m).Value() * subdivisionsPerWholeNote)));
            // Subpixel helper lines share a visible line; snapping still uses the full division.
            const int drawStride = std::max(1, static_cast<int>(std::ceil(count / static_cast<double>(MeasureWidth))));
            for (int i = 0; i < count; i += drawStride)
            {
                const float gx = x + static_cast<float>(chart::Rational{i, subdivisionsPerWholeNote}.Value() /
                                                        timeline.MeasureLength(m).Value()) *
                                         MeasureWidth;
                canvas.Box({gx, y, i == 0 ? 2.0F : 1.0F, StaffHeight}, i == 0 ? Blue : Pale);
            }
        }
    }
    std::optional<chart::PatternNote> head;
    for (const auto &note : state.Document().NotesInMeasures(state.Score().firstMeasure, state.Score().firstMeasure + MeasuresPerRow * VisibleRows))
    {
        const auto &n = note.note;
        if (n.actionType == 1 && !head)
            head = n;
        if (n.actionType == 2 && head && head->keyType == n.keyType)
        {
            for (int row = 0; row < VisibleRows; ++row)
            {
                const auto begin = state.Score().firstMeasure + row * MeasuresPerRow, end = begin + MeasuresPerRow;
                if (n.position.measure < begin || head->position.measure >= end)
                    continue;
                const float left = head->position.measure < begin ? ScoreLeft : xAt(head->position),
                            right = n.position.measure >= end ? ScoreRight : xAt(n.position);
                canvas.Box({left, 166.0F + row * RowHeight, std::max(0.0F, right - left), 16}, Gold, 8);
            }
            head.reset();
        }
        if (n.position.measure < state.Score().firstMeasure || n.position.measure >= state.Score().firstMeasure + MeasuresPerRow * VisibleRows)
            continue;
        const float x = xAt(n.position),
                    y = 174 + static_cast<float>((n.position.measure - state.Score().firstMeasure) / MeasuresPerRow) * RowHeight;
        Circle(canvas, x, y, n.keyType, (n.keyType >= 3 && n.keyType <= 5) ? 15.0F : 10.0F);
        noteHits_.push_back({{x, y}, n.sourceOrder});
    }
    for (const auto &t : state.Document().Timing())
        if (t.type == chart::TimingDirectiveType::Bpm && t.position.measure >= state.Score().firstMeasure &&
            t.position.measure < state.Score().firstMeasure + MeasuresPerRow * VisibleRows)
            canvas.Text({xAt(t.position),
                         200 + static_cast<float>((t.position.measure - state.Score().firstMeasure) / MeasuresPerRow) * RowHeight, 140,
                         24},
                        L"BPM " + std::to_wstring(static_cast<int>(t.value)), 15, Blue);
}

void TaikoEditorMode::DrawRealtime(IEditorModeCanvas &canvas, IEditorContext &state)
{
    const auto &timeline = state.Document().Timeline();
    const int subdivisionsPerWholeNote = state.Score().division * 4;
    // At Base BPM, a sixteenth note spans 85% of the normal head diameter.
    // Compiled note times preserve the actual spacing through BPM changes.
    canvas.Box(RealtimeLane, {.055F, .060F, .12F, 1});
    canvas.Box({230, 351, 3, 228}, White);
    constexpr float normalHeadRadius = 36.0F;
    constexpr float pixelsPerWholeNote = normalHeadRadius * 2.0F * 16.0F * .85F;
    const auto currentTime = editor_time::Microseconds(state.TimeMilliseconds());
    const auto xAtTime = [currentTime, pixelsPerWholeNote, &document = state.Document(), baseBpm = timeline.BaseBpm()](const auto timing,
                                                                                         const double speed) {
        return 230.0F +
               static_cast<float>(document.ScrollDistance(timing, currentTime)) *
                   static_cast<float>(baseBpm / 240'000'000.0) * pixelsPerWholeNote * static_cast<float>(speed);
    };
    const auto currentMeasure = editor_time::MeasureNearTime(timeline, state.TimeMilliseconds());
    const auto gridBegin = std::max<std::int64_t>(0, currentMeasure - 1);
    const long double pixelsPerMicrosecond = timeline.BaseBpm() / 240'000'000.0L * pixelsPerWholeNote;
    const auto minimumSpeed = state.Document().MinimumScrollMultiplier();
    const auto visibleBegin = BoundedTime(currentTime.count() +
        (RealtimeLane.x - 230.0L - 110) / (pixelsPerMicrosecond * minimumSpeed));
    const auto visibleEnd = BoundedTime(currentTime.count() +
        (RealtimeLane.x + RealtimeLane.width - 230.0L + 110) / (pixelsPerMicrosecond * minimumSpeed));
    // Drawing and input share the same visible snap positions.
    for (std::int64_t m = gridBegin; m < std::min(currentMeasure + 32, chart::MaximumMeasureCount); ++m)
    {
        const int count = static_cast<int>(
            std::min(4096.0L, std::ceil(timeline.MeasureLength(m).Value() * subdivisionsPerWholeNote)));
        const auto [firstTime, lastTime] = GridTimeRange(state.Document(), m, subdivisionsPerWholeNote, count);
        if (lastTime < visibleBegin || firstTime > visibleEnd)
            continue;
        float lastDrawnX = -std::numeric_limits<float>::infinity();
        for (int i = 0; i < count; ++i)
        {
            chart::MusicalPosition p{m, {i, subdivisionsPerWholeNote}};
            const auto speed =
                state.Document().EffectValueAt(chart::EffectCommandType::NoteSpeed, p);
            const float x = xAtTime(timeline.Compile(p), speed);
            if (x < RealtimeLane.x || x > RealtimeLane.x + RealtimeLane.width)
                continue;
            if ((i == 0 || std::abs(x - lastDrawnX) >= 1) &&
                (i != 0 || state.Document().EffectValueAt(chart::EffectCommandType::MeasureLineVisible, p) >= .5))
            {
                canvas.Box({x, 361, i == 0 ? 3.0F : 1.0F, 202}, {.3F, .34F, .42F, i == 0 ? 1.0F : .45F});
                lastDrawnX = x;
            }
            realtimeGrid_.push_back({x, p});
        }
    }
    // A linked tail uses its head's speed so the whole interval stays aligned.
    std::optional<float> headX;
    double headSpeed = 1;
    for (const auto &n : state.Document().NotesInTimeRange(visibleBegin, visibleEnd))
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
            const float left = std::max(*headX, RealtimeLane.x),
                        right = std::min(x, RealtimeLane.x + RealtimeLane.width);
            if (right > left)
                canvas.Box({left, 449, right - left, 32}, Gold, 16);
            headX.reset();
        }
        if (x < 200 || x > 1810)
            continue;
        Circle(canvas, x, 465, n.note.keyType, (n.note.keyType >= 3 && n.note.keyType <= 5) ? 52.0F : normalHeadRadius);
        noteHits_.push_back({{x, 465}, n.note.sourceOrder});
    }
    for (const auto &t : state.Document().Timing())
    {
        if (t.type != chart::TimingDirectiveType::Bpm)
            continue;
        const float x = xAtTime(timeline.Compile(t.position), 1.0);
        if (x >= 164 && x <= 1710)
            canvas.Text({x, 320, 140, 28}, L"BPM " + std::to_wstring(static_cast<int>(t.value)), 18, Blue);
    }
}

void TaikoEditorMode::EditScore(IEditorContext &state, v::Point point, bool erase)
{
    if (state.Score().realtime && !RealtimeLane.Contains(point))
        return;
    if (erase)
    {
        const auto nearest = std::ranges::min_element(
            noteHits_, {}, [point](const NoteHit &n) { return std::hypot(n.point.x - point.x, n.point.y - point.y); });
        if (nearest != noteHits_.end() &&
            std::hypot(nearest->point.x - point.x, nearest->point.y - point.y) < (state.Score().realtime ? 55 : 24))
            state.DeleteNote(nearest->order);
        state.RequestRebuild();
        return;
    }
    chart::MusicalPosition p;
    if (state.Score().realtime)
    {
        if (realtimeGrid_.empty())
            return;
        p = std::ranges::min_element(realtimeGrid_, {}, [point](const auto &g) {
                return std::abs(g.first - point.x);
            })->second;
    }
    else
    {
        const int row = static_cast<int>((point.y - RowTop) / RowHeight), col = static_cast<int>((point.x - ScoreLeft) / MeasureWidth);
        if (point.x < ScoreLeft || point.x >= ScoreRight || point.y < RowTop || row >= VisibleRows || point.y - (RowTop + row * RowHeight) > StaffHeight)
            return;
        p.measure = state.Score().firstMeasure + row * MeasuresPerRow + col;
        if (p.measure >= chart::MaximumMeasureCount)
            return;
        const auto length = state.Document().Timeline().MeasureLength(p.measure);
        const int subdivisionsPerWholeNote = state.Score().division * 4;
        const auto tick = static_cast<std::int64_t>(
            std::llround((point.x - ScoreLeft - col * MeasureWidth) / static_cast<double>(MeasureWidth) * length.Value() * subdivisionsPerWholeNote));
        p.fraction = chart::Rational{tick, subdivisionsPerWholeNote};
        if (p.fraction >= length)
            p.fraction = chart::Rational{std::max<std::int64_t>(0, tick - 1), subdivisionsPerWholeNote};
    }
    PlaceNote(state, p);
}

void TaikoEditorMode::Circle(IEditorModeCanvas &canvas, float x, float y, int type, float radius)
{
    const auto color = type == 2 || type == 4 ? Kat : type == 5 ? v::Color{.65F, .30F, .85F, 1} : type > 5 ? Gold : Don;
    canvas.Box({x - radius - 2, y - radius - 2, radius * 2 + 4, radius * 2 + 4}, White, radius + 2);
    canvas.Box({x - radius, y - radius, radius * 2, radius * 2}, color, radius);
}
