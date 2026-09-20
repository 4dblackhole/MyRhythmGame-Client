#include "EditorScene.h"
#include "App/AssetPaths.h"
#include "Editing/ChartEditor.h"
#include "EditorAudioAnalysis.h"
#include "GameFlow/FingerDrumSceneIds.h"
#include "Parsing/ChartParser.h"
#include <Windows.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <functional>
#include <future>
#include <sstream>
#include <stdexcept>

namespace
{
namespace chart = finger_drum::chart;
namespace v = mrg::visual2d;
constexpr v::Color Background{.914F, .961F, 1, 1}, Paper{.973F, .988F, 1, 1};
constexpr v::Color Ink{.17F, .34F, .47F, 1}, Blue{.31F, .62F, .88F, 1}, Pale{.82F, .91F, .96F, 1}, White{1, 1, 1, 1};
constexpr v::Color Don{1, .33F, .43F, 1}, Kat{.10F, .76F, .82F, 1}, Gold{.96F, .80F, .29F, 1};
std::wstring Wide(const std::string &s)
{
    const int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<std::size_t>(n), L'\0');
    if (n)
        MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}
std::string Utf8(const std::wstring &w)
{
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    std::string s(static_cast<std::size_t>(n), '\0');
    if (n)
        WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), s.data(), n, nullptr, nullptr);
    return s;
}
std::string Fraction(chart::Rational beat)
{
    return std::to_string(beat.Numerator()) + "/" + std::to_string(beat.Denominator());
}
double Number(const std::string &value)
{
    std::size_t used{};
    const double result = std::stod(value, &used);
    if (used != value.size() || !std::isfinite(result))
        throw std::invalid_argument("Invalid number.");
    return result;
}
std::int64_t Integer(const std::string &value)
{
    std::size_t used{};
    const auto result = std::stoll(value, &used);
    if (used != value.size())
        throw std::invalid_argument("Invalid integer.");
    return result;
}
chart::MusicalPosition Position(const std::string &measure, const std::string &fraction)
{
    chart::MusicalPosition result;
    if (!chart::TryParseMusicalPosition(fraction, Integer(measure) - 1, result))
        throw std::invalid_argument("Use a one-based measure and N/D without internal spaces.");
    return result;
}
std::int64_t MeasureNearTime(const chart::MusicalTimeline &timeline, double milliseconds)
{
    // Seek through cached prefix sums/tempo anchors, not from measure zero.
    std::int64_t low = 0, high = 1;
    const auto at = [&](std::int64_t measure) { return timeline.Compile({measure, {}}).count() / 1000.0; };
    while (at(high) <= milliseconds)
    {
        if (high > 1'000'000)
            throw std::invalid_argument("Editor time is beyond the supported measure range.");
        high *= 2;
    }
    while (low + 1 < high)
    {
        const auto middle = low + (high - low) / 2;
        if (at(middle) <= milliseconds)
            low = middle;
        else
            high = middle;
    }
    return low;
}
struct EditDialog
{
    std::wstring title, value;
    HWND edit{};
    bool accepted{};
};
INT_PTR CALLBACK EditProcedure(HWND dialog, UINT message, WPARAM w, LPARAM l)
{
    auto *data = reinterpret_cast<EditDialog *>(GetWindowLongPtrW(dialog, DWLP_USER));
    if (message == WM_INITDIALOG)
    {
        data = reinterpret_cast<EditDialog *>(l);
        SetWindowLongPtrW(dialog, DWLP_USER, l);
        SetWindowTextW(dialog, data->title.c_str());
        RECT r{};
        GetClientRect(dialog, &r);
        data->edit =
            CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", data->value.c_str(),
                            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL, 12, 12,
                            r.right - 24, r.bottom - 65, dialog, reinterpret_cast<HMENU>(100), nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"OK", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, r.right - 195,
                      r.bottom - 42, 85, 30, dialog, reinterpret_cast<HMENU>(IDOK), nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | WS_TABSTOP, r.right - 100, r.bottom - 42, 85, 30,
                      dialog, reinterpret_cast<HMENU>(IDCANCEL), nullptr, nullptr);
        SendMessageW(data->edit, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        SetFocus(data->edit);
        return FALSE;
    }
    if (message == WM_COMMAND && (LOWORD(w) == IDOK || LOWORD(w) == IDCANCEL))
    {
        if (LOWORD(w) == IDOK)
        {
            const int n = GetWindowTextLengthW(data->edit);
            data->value.resize(static_cast<std::size_t>(n) + 1);
            GetWindowTextW(data->edit, data->value.data(), n + 1);
            data->value.resize(static_cast<std::size_t>(n));
            data->accepted = true;
        }
        EndDialog(dialog, LOWORD(w));
        return TRUE;
    }
    return FALSE;
}
std::optional<std::string> EditText(const std::wstring &title, const std::string &value)
{
    struct Template
    {
        DLGTEMPLATE dialog;
        WORD menu{}, windowClass{}, title{};
    } layout{};
    layout.dialog.style = WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME | DS_CENTER;
    layout.dialog.cx = 360;
    layout.dialog.cy = 190;
    EditDialog data{title, Wide(value)};
    if (DialogBoxIndirectParamW(GetModuleHandleW(nullptr), &layout.dialog, GetActiveWindow(), EditProcedure,
                                reinterpret_cast<LPARAM>(&data)) == -1)
        throw std::runtime_error("Could not open the text input dialog.");
    return data.accepted ? std::optional{Utf8(data.value)} : std::nullopt;
}
class EditorVisual final : public v::Visual2DComponent
{
  public:
    std::vector<v::DrawPacket> packets;
    void AppendDrawPackets(std::vector<v::DrawPacket> &output) const override
    {
        output.insert(output.end(), packets.begin(), packets.end());
    }
};
struct Control
{
    v::Rect rect;
    std::function<void()> click;
};
struct NoteHit
{
    v::Point point;
    std::size_t order;
};
struct AnalysisBatch
{
    std::map<std::string, finger_drum::editor::AudioAnalysis> sounds;
    std::string errors;
};
struct AudioMarker
{
    double seconds{};
    std::string sound;
};
} // namespace

