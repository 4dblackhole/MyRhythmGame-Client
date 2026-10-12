#include "../../EditorSupport.h"
#include "../../EditorTime.h"
#include "App/AssetPaths.h"
#include "Taiko/Submodules/TaikoNoteOptions.h"
#include "TaikoEditorMode.h"
#include "TaikoEditorTool.h"
#include "Texts/EditorScene/Taiko/TaikoEditorTexts.h"
#include <algorithm>
#include <limits>
using namespace editor_ui;

namespace
{
    constexpr v::Rect RealtimeLane{164, 351, 1324, 228};
    constexpr float ScoreLeft = 205, MeasureWidth = 315, RowTop = 150, RowHeight = 100, StaffHeight = 48;
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
                const auto nearestIndex =
                    static_cast<int>(std::clamp(std::floor(directive.position.fraction.Value() * subdivisions), 0.0L,
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
    state.SetFirstMeasure(std::clamp<std::int64_t>(state.Score().firstMeasure + direction * MeasuresPerRow, 0,
                                                   chart::MaximumMeasureCount - 1));
}

void TaikoEditorMode::DrawChart(IEditorModeCanvas &canvas, IEditorContext &state, finger_drum::texts::Language language)
{
    canvas.Box({112, 116, 1784, 678}, Paper);
    noteHits_.clear();
    bodyHits_.clear();
    realtimeGrid_.clear();
    if (state.Score().realtime)
        DrawRealtime(canvas, state, language);
    else
        DrawOverview(canvas, state, language);
    inspector_.Draw(canvas, state, language);
}

void TaikoEditorMode::DrawOverview(IEditorModeCanvas &canvas, IEditorContext &state,
                                   finger_drum::texts::Language language)
{
    const auto &timeline = state.Document().Timeline();
    const int subdivisionsPerWholeNote = state.Score().division * 4;
    const auto xAt = [&](chart::MusicalPosition p)
    {
        return ScoreLeft +
               static_cast<float>((p.measure - state.Score().firstMeasure) % MeasuresPerRow) * MeasureWidth +
               static_cast<float>(p.fraction.Value() / timeline.MeasureLength(p.measure).Value()) * MeasureWidth;
    };
    // Rational score grid, followed by note intervals and BPM labels.
    for (int row = 0; row < VisibleRows; ++row)
    {
        const float y = RowTop + row * RowHeight;
        canvas.Box({164, y, 1324, StaffHeight}, {.957F, .984F, 1, 1}, 3);
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
    const auto visibleNotes = state.Document().NotesInMeasures(
        state.Score().firstMeasure, state.Score().firstMeasure + MeasuresPerRow * VisibleRows);
    // Interval bodies precede skin heads so the body cannot cover their artwork.
    for (const auto &note : visibleNotes)
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
                const v::Rect body{left, 166.0F + row * RowHeight, std::max(0.0F, right - left), 16};
                canvas.Box(body, Gold, 8);
                bodyHits_.push_back({body, head->sourceOrder});
            }
            if (head->position.measure >= state.Score().firstMeasure &&
                head->position.measure < state.Score().firstMeasure + MeasuresPerRow * VisibleRows)
                canvas.Text(
                    {xAt(head->position) + 16,
                     150 + static_cast<float>((head->position.measure - state.Score().firstMeasure) / MeasuresPerRow) *
                               RowHeight,
                     std::max(0.0F, ScoreRight - xAt(head->position) - 18), 22},
                    LongNoteCaption(state.Document(), *head, n, language), 13);
            head.reset();
        }
    }
    for (const auto &note : visibleNotes)
    {
        const auto &n = note.note;
        if (n.position.measure < state.Score().firstMeasure ||
            n.position.measure >= state.Score().firstMeasure + MeasuresPerRow * VisibleRows)
            continue;
        const float x = xAt(n.position),
                    y = 174 + static_cast<float>((n.position.measure - state.Score().firstMeasure) / MeasuresPerRow) *
                                  RowHeight;
        if (inspector_.IsSelected(n.sourceOrder))
            canvas.Box({x - 19, y - 19, 38, 38}, Blue, 19);
        Circle(canvas, x, y, VisualTool(n),
               (n.keyType >= 3 && n.keyType <= 5) || n.keyType == 13 || n.keyType == 14 ? 15.0F : 10.0F);
        noteHits_.push_back({{x, y}, n.sourceOrder});
    }
    for (const auto &t : state.Document().Timing())
        if (t.type == chart::TimingDirectiveType::Bpm && t.position.measure >= state.Score().firstMeasure &&
            t.position.measure < state.Score().firstMeasure + MeasuresPerRow * VisibleRows)
            canvas.Text({xAt(t.position),
                         200 + static_cast<float>((t.position.measure - state.Score().firstMeasure) / MeasuresPerRow) *
                                   RowHeight,
                         140, 24},
                        L"BPM " + std::to_wstring(static_cast<int>(t.value)), 15, Blue);
}