struct EditorScene::State
{
    v::ScreenVisual2DManager &visuals;
    std::shared_ptr<finger_drum::GameplayLaunchRequest> request;
    v::ScreenCanvasHandle canvas;
    v::Visual2DNode *node{};
    EditorVisual *visual{};
    std::unique_ptr<chart::ChartEditor> editor;
    std::vector<Control> controls;
    std::vector<NoteHit> noteHits;
    std::vector<std::pair<float, chart::MusicalPosition>> realtimeGrid;
    std::optional<chart::MusicalPosition> pending;
    std::uint32_t width{1280}, height{720};
    int tab{}, tool{1}, popup{-1}, division{16};
    int smallTool{1}, bigTool{3}, rollTool{11}, focusTool{15};
    std::int64_t firstMeasure{};
    bool realtime{}, rebuild{true}, analysisDirty{true};
    double timeMs{}, audioWindow{8.0};
    std::string status;
    std::array<std::string, 4> timingFields{"1", "0/4", "120", "4/4"};
    std::array<std::string, 7> effectFields{"1", "0/4", "", "", "1", "1", "HitSound"};
    int effectType{}, curve{};
    std::size_t listOffset{};
    std::stop_source analysisStop;
    std::future<AnalysisBatch> analysisJob;
    AnalysisBatch analysis;
    std::map<std::string, std::filesystem::path> analysisFiles;
    std::vector<AudioMarker> audioMarkers;
    std::uint64_t audioMarkerRevision{};
    State(v::ScreenVisual2DManager &v, std::shared_ptr<finger_drum::GameplayLaunchRequest> r)
        : visuals(v), request(std::move(r))
    {
    }
    void Box(v::Rect r, v::Color color, float radius = 0)
    {
        v::DrawPacket p{};
        p.bounds = {r.x - 960, 540 - r.y - r.height, r.width, r.height};
        p.color = color;
        p.cornerRadius = radius;
        visual->packets.push_back(std::move(p));
    }
    void Text(v::Rect r, std::wstring text, float size = 20, v::Color color = Ink)
    {
        v::DrawPacket p{};
        p.type = v::DrawPacketType::Text;
        p.bounds = {r.x - 960, 540 - r.y - r.height, r.width, r.height};
        p.color = color;
        p.text = std::move(text);
        p.fontSize = size;
        visual->packets.push_back(std::move(p));
    }
    void Button(v::Rect r, std::wstring text, std::function<void()> action, bool selected = false)
    {
        Box(r, selected ? Blue : Pale, 5);
        Text({r.x + 10, r.y + 3, r.width - 15, r.height - 4}, std::move(text), 20, selected ? White : Ink);
        controls.push_back({r, std::move(action)});
    }
    void Field(v::Rect r, const wchar_t *label, std::string &value)
    {
        Text({r.x, r.y - 28, r.width, 24}, label, 17);
        Button(r, Wide(value), [this, label, &value] {
            if (auto edited = EditText(label, value))
            {
                value = *edited;
                rebuild = true;
            }
        });
    }
    void SelectTool(int value)
    {
        tool = value;
        if (value == 1 || value == 2)
            smallTool = value;
        else if (value >= 3 && value <= 5)
            bigTool = value;
        else if (value == 15 || value == 16)
            focusTool = value;
        else if (value >= 11)
            rollTool = value;
        pending.reset();
        popup = -1;
        rebuild = true;
    }
    void Circle(float x, float y, int type, float radius)
    {
        const auto color = type == 2 || type == 4 ? Kat
                           : type == 5            ? v::Color{.65F, .30F, .85F, 1}
                           : type > 5             ? Gold
                                                  : Don;
        Box({x - radius - 2, y - radius - 2, radius * 2 + 4, radius * 2 + 4}, White, radius + 2);
        Box({x - radius, y - radius, radius * 2, radius * 2}, color, radius);
    }
    void ReloadSize()
    {
        const float scale = std::min(1.0F, canvas.Get()->LogicalSize().width / 1920.0F);
        node->Transform().SetScale(scale, scale, 1);
        rebuild = true;
    }
    void Save()
    {
        editor->Save();
        request->effectPath = editor->Effects().sourcePath;
        status = "Saved: " + Utf8(editor->Pattern().sourcePath.wstring());
        rebuild = true;
    }
    void DrawScore();
    void DrawTools();
    void DrawChartContent();
    void DrawVariantMenu();
    void DrawTiming();
    void DrawMetadata();
    void DrawEffects();
    void DrawAudio();
    void CacheAudioMarkers();
    void Build();
    void UpdateAnalysis();
    void EditScore(v::Point point, bool erase);
};

void EditorScene::State::DrawScore()
{
    DrawTools();
    DrawChartContent();
    if (pending)
        Text({180, 740, 1550, 32}, L"롱노트 끝 위치를 클릭하세요 · 도구 변경/Escape: 취소", 22, Blue);
    DrawAudio();
    DrawVariantMenu();
}

void EditorScene::State::DrawTools()
{
    Button(
        {112, 76, 180, 32}, L"전체 악보 뷰",
        [this] {
            realtime = false;
            rebuild = true;
        },
        !realtime);
    Button(
        {292, 76, 180, 32}, L"실시간 뷰",
        [this] {
            realtime = true;
            rebuild = true;
        },
        realtime);
    Text({1080, 12, 160, 32}, L"박자 디바이더", 18);
    for (int i = 0; i < 6; ++i)
    {
        const int d = 4 << i;
        Button(
            {1240.0F + i * 80, 8, 72, 40}, L"1/" + std::to_wstring(d),
            [this, d] {
                division = d;
                rebuild = true;
            },
            division == d);
    }
    Button({1740, 8, 150, 40}, L"직접 입력", [this] {
        if (auto s = EditText(L"박자 분할 (1/N)", std::to_string(division)))
        {
            const auto d = Integer(*s);
            if (d < 1 || d > 1024)
                throw std::invalid_argument("Division must be 1..1024.");
            division = static_cast<int>(d);
            rebuild = true;
        }
    });
    Box({24, 76, 70, 918}, Paper, 18);
    Text({38, 86, 55, 25}, L"도구", 16);
    const std::array<std::wstring, 7> names{L"선택",
                                            smallTool == 2 ? L"캇" : L"동",
                                            bigTool == 5   ? L"보라노트"
                                            : bigTool == 4 ? L"큰 캇"
                                                           : L"큰 동",
                                            rollTool >= 17                     ? L"버즈"
                                            : rollTool == 12 || rollTool == 14 ? L"틱롤"
                                                                               : L"롤노트",
                                            focusTool == 16 ? L"뎅뎅" : L"풍선",
                                            L"BPM",
                                            L"마디"};
    const std::array<int, 7> ids{0, smallTool, bigTool, rollTool, focusTool, -2, -3};
    for (std::size_t i = 0; i < ids.size(); ++i)
    {
        const float y = 122 + static_cast<float>(i) * 80;
        Button({35, y, 48, 48}, i < 5 ? L"" : names[i], [this, id = ids[i]] { SelectTool(id); }, tool == ids[i]);
        if (i > 0 && i < 5)
            Circle(59, y + 24, ids[i], i == 2 ? 15.0F : 10.0F);
        Text({27, y + 49, 67, 25}, names[i], 13);
    }
}

void EditorScene::State::DrawChartContent()
{
    Box({112, 116, 1784, 678}, Paper);
    noteHits.clear();
    realtimeGrid.clear();
    const auto &timeline = editor->Timeline();
    const auto xAt = [&](chart::MusicalPosition p) {
        return 205.0F + static_cast<float>((p.measure - firstMeasure) % 4) * 401 +
               static_cast<float>(p.fraction.Value() / timeline.MeasureLength(p.measure).Value()) * 401;
    };
    if (!realtime)
    {
        // Rational score grid, followed by note intervals and BPM labels.
        for (int row = 0; row < 6; ++row)
        {
            const float y = 150.0F + row * 100;
            Box({164, y, 1684, 48}, {.957F, .984F, 1, 1}, 3);
            for (int col = 0; col < 4; ++col)
            {
                const auto m = firstMeasure + row * 4 + col;
                const float x = 205.0F + col * 401;
                Text({x, y - 25, 140, 22}, std::to_wstring(m + 1) + L"  " + Wide(Fraction(timeline.MeasureLength(m))),
                     13);
                const auto count =
                    static_cast<int>(std::min(4096.0L, std::ceil(timeline.MeasureLength(m).Value() * division)));
                for (int i = 0; i < count; ++i)
                {
                    const float gx = x + static_cast<float>(chart::Rational{i, division}.Value() /
                                                            timeline.MeasureLength(m).Value()) *
                                             401;
                    Box({gx, y, i == 0 ? 2.0F : 1.0F, 48}, i == 0 ? Blue : Pale);
                }
            }
        }
        std::optional<chart::PatternNote> head;
        for (const auto &note : editor->Notes())
        {
            const auto &n = note.note;
            if (n.actionType == 1 && !head)
                head = n;
            if (n.actionType == 2 && head && head->keyType == n.keyType)
            {
                for (int row = 0; row < 6; ++row)
                {
                    const auto begin = firstMeasure + row * 4, end = begin + 4;
                    if (n.position.measure < begin || head->position.measure >= end)
                        continue;
                    const float left = head->position.measure < begin ? 205 : xAt(head->position),
                                right = n.position.measure >= end ? 1809 : xAt(n.position);
                    Box({left, 166.0F + row * 100, std::max(0.0F, right - left), 16}, Gold, 8);
                }
                head.reset();
            }
            if (n.position.measure < firstMeasure || n.position.measure >= firstMeasure + 24)
                continue;
            const float x = xAt(n.position),
                        y = 174 + static_cast<float>((n.position.measure - firstMeasure) / 4) * 100;
            Circle(x, y, n.keyType, (n.keyType >= 3 && n.keyType <= 5) ? 15.0F : 10.0F);
            noteHits.push_back({{x, y}, n.sourceOrder});
        }
        for (const auto &t : editor->Pattern().timing)
            if (t.type == chart::TimingDirectiveType::Bpm && t.position.measure >= firstMeasure &&
                t.position.measure < firstMeasure + 24)
                Text(
                    {xAt(t.position), 200 + static_cast<float>((t.position.measure - firstMeasure) / 4) * 100, 140, 24},
                    L"BPM " + std::to_wstring(static_cast<int>(t.value)), 15, Blue);
    }
    else
    {
        // The realtime view projects the same cached note times around the
        // selected time. A long note uses its head's speed for both endpoints.
        Box({164, 351, 1684, 228}, {.055F, .060F, .12F, 1});
        Box({230, 351, 3, 228}, White);
        constexpr double pixelsPerMs = .30;
        const auto currentMeasure = MeasureNearTime(timeline, timeMs);
        const auto gridBegin = std::max<std::int64_t>(0, currentMeasure - 1);
        for (std::int64_t m = gridBegin; m < currentMeasure + 32; ++m)
        {
            const int count =
                static_cast<int>(std::min(4096.0L, std::ceil(timeline.MeasureLength(m).Value() * division)));
            for (int i = 0; i < count; ++i)
            {
                chart::MusicalPosition p{m, {i, division}};
                const auto speed = timeline.EffectValueAt(editor->Effects(), chart::EffectCommandType::NoteSpeed, p) *
                                   timeline.EffectValueAt(editor->Effects(), chart::EffectCommandType::ScrollSpeed, p);
                const float x =
                    230 + static_cast<float>((timeline.Compile(p).count() / 1000.0 - timeMs) * pixelsPerMs * speed);
                if (x < 164 || x > 1848)
                    continue;
                if (i != 0 ||
                    timeline.EffectValueAt(editor->Effects(), chart::EffectCommandType::MeasureLineVisible, p) >= .5)
                    Box({x, 361, i == 0 ? 2.0F : 1.0F, 202}, {.3F, .34F, .42F, i == 0 ? 1.0F : .45F});
                realtimeGrid.push_back({x, p});
            }
        }
        std::optional<float> headX;
        double headSpeed = 1;
        for (const auto &n : editor->Notes())
        {
            const auto speed = n.note.actionType == 2 && headX ? headSpeed : n.scrollMultiplier;
            const float x = 230 + static_cast<float>((n.timing.count() / 1000.0 - timeMs) * pixelsPerMs * speed);
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
            Circle(x, 465, n.note.keyType, (n.note.keyType >= 3 && n.note.keyType <= 5) ? 52.0F : 36.0F);
            noteHits.push_back({{x, 465}, n.note.sourceOrder});
        }
        for (const auto &t : editor->Pattern().timing)
        {
            if (t.type != chart::TimingDirectiveType::Bpm)
                continue;
            const float x =
                230 + static_cast<float>((timeline.Compile(t.position).count() / 1000.0 - timeMs) * pixelsPerMs);
            if (x >= 164 && x <= 1710)
                Text({x, 320, 140, 28}, L"BPM " + std::to_wstring(static_cast<int>(t.value)), 18, Blue);
        }
    }
}