void TaikoEditorMode::DrawRealtime(IEditorModeCanvas &canvas, IEditorContext &state,
                                   finger_drum::texts::Language language)
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
    const auto xAtTime = [currentTime, pixelsPerWholeNote, &document = state.Document(),
                          baseBpm = timeline.BaseBpm()](const auto timing, const double speed)
    {
        return 230.0F + static_cast<float>(document.ScrollDistance(timing, currentTime)) *
                            static_cast<float>(baseBpm / 240'000'000.0) * pixelsPerWholeNote *
                            static_cast<float>(speed);
    };
    const auto currentMeasure = editor_time::MeasureNearTime(timeline, state.TimeMilliseconds());
    const auto gridBegin = std::max<std::int64_t>(0, currentMeasure - 1);
    const long double pixelsPerMicrosecond = timeline.BaseBpm() / 240'000'000.0L * pixelsPerWholeNote;
    const auto minimumSpeed = state.Document().MinimumScrollMultiplier();
    const bool customGridSpeed = std::ranges::any_of(state.Document().Effects().commands,
                                                     [](const auto &command)
                                                     {
                                                         return command.type == chart::EffectCommandType::NoteSpeed &&
                                                                command.curve == chart::AutomationCurve::Expression;
                                                     });
    const auto visibleBegin =
        BoundedTime(currentTime.count() + (RealtimeLane.x - 230.0L - 110) / (pixelsPerMicrosecond * minimumSpeed));
    const auto visibleEnd = BoundedTime(currentTime.count() + (RealtimeLane.x + RealtimeLane.width - 230.0L + 110) /
                                                                  (pixelsPerMicrosecond * minimumSpeed));
    // Drawing and input share the same visible snap positions.
    for (std::int64_t m = gridBegin; m < std::min(currentMeasure + 32, chart::MaximumMeasureCount); ++m)
    {
        const int count = static_cast<int>(
            std::min(4096.0L, std::ceil(timeline.MeasureLength(m).Value() * subdivisionsPerWholeNote)));
        // Arbitrary expressions can dip below both endpoints between authored
        // notes. Keep the bounded grid traversal and clip by its actual X.
        if (!customGridSpeed)
        {
            const auto [firstTime, lastTime] = GridTimeRange(state.Document(), m, subdivisionsPerWholeNote, count);
            if (lastTime < visibleBegin || firstTime > visibleEnd)
                continue;
        }
        float lastDrawnX = -std::numeric_limits<float>::infinity();
        for (int i = 0; i < count; ++i)
        {
            chart::MusicalPosition p{m, {i, subdivisionsPerWholeNote}};
            const auto speed = state.Document().EffectValueAt(chart::EffectCommandType::NoteSpeed, p);
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
    std::optional<chart::PatternNote> realtimeHead;
    const auto visibleNotes = state.Document().NotesInTimeRange(visibleBegin, visibleEnd);
    // Borrow from this frame's snapshot only until the head pass below.
    std::vector<std::pair<const chart::CompiledPatternNote *, float>> heads;
    for (const auto &n : visibleNotes)
    {
        const auto speed = n.note.actionType == 2 && headX ? headSpeed : n.scrollMultiplier;
        const float x = xAtTime(n.timing, speed);
        if (n.note.actionType == 1)
        {
            headX = x;
            realtimeHead = n.note;
            headSpeed = n.scrollMultiplier;
        }
        if (n.note.actionType == 2 && headX)
        {
            const float left = std::max(*headX, RealtimeLane.x),
                        right = std::min(x, RealtimeLane.x + RealtimeLane.width);
            if (right > left)
            {
                const v::Rect body{left, 449, right - left, 32};
                canvas.Box(body, Gold, 16);
                if (realtimeHead)
                    bodyHits_.push_back({body, realtimeHead->sourceOrder});
            }
            if (realtimeHead && *headX >= RealtimeLane.x && *headX <= RealtimeLane.x + RealtimeLane.width - 180)
                canvas.Text({*headX, 522, 300, 38}, LongNoteCaption(state.Document(), *realtimeHead, n.note, language),
                            17, White);
            headX.reset();
        }
        heads.emplace_back(&n, x);
    }
    for (const auto &[source, x] : heads)
    {
        const auto &n = *source;
        if (x < RealtimeLane.x + 55 || x > RealtimeLane.x + RealtimeLane.width - 55)
            continue;
        if (inspector_.IsSelected(n.note.sourceOrder))
            canvas.Box({x - 58, 407, 116, 116}, Blue, 58);
        Circle(canvas, x, 465, VisualTool(n.note),
               (n.note.keyType >= 3 && n.note.keyType <= 5) || n.note.keyType == 13 || n.note.keyType == 14
                   ? 54.0F
                   : normalHeadRadius);
        noteHits_.push_back({{x, 465}, n.note.sourceOrder});
    }
    for (const auto &t : state.Document().Timing())
    {
        if (t.type != chart::TimingDirectiveType::Bpm)
            continue;
        const float x = xAtTime(timeline.Compile(t.position), 1.0);
        if (x >= 164 && x <= 1360)
            canvas.Text({x, 320, 140, 28}, L"BPM " + std::to_wstring(static_cast<int>(t.value)), 18, Blue);
    }
}

void TaikoEditorMode::EditScore(IEditorContext &state, v::Point point, bool erase)
{
    if (state.Score().realtime
            ? !RealtimeLane.Contains(point)
            : !v::Rect{ScoreLeft, RowTop, MeasureWidth * MeasuresPerRow, RowHeight * VisibleRows}.Contains(point))
        return;
    const auto nearest = std::ranges::min_element(noteHits_, {}, [point](const NoteHit &n)
                                                  { return std::hypot(n.point.x - point.x, n.point.y - point.y); });
    const bool hitsNote =
        nearest != noteHits_.end() &&
        std::hypot(nearest->point.x - point.x, nearest->point.y - point.y) < (state.Score().realtime ? 55 : 24);
    if (erase || tool_ == 0)
    {
        if (hitsNote)
        {
            if (erase)
                state.DeleteNote(nearest->order);
            else
                inspector_.Select(state.Document(), nearest->order);
            state.RequestRebuild();
            return;
        }
        if (erase)
            return;
        const auto body =
            std::ranges::find_if(bodyHits_, [point](const auto &hit) { return hit.bounds.Contains(point); });
        if (body != bodyHits_.end())
        {
            inspector_.Select(state.Document(), body->order);
            state.RequestRebuild();
            return;
        }
        if (inspector_.Clear())
            state.RequestRebuild();
    }
    chart::MusicalPosition p;
    if (state.Score().realtime)
    {
        if (realtimeGrid_.empty())
            return;
        p = std::ranges::min_element(realtimeGrid_, {}, [point](const auto &g) { return std::abs(g.first - point.x); })
                ->second;
    }
    else
    {
        const int row = static_cast<int>((point.y - RowTop) / RowHeight),
                  col = static_cast<int>((point.x - ScoreLeft) / MeasureWidth);
        if (point.x < ScoreLeft || point.x >= ScoreRight || point.y < RowTop || row >= VisibleRows ||
            point.y - (RowTop + row * RowHeight) > StaffHeight)
            return;
        p.measure = state.Score().firstMeasure + row * MeasuresPerRow + col;
        if (p.measure >= chart::MaximumMeasureCount)
            return;
        const auto length = state.Document().Timeline().MeasureLength(p.measure);
        const int subdivisionsPerWholeNote = state.Score().division * 4;
        const auto tick = static_cast<std::int64_t>(std::llround((point.x - ScoreLeft - col * MeasureWidth) /
                                                                 static_cast<double>(MeasureWidth) * length.Value() *
                                                                 subdivisionsPerWholeNote));
        p.fraction = chart::Rational{tick, subdivisionsPerWholeNote};
        if (p.fraction >= length)
            p.fraction = chart::Rational{std::max<std::int64_t>(0, tick - 1), subdivisionsPerWholeNote};
    }
    PlaceNote(state, p);
}

int TaikoEditorMode::VisualTool(const chart::PatternNote &note)
{
    using namespace finger_drum::mode;
    if (note.keyType == static_cast<int>(TaikoNoteType::Buzz))
    {
        std::vector<chart::Diagnostic> diagnostics;
        if (taiko_options::ReadBuzzAction(note, diagnostics) == TaikoAction::Kat)
            return 18;
    }
    return note.keyType;
}

std::wstring TaikoEditorMode::LongNoteCaption(const chart::IEditorDocument &document, const chart::PatternNote &head,
                                              const chart::PatternNote &tail,
                                              finger_drum::texts::Language language) const
{
    using namespace finger_drum::mode;
    const auto &text = finger_drum::texts::TaikoEditor(language);
    const int id = VisualTool(head);
    const std::wstring name(editor_tools::Name(id, text.toolVariants));
    std::vector<chart::Diagnostic> diagnostics;
    if (editor_tools::UsesTickDivision(id))
    {
        const auto division = taiko_options::ReadPositiveOption(head, taiko_option::TickDivision, 16, diagnostics);
        // Persisted TickDivision divides a whole note; a quarter-note beat is 1/4.
        return name + L" | " + std::wstring(text.tickInterval) + L" " +
               (division ? Wide(Fraction(chart::Rational{4, static_cast<std::int64_t>(*division)})) + L" " +
                               std::wstring(text.beats)
                         : L"?");
    }
    const auto count = taiko_options::ReadHitCount(
        head, document.Timeline().CountSubdivisions(head.position, tail.position, 12), diagnostics);
    return name + L" | " + (count ? std::to_wstring(*count) : L"?") + L" " + std::wstring(text.hits);
}

const std::filesystem::path &TaikoEditorMode::SkinImage(std::wstring_view file)
{
    // Path resolution can probe external skin files. Do it once per selected
    // skin revision; the common renderer continues to own/caches image handles.
    const auto revision = mrg_client::SkinSetSelection::Instance().Revision();
    if (skinRevision_ != revision)
    {
        skinImagePaths_.clear();
        skinRevision_ = revision;
    }
    const auto cached = skinImagePaths_.find(file);
    if (cached != skinImagePaths_.end())
        return cached->second;
    return skinImagePaths_.emplace(std::wstring(file), mrg_client::asset_paths::skin::InGame(file)).first->second;
}

void TaikoEditorMode::Circle(IEditorModeCanvas &canvas, float x, float y, int type, float radius)
{
    const bool custom = type == 15 || type == 16;
    const bool big = (type >= 3 && type <= 5) || type == 13 || type == 14;
    const auto color = type == 2 || type == 4 || type == 18 ? Kat
                       : type == 5                          ? v::Color{.65F, .30F, .85F, 1}
                       : type > 5 && type < 17              ? Gold
                                                            : Don;
    const auto file = type == 15 ? L"Balloon.png" : type == 16 ? L"DengDeng.png" : big ? L"bignote.png" : L"note.png";
    const v::Rect bounds{x - radius, y - radius, radius * 2, radius * 2};
    canvas.Image(bounds, SkinImage(file), custom ? White : color);
    if (!custom)
        canvas.Image(bounds, SkinImage(big ? L"bigcircleoverlay.png" : L"noteoverlay.png"), White);
}