void EditorScene::State::DrawVariantMenu()
{
    if (popup >= 0)
    {
        const std::vector<std::pair<std::wstring, int>> choices =
            popup == 1 ? std::vector<std::pair<std::wstring, int>>{{L"동", 1}, {L"캇", 2}}
            : popup == 2
                ? std::vector<std::pair<std::wstring, int>>{{L"큰 동", 3}, {L"큰 캇", 4}, {L"보라노트", 5}}
                : std::vector<std::pair<std::wstring, int>>{{L"Roll", 11},        {L"TickRoll", 12}, {L"BigRoll", 13},
                                                            {L"BigTickRoll", 14}, {L"Balloon", 15},  {L"DengDeng", 16},
                                                            {L"Don Buzz", 17},    {L"Kat Buzz", 18}};
        for (std::size_t i = 0; i < choices.size(); ++i)
            Button({105, 200 + static_cast<float>(i) * 40, 220, 38}, choices[i].first,
                   [this, id = choices[i].second] { SelectTool(id); });
    }
}

void EditorScene::State::EditScore(v::Point point, bool erase)
{
    if (erase)
    {
        const auto nearest = std::ranges::min_element(
            noteHits, {}, [point](const NoteHit &n) { return std::hypot(n.point.x - point.x, n.point.y - point.y); });
        if (nearest != noteHits.end() &&
            std::hypot(nearest->point.x - point.x, nearest->point.y - point.y) < (realtime ? 55 : 24))
            editor->DeleteNote(nearest->order);
        rebuild = true;
        return;
    }
    chart::MusicalPosition p;
    if (realtime)
    {
        if (point.y < 351 || point.y > 579 || realtimeGrid.empty())
            return;
        p = std::ranges::min_element(realtimeGrid, {}, [point](const auto &g) {
                return std::abs(g.first - point.x);
            })->second;
    }
    else
    {
        const int row = static_cast<int>((point.y - 150) / 100), col = static_cast<int>((point.x - 205) / 401);
        if (point.x < 205 || point.x >= 1809 || point.y < 150 || row >= 6 || point.y - (150 + row * 100) > 48)
            return;
        p.measure = firstMeasure + row * 4 + col;
        const auto length = editor->Timeline().MeasureLength(p.measure);
        const auto tick =
            static_cast<std::int64_t>(std::llround((point.x - 205 - col * 401) / 401.0 * length.Value() * division));
        p.fraction = chart::Rational{tick, division};
        if (p.fraction >= length)
            p.fraction = chart::Rational{std::max<std::int64_t>(0, tick - 1), division};
    }
    // Placing the first realtime endpoint must not pan the second-click grid.
    if (!realtime || tool == 0)
        timeMs = editor->Timeline().Compile(p).count() / 1000.0;
    if (tool == 0)
    {
        rebuild = true;
        return;
    }
    if (tool < 0)
    {
        timingFields[0] = std::to_string(p.measure + 1);
        timingFields[1] = Fraction(p.fraction);
        tab = 1;
        rebuild = true;
        return;
    }
    if (tool >= 11)
    {
        if (!pending)
            pending = p;
        else
        {
            editor->AddNote(std::min(*pending, p), tool == 18 ? 17 : tool, std::max(*pending, p),
                            tool >= 17
                                ? std::vector<std::string>{tool == 18 ? "Action=Kat" : "Action=Don", "TickDivision=16"}
                                : std::vector<std::string>{});
            pending.reset();
        }
    }
    else
        editor->AddNote(p, tool);
    rebuild = true;
}

void EditorScene::State::Build()
{
    controls.clear();
    visual->packets.clear();
    Box({0, 0, 1920, 1080}, Background);
    static constexpr const wchar_t *tabs[]{L"패턴", L"박자표", L"메타데이터", L"이펙트"};
    for (int i = 0; i < 4; ++i)
        Button(
            {i * 220.0F, 0, 220, 60}, tabs[i],
            [this, i] {
                tab = i;
                listOffset = 0;
                popup = -1;
                rebuild = true;
            },
            tab == i);
    if (tab == 0)
        DrawScore();
    else if (tab == 1)
        DrawTiming();
    else if (tab == 2)
        DrawMetadata();
    else
        DrawEffects();
    Button({1740, 1035, 150, 36}, editor->Dirty() ? L"저장 *" : L"저장", [this] { Save(); }, true);
    Text({24, 1040, 1690, 32}, Wide(status), 16);
    rebuild = false;
}

void EditorScene::State::DrawTiming()
{
    Text({136, 105, 1000, 40}, L"박자표 · BPM / 마디 길이", 28);
    Box({132, 173, 920, 650}, White, 12);
    Box({1080, 173, 780, 520}, White, 12);
    Text({156, 198, 830, 38}, L"마디     위치          종류            값", 22);
    const auto &timing = editor->Pattern().timing;
    for (std::size_t i = listOffset; i < timing.size() && i < listOffset + 11; ++i)
    {
        const auto &t = timing[i];
        const float y = 250 + static_cast<float>(i - listOffset) * 46;
        const auto value =
            t.type == chart::TimingDirectiveType::MeasureLength ? Fraction(t.ratio) : std::to_string(t.value);
        Button({156, y, 715, 40},
               std::to_wstring(t.position.measure + 1) + L"    " + Wide(Fraction(t.position.fraction)) + L"    " +
                   (t.type == chart::TimingDirectiveType::Bpm             ? L"BPM"
                    : t.type == chart::TimingDirectiveType::MeasureLength ? L"Measure"
                                                                          : L"Delay") +
                   L"    " + Wide(value),
               [this, t] {
                   timingFields[0] = std::to_string(t.position.measure + 1);
                   timingFields[1] = Fraction(t.position.fraction);
                   if (t.type == chart::TimingDirectiveType::Bpm)
                       timingFields[2] = std::to_string(t.value);
                   if (t.type == chart::TimingDirectiveType::MeasureLength)
                       timingFields[3] = Fraction(t.ratio);
                   rebuild = true;
               });
        Button({883, y, 125, 40}, L"삭제", [this, i] {
            auto p = editor->Pattern();
            p.timing.erase(p.timing.begin() + i);
            editor->Replace(p, editor->Effects());
            rebuild = true;
        });
    }
    Field({1104, 270, 180, 42}, L"시작 마디", timingFields[0]);
    Field({1310, 270, 210, 42}, L"변경 위치 N/D", timingFields[1]);
    Field({1550, 270, 275, 42}, L"BPM", timingFields[2]);
    Button(
        {1540, 350, 285, 42}, L"BPM 추가 / 수정",
        [this] {
            auto p = editor->Pattern();
            const auto pos = Position(timingFields[0], timingFields[1]);
            const auto bpm = Number(timingFields[2]);
            if (bpm <= 0)
                throw std::invalid_argument("BPM must be positive.");
            std::erase_if(p.timing, [pos](const auto &d) {
                return d.position == pos && d.type == chart::TimingDirectiveType::Bpm;
            });
            p.timing.push_back({pos, chart::TimingDirectiveType::Bpm, bpm});
            editor->Replace(p, editor->Effects());
            rebuild = true;
        },
        true);
    Field({1104, 460, 320, 42}, L"마디 길이 N/D", timingFields[3]);
    Button(
        {1470, 460, 355, 42}, L"마디 길이 적용",
        [this] {
            editor->SetMeasureLength(Integer(timingFields[0]) - 1, Position("1", timingFields[3]).fraction);
            rebuild = true;
        },
        true);
    Text({1104, 545, 715, 120},
         L"넘친 노트는 초과 박자를 유지해 다음 마디로 이동합니다.\n마디 밖의 BPM/이펙트는 먼저 이동해 주세요.", 19);
}

void EditorScene::State::DrawMetadata()
{
    Text({136, 105, 1000, 40}, L"메타데이터", 28);
    Box({132, 173, 1728, 800}, White, 12);
    auto metadata = [this](float y, const wchar_t *title, std::string value,
                           std::function<void(chart::PatternDocument &, const std::string &)> apply) {
        Text({156, y, 360, 40}, title, 22);
        Button({520, y, 1295, 44}, Wide(value), [this, title, value, apply] {
            if (auto edited = EditText(title, value))
            {
                auto p = editor->Pattern();
                apply(p, *edited);
                editor->Replace(p, editor->Effects());
                analysisDirty = true;
                rebuild = true;
            }
        });
    };
    const auto &p = editor->Pattern();
    metadata(225, L"YMM 상대 경로", Utf8(p.musicMetadataFile.wstring()),
             [](auto &d, const auto &value) { d.musicMetadataFile = std::filesystem::path(Wide(value)); });
    metadata(305, L"패턴 이름", p.name, [](auto &d, const auto &value) { d.name = value; });
    auto join = [](const std::vector<std::string> &values) {
        std::string r;
        for (const auto &s : values)
        {
            if (!r.empty())
                r += '\n';
            r += s;
        }
        return r;
    };
    auto lines = [](const std::string &value) {
        std::vector<std::string> r;
        std::istringstream in(value);
        std::string s;
        while (std::getline(in, s))
        {
            if (!s.empty() && s.back() == '\r')
                s.pop_back();
            if (!s.empty())
                r.push_back(s);
        }
        return r;
    };
    metadata(385, L"제작자 (한 줄에 한 명)", join(p.makers),
             [lines](auto &d, const auto &value) { d.makers = lines(value); });
    metadata(465, L"태그 (한 줄에 하나)", join(p.tags), [lines](auto &d, const auto &value) { d.tags = lines(value); });
    metadata(545, L"오프셋 (ms)", std::to_string(p.patternOffsetMilliseconds),
             [](auto &d, const auto &value) { d.patternOffsetMilliseconds = Number(value); });
    metadata(625, L"기본 BPM", std::to_string(p.baseBpm),
             [](auto &d, const auto &value) { d.baseBpm = Number(value); });
    std::string sounds;
    for (const auto &[id, path] : p.hitSounds)
        sounds += id + ": " + Utf8(path.wstring()) + "\n";
    metadata(705, L"히트사운드 테이블", sounds, [](auto &d, const auto &value) {
        const auto result = chart::ChartParser{}.ParsePattern("[HitSounds]\n" + value);
        if (!result.Succeeded())
            throw std::invalid_argument("Invalid hit sound table.");
        d.hitSounds = result.document.hitSounds;
    });
    Text({156, 805, 1640, 95},
         L"히트사운드는 인덱스: 상대경로로 등록합니다. 예: 1: Sounds/pop.wav\nYMP와 같은 폴더에 같은 이름의 YME를 "
         L"저장합니다. Ctrl+S: 저장",
         21);
}

void EditorScene::State::DrawEffects()
{
    Text({136, 105, 1000, 40}, L"이펙트", 28);
    Box({132, 173, 1728, 310}, White, 12);
    Box({132, 503, 1728, 365}, White, 12);
    Text({156, 190, 1650, 38}, L"시작 마디 / 박자      종류           값 / 참조 인덱스", 22);
    const auto &e = editor->Effects();
    const auto total = e.commands.size() + e.hitSoundChanges.size();
    static constexpr const wchar_t *names[]{L"볼륨",        L"마디선",    L"노트 속도",
                                            L"스크롤 속도", L"동 사운드", L"캇 사운드"};
    for (std::size_t i = listOffset; i < total && i < listOffset + 4; ++i)
    {
        const float y = 245 + static_cast<float>(i - listOffset) * 48;
        std::wstring text;
        if (i < e.commands.size())
        {
            const auto &c = e.commands[i];
            text = std::to_wstring(c.position.measure + 1) + L" / " + Wide(Fraction(c.position.fraction)) + L"    " +
                   std::to_wstring(static_cast<int>(c.type)) + L"    " + std::to_wstring(c.beginValue) + L" → " +
                   std::to_wstring(c.endValue);
        }
        else
        {
            const auto &c = e.hitSoundChanges[i - e.commands.size()];
            text = std::to_wstring(c.position.measure + 1) + L" / " + Wide(Fraction(c.position.fraction)) +
                   (c.keyType == 1 ? L"    동    " : L"    캇    ") + Wide(c.soundIndex);
        }
        Button({156, y, 1430, 40}, text, [this, i] {
            const auto &effects = editor->Effects();
            effectFields[2].clear();
            effectFields[3].clear();
            if (i < effects.commands.size())
            {
                const auto &c = effects.commands[i];
                if (c.type == chart::EffectCommandType::BusVolume)
                    effectType = 0;
                else if (c.type == chart::EffectCommandType::MeasureLineVisible)
                    effectType = 1;
                else if (c.type == chart::EffectCommandType::NoteSpeed)
                    effectType = 2;
                else if (c.type == chart::EffectCommandType::ScrollSpeed)
                    effectType = 3;
                else
                    throw std::runtime_error("This legacy effect is preserved; its editor is not part of this task.");
                effectFields[0] = std::to_string(c.position.measure + 1);
                effectFields[1] = Fraction(c.position.fraction);
                effectFields[4] = std::to_string(c.beginValue);
                effectFields[5] = std::to_string(c.endValue);
                effectFields[6] = c.target;
                curve = static_cast<int>(c.curve);
                if (c.endPosition)
                {
                    effectFields[2] = std::to_string(c.endPosition->measure + 1);
                    effectFields[3] = Fraction(c.endPosition->fraction);
                }
            }
            else
            {
                const auto &c = effects.hitSoundChanges[i - effects.commands.size()];
                effectType = c.keyType == 1 ? 4 : 5;
                effectFields[0] = std::to_string(c.position.measure + 1);
                effectFields[1] = Fraction(c.position.fraction);
                effectFields[4] = c.soundIndex;
            }
            rebuild = true;
        });
        Button({1620, y, 195, 40}, L"삭제", [this, i] {
            auto effects = editor->Effects();
            if (i < effects.commands.size())
                effects.commands.erase(effects.commands.begin() + i);
            else
                effects.hitSoundChanges.erase(effects.hitSoundChanges.begin() + (i - effects.commands.size()));
            editor->Replace(editor->Pattern(), effects);
            rebuild = true;
        });
    }
    Field({156, 595, 188, 42}, L"시작 마디", effectFields[0]);
    Field({370, 595, 188, 42}, L"시작 박자", effectFields[1]);
    Field({584, 595, 188, 42}, L"끝 마디 (선택)", effectFields[2]);
    Field({798, 595, 188, 42}, L"끝 박자 (선택)", effectFields[3]);
    Button({1036, 595, 789, 42}, names[effectType], [this] {
        effectType = (effectType + 1) % 6;
        rebuild = true;
    });
    Field({156, 718, 319, 42}, effectType >= 4 ? L"히트사운드 인덱스" : L"시작 값 (마디선 0/1)", effectFields[4]);
    Field({500, 718, 319, 42}, L"끝 값", effectFields[5]);
    static constexpr const wchar_t *curves[]{L"Step", L"Linear", L"Smoothstep", L"Exponential"};
    Button({844, 718, 360, 42}, curves[curve], [this] {
        curve = (curve + 1) % 4;
        rebuild = true;
    });
    Field({1230, 718, 245, 42}, L"오디오 버스", effectFields[6]);
    Button(
        {1508, 718, 317, 42}, L"추가 / 같은 위치 수정",
        [this] {
            auto effects = editor->Effects();
            const auto p = Position(effectFields[0], effectFields[1]);
            if (effectType >= 4)
            {
                const int key = effectType == 4 ? 1 : 2;
                std::erase_if(effects.hitSoundChanges,
                              [&](const auto &c) { return c.position == p && c.keyType == key; });
                effects.hitSoundChanges.push_back({p, effectFields[4], key});
            }
            else
            {
                constexpr chart::EffectCommandType types[]{
                    chart::EffectCommandType::BusVolume, chart::EffectCommandType::MeasureLineVisible,
                    chart::EffectCommandType::NoteSpeed, chart::EffectCommandType::ScrollSpeed};
                chart::EffectCommand c;
                c.position = p;
                c.type = types[effectType];
                c.target = effectType == 0 ? effectFields[6] : "";
                c.beginValue = Number(effectFields[4]);
                c.endValue = Number(effectFields[5]);
                c.curve = static_cast<chart::AutomationCurve>(curve);
                if (!effectFields[2].empty() || !effectFields[3].empty())
                    c.endPosition = Position(effectFields[2], effectFields[3]);
                else
                    c.endValue = c.beginValue;
                if (effectType >= 2 && (c.beginValue <= 0 || c.endValue <= 0))
                    throw std::invalid_argument("Speed must be positive.");
                std::erase_if(effects.commands, [&](const auto &old) {
                    return old.position == p && old.type == c.type && old.target == c.target;
                });
                effects.commands.push_back(std::move(c));
            }
            editor->Replace(editor->Pattern(), effects);
            rebuild = true;
        },
        true);
    Text({156, 800, 1620, 50},
         L"동·캇은 독립 설정입니다. 둘 다 변경하려면 같은 위치에 각각 추가하세요. 변경 값은 다음 지시까지 유지합니다.",
         18);
}

void EditorScene::State::CacheAudioMarkers()
{
    if (audioMarkerRevision == editor->Revision())
        return;
    audioMarkers.clear();
    const auto append = [&](const chart::PatternNote &note, chart::MusicalPosition position,
                            finger_drum::rhythm::RhythmTime time) {
        bool kat = note.keyType == 2 || note.keyType == 4;
        for (auto option : note.extraData)
        {
            std::ranges::transform(option, option.begin(),
                                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (option == "action=kat")
                kat = true;
        }
        std::string sound = kat ? (note.keyType == 4 ? "BigKat" : "Kat") : (note.keyType == 3 ? "BigDon" : "Don");
        const chart::HitSoundChange *last = nullptr;
        for (const auto &c : editor->Effects().hitSoundChanges)
            if (c.keyType == (kat ? 2 : 1) && c.position <= position && (!last || last->position <= c.position))
                last = &c;
        if (last)
            sound = "Table." + last->soundIndex;
        if (!note.hitSound.empty())
            sound = "Table." + note.hitSound;
        audioMarkers.push_back({time.count() / 1e6, std::move(sound)});
    };
    std::optional<chart::PatternNote> head;
    const auto &timeline = editor->Timeline();
    for (const auto &n : editor->Notes())
    {
        if (n.note.actionType == 1 && !head)
            head = n.note;
        if (n.note.actionType != 2)
            append(n.note, n.note.position, n.timing);
        else if (head && head->keyType == n.note.keyType)
        {
            if (head->keyType == 12 || head->keyType == 14 || head->keyType == 17)
            {
                std::int64_t tickDivision = 16;
                for (auto option : head->extraData)
                {
                    std::ranges::transform(option, option.begin(),
                                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                    if (option.starts_with("tickdivision="))
                        tickDivision = Integer(option.substr(13));
                }
                if (tickDivision <= 0 || tickDivision > 1024)
                    throw std::invalid_argument("TickDivision must be 1..1024.");
                const auto ticks = timeline.CompileSubdivisions(head->position, n.note.position,
                                                                static_cast<std::size_t>(tickDivision));
                const auto first = timeline.PositionToWholeNotes(head->position);
                // The head was already added. Remaining markers correspond to
                // successful TickRoll taps or a continuously held Buzz.
                for (std::size_t i = 1; i < ticks.size(); ++i)
                    append(*head,
                           timeline.PositionAtWholeNotes(first +
                                                         chart::Rational{static_cast<std::int64_t>(i), tickDivision}),
                           ticks[i]);
            }
            head.reset();
        }
    }
    std::ranges::stable_sort(audioMarkers, {}, &AudioMarker::seconds);
    audioMarkerRevision = editor->Revision();
}

void EditorScene::State::DrawAudio()
{
    try
    {
        CacheAudioMarkers();
    }
    catch (const std::exception &error)
    {
        audioMarkers.clear();
        audioMarkerRevision = editor->Revision();
        status = error.what();
    }
    Box({112, 806, 1784, 250}, Paper);
    Text({132, 813, 400, 28}, L"오디오 분석 / 히트사운드", 22);
    Box({205, 845, 1399, 60}, {.07F, .15F, .20F, 1}, 4);
    Box({205, 915, 1399, 68}, {.07F, .15F, .20F, 1}, 4);
    const double begin = timeMs / 1000.0 - audioWindow * .25;
    const auto drawSpectrum = [&](const finger_drum::editor::AudioAnalysis &data, double offset, bool hit) {
        if (data.frames.empty() || data.secondsPerFrame <= 0)
            return;
        constexpr int columns = 350;
        for (int col = 0; col < columns; ++col)
        {
            const double left = begin + audioWindow * col / columns - offset;
            const double right = left + audioWindow / columns;
            if (right <= 0 || left >= data.durationSeconds)
                continue;
            const double seconds = std::max(0.0, left);
            const auto index =
                std::min(data.frames.size() - 1, static_cast<std::size_t>(seconds / data.secondsPerFrame));
            auto frame = data.frames[index];
            const auto lastIndex = std::min(
                data.frames.size(),
                static_cast<std::size_t>(std::ceil(std::min(data.durationSeconds, right) / data.secondsPerFrame)));
            // Max-pool rather than skip short transients when zoomed out.
            for (auto i = index + 1; i < lastIndex; ++i)
            {
                frame.peak = std::max(frame.peak, data.frames[i].peak);
                for (std::size_t band = 0; band < frame.bands.size(); ++band)
                    frame.bands[band] = std::max(frame.bands[band], data.frames[i].bands[band]);
            }
            frame.peak = std::min(1.0F, frame.peak);
            const float x = 205 + 1399.0F * col / columns;
            if (!hit)
                Box({x, 875 - frame.peak * 27, 3, std::max(1.0F, frame.peak * 54)}, Kat);
            for (std::size_t band = 0; band < frame.bands.size(); ++band)
            {
                const float level = frame.bands[band];
                if (level < .06F)
                    continue;
                Box({x, 980 - static_cast<float>(band) * 2.65F, 4, 2.7F},
                    hit ? v::Color{1, .83F, .3F, level * .85F}
                        : v::Color{.10F + .3F * level, .2F + .6F * level, .3F + .6F * level, 1});
            }
        }
    };
    if (const auto music = analysis.sounds.find("Music"); music != analysis.sounds.end())
        drawSpectrum(music->second, 0, false);
    double longestSound = 0;
    for (const auto &[id, sound] : analysis.sounds)
        if (id != "Music")
            longestSound = std::max(longestSound, sound.durationSeconds);
    const auto firstMarker = std::ranges::lower_bound(audioMarkers, begin - longestSound, {}, &AudioMarker::seconds);
    for (auto marker = firstMarker; marker != audioMarkers.end() && marker->seconds <= begin + audioWindow; ++marker)
    {
        const auto seconds = marker->seconds;
        if (const auto found = analysis.sounds.find(marker->sound); found != analysis.sounds.end())
            drawSpectrum(found->second, seconds, true);
        const float x = 205 + static_cast<float>((seconds - begin) / audioWindow) * 1399;
        if (x >= 205 && x <= 1604)
            Box({x, 915, 2, 68}, Gold);
    }
    Box({205 + 1399 * .25F, 843, 2, 140}, White);
    Button({132, 855, 54, 42}, L"+", [this] {
        audioWindow = std::max(.25, audioWindow / 2);
        rebuild = true;
    });
    Button({132, 915, 54, 42}, L"−", [this] {
        audioWindow = std::min(120.0, audioWindow * 2);
        rebuild = true;
    });
    Button({1620, 845, 260, 42}, L"현재 시간 (ms)", [this] {
        if (auto s = EditText(L"현재 시간 (ms)", std::to_string(timeMs)))
        {
            timeMs = Number(*s);
            rebuild = true;
        }
    });
    Text({1620, 903, 260, 40}, Wide(std::to_string(timeMs)), 20);
    Text({205, 995, 1500, 38}, L"마우스 휠: 마디 이동 · ← / →: 1ms · Ctrl+S: 저장", 18);
    if (analysisJob.valid())
        Text({620, 813, 850, 28}, L"오디오 분석 중…", 18, Blue);
}

void EditorScene::State::UpdateAnalysis()
{
    // Asset resolution/worker restart happens only after metadata changes.
    // Ordinary update ticks only poll the asynchronous result.
    if (analysisDirty)
    {
        analysisDirty = false;
        const auto metadataPath = editor->Pattern().sourcePath.parent_path() / editor->Pattern().musicMetadataFile;
        const auto music = chart::ChartParser{}.ParseMusicFile(metadataPath);
        if (!music.Succeeded())
            throw std::runtime_error("Audio analysis: the selected YMM could not be parsed.");
        request->musicPath = metadataPath.parent_path() / music.document.audioFile;
        std::map<std::string, std::filesystem::path> files{{"Music", request->musicPath}};
        for (const auto &[id, file] : std::array<std::pair<const char *, const wchar_t *>, 4>{
                 {{"Don", L"don.wav"}, {"Kat", L"kat.wav"}, {"BigDon", L"bigdon.wav"}, {"BigKat", L"bigkat.wav"}}})
            files[id] = mrg_client::asset_paths::default_skin::TaikoHitSound(file);
        for (const auto &[id, path] : editor->Pattern().hitSounds)
            files["Table." + id] = editor->Pattern().sourcePath.parent_path() / path;
        if (files != analysisFiles)
        {
            analysisStop.request_stop();
            if (analysisJob.valid())
                analysisJob.wait();
            analysisStop = std::stop_source{};
            analysisFiles = files;
            analysisJob = std::async(std::launch::async, [files = std::move(files), stop = analysisStop.get_token()] {
                AnalysisBatch result;
                for (const auto &[id, path] : files)
                {
                    if (stop.stop_requested())
                        break;
                    try
                    {
                        result.sounds.emplace(id, finger_drum::editor::AnalyzeAudio(path, stop));
                    }
                    catch (const std::exception &error)
                    {
                        result.errors += id + ": " + error.what() + "; ";
                    }
                }
                return result;
            });
        }
    }
    if (analysisJob.valid() && analysisJob.wait_for(std::chrono::seconds{0}) == std::future_status::ready)
    {
        analysis = analysisJob.get();
        if (!analysis.errors.empty())
            status = analysis.errors;
        rebuild = true;
    }
}

EditorScene::EditorScene(v::ScreenVisual2DManager &visuals, std::shared_ptr<finger_drum::GameplayLaunchRequest> request)
    : state_(std::make_unique<State>(visuals, std::move(request)))
{
}
EditorScene::~EditorScene() = default;
void EditorScene::Initialize(const mrg::EngineServices &services)
{
    auto &s = *state_;
    s.width = services.windowWidth;
    s.height = services.windowHeight;
    chart::ChartParser parser;
    auto p = parser.ParsePatternFile(s.request->patternPath);
    chart::ParseResult<chart::EffectDocument> e;
    if (s.request->effectPath)
        e = parser.ParseEffectFile(*s.request->effectPath);
    if (!p.Succeeded() || !e.Succeeded())
        throw std::runtime_error("Editor could not parse the selected chart.");
    s.editor = std::make_unique<chart::ChartEditor>(std::move(p.document), std::move(e.document));
    s.canvas = s.visuals.CreateOwnedCanvas({{1920, 1080}, v::CanvasScaleMode::FixedHeight});
    static_cast<void>(s.canvas.SetVisible(true));
    s.node = &s.canvas.Get()->CreateNode(v::Anchor::Center, "Editor");
    s.visual = &s.node->AddComponent<EditorVisual>();
    s.ReloadSize();
    s.Build();
}
void EditorScene::Update(const mrg::UpdateContext &context, mrg::scene::SceneManager &scenes)
{
    auto &s = *state_;
    const auto &input = context.input;
    try
    {
        s.UpdateAnalysis();
        if (input.WasKeyPressed(VK_ESCAPE))
        {
            if (s.pending || s.popup >= 0)
            {
                s.pending.reset();
                s.popup = -1;
                s.rebuild = true;
            }
            else if (!s.editor->Dirty() ||
                     MessageBoxW(GetActiveWindow(), L"저장하지 않은 변경을 버리고 나가시겠습니까?",
                                 L"FingerDrum Editor", MB_YESNO | MB_ICONQUESTION) == IDYES)
                static_cast<void>(scenes.ChangeScene(finger_drum::scene_ids::EditorSongSelect));
        }
        if (input.IsKeyDown(VK_CONTROL) && input.WasKeyPressed('S'))
            s.Save();
        if (input.WasKeyPressed(VK_LEFT))
        {
            s.timeMs -= 1;
            s.rebuild = true;
        }
        if (input.WasKeyPressed(VK_RIGHT))
        {
            s.timeMs += 1;
            s.rebuild = true;
        }
        if (input.MouseWheelDelta() != 0)
        {
            const int delta = input.MouseWheelDelta() > 0 ? -1 : 1;
            if (s.tab == 0)
                s.firstMeasure = std::max<std::int64_t>(0, s.firstMeasure + delta * 4);
            else
                s.listOffset = static_cast<std::size_t>(
                    std::max<std::int64_t>(0, static_cast<std::int64_t>(s.listOffset) + delta));
            s.rebuild = true;
        }
        const float scale = std::max(.001F, std::min(s.width / 1920.0F, s.height / 1080.0F));
        const v::Point point{(input.MousePositionX() - s.width * .5F) / scale + 960,
                             (input.MousePositionY() - s.height * .5F) / scale + 540};
        if (input.WasMouseButtonPressed(mrg::platform::MouseButton::Right) && s.tab == 0)
        {
            if (point.x >= 24 && point.x < 94 && point.y >= 202 && point.y < 522)
            {
                s.popup = point.y < 282 ? 1 : point.y < 362 ? 2 : 3;
                s.pending.reset();
                s.rebuild = true;
            }
            else
                s.EditScore(point, true);
        }
        if (input.WasMouseButtonPressed(mrg::platform::MouseButton::Left))
        {
            auto found = std::find_if(s.controls.rbegin(), s.controls.rend(),
                                      [point](const Control &c) { return c.rect.Contains(point); });
            if (found != s.controls.rend())
            {
                auto action = found->click;
                action();
            }
            else if (s.tab == 0)
                s.EditScore(point, false);
        }
    }
    catch (const std::exception &e)
    {
        s.status = e.what();
        s.rebuild = true;
    }
    if (s.rebuild)
        s.Build();
}
void EditorScene::OnResize(std::uint32_t width, std::uint32_t height)
{
    state_->width = width;
    state_->height = height;
    if (state_->node)
        state_->ReloadSize();
}
void EditorScene::Shutdown() noexcept
{
    state_->analysisStop.request_stop();
    state_->canvas.Reset();
    state_->node = nullptr;
    state_->visual = nullptr;
}
void EditorScene::Render(const mrg::graphics::RenderContext &)
{
}
