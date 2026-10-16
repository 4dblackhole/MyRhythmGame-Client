#include "EditorModeTests.h"
#include "Editing/ChartEditor.h"
#include "EditorScene/Submodules/EditorView.h"
#include "EditorScene/Submodules/Modes/EditorModeFactory.h"
#include "EditorScene/Submodules/Modes/Taiko/TaikoEditorMode.h"
#include "EditorScene/Submodules/Modes/Taiko/TaikoEditorNoteInspector.h"
#include "Taiko/TaikoMode.h"
#include "Texts/EditorScene/Taiko/TaikoEditorTexts.h"
#include <limits>
#include <stdexcept>

namespace
{
    const finger_drum::texts::TextCatalog &TestCatalog(finger_drum::texts::Language language)
    {
        struct Catalogs
        {
            finger_drum::texts::TextCatalog ko, en;
            Catalogs()
            {
                en.SetLanguage(finger_drum::texts::Language::English);
            }
        };
        static const Catalogs catalogs;
        return language == finger_drum::texts::Language::Korean ? catalogs.ko : catalogs.en;
    }
} // namespace
namespace
{
    namespace chart = finger_drum::chart;
    namespace v = mrg::visual2d;
    void Check(bool value, const char *message)
    {
        if (!value)
            throw std::runtime_error(message);
    }
    class RecordingCanvas final : public IEditorModeCanvas
    {
      public:
        std::vector<v::Rect> rectangles;
        std::vector<std::function<void()>> actions;
        std::vector<std::filesystem::path> images;
        std::vector<v::Rect> imageRectangles;
        std::vector<std::wstring> labels;
        std::vector<std::wstring> buttonLabels;
        void Box(v::Rect r, v::Color, float) override
        {
            rectangles.push_back(r);
        }
        void Text(v::Rect, std::wstring text, float, v::Color) override
        {
            labels.push_back(std::move(text));
        }
        void Image(v::Rect rect, const std::filesystem::path &path, v::Color) override
        {
            rectangles.push_back(rect);
            images.push_back(path);
            imageRectangles.push_back(rect);
        }
        void Button(v::Rect, std::wstring text, std::function<void()> action, bool) override
        {
            actions.push_back(std::move(action));
            buttonLabels.push_back(text);
            labels.push_back(std::move(text));
        }
    };

    // A test-only format adapter demonstrates that the workspace neither
    // constructs ChartEditor nor forces YMP serialization during Save.
    class ProbeDocument final : public chart::IEditorDocument
    {
      public:
        chart::ChartEditor score{chart::PatternDocument{}};
        bool saved{};
        const chart::PatternDocument &Pattern() const noexcept override
        {
            return score.Pattern();
        }
        const chart::EffectDocument &Effects() const noexcept override
        {
            return score.Effects();
        }
        const chart::MusicalTimeline &Timeline() const noexcept override
        {
            return score.Timeline();
        }
        const std::vector<chart::CompiledPatternNote> &Notes() const noexcept override
        {
            return score.Notes();
        }
        bool Dirty() const noexcept override
        {
            return score.Dirty();
        }
        std::vector<chart::CompiledPatternNote> NotesInMeasures(std::int64_t begin, std::int64_t end) const override
        {
            return score.NotesInMeasures(begin, end);
        }
        std::vector<chart::CompiledPatternNote> NotesInTimeRange(finger_drum::rhythm::RhythmTime begin,
                                                                 finger_drum::rhythm::RhythmTime end) const override
        {
            return score.NotesInTimeRange(begin, end);
        }
        std::uint64_t Revision() const noexcept override
        {
            return score.Revision();
        }
        void Replace(chart::PatternDocument p, chart::EffectDocument e) override
        {
            score.Replace(std::move(p), std::move(e));
        }
        void AddNote(chart::MusicalPosition p, int key, std::optional<chart::MusicalPosition> end,
                     std::vector<std::string> extra) override
        {
            score.AddNote(p, key, end, std::move(extra));
        }
        void ReplaceTiming(std::vector<chart::TimingDirective> timing) override
        {
            score.ReplaceTiming(std::move(timing));
        }
        void ReplaceEffects(chart::EffectDocument effects) override
        {
            score.ReplaceEffects(std::move(effects));
        }
        std::optional<chart::EditorNoteSelection> FindNote(std::size_t id) const override
        {
            return score.FindNote(id);
        }
        void UpdateNote(std::size_t id, chart::PatternNote note, std::optional<chart::MusicalPosition> end) override
        {
            score.UpdateNote(id, std::move(note), end);
        }
        void DeleteNote(std::size_t order) override
        {
            score.DeleteNote(order);
        }
        void SetMeasureLength(std::int64_t measure, chart::Rational length) override
        {
            score.SetMeasureLength(measure, length);
        }
        void Save() override
        {
            saved = true;
        }
    };
    class ProbeMode final : public IEditorMode
    {
      public:
        int selected{42};
        mutable int markerCalls{};
        bool failDrawing{};
        int chartCalls{};
        std::string_view Id() const noexcept override
        {
            return "TestOnly";
        }
        std::unique_ptr<chart::IEditorDocument> OpenDocument(const finger_drum::GameplayLaunchRequest &) const override
        {
            return std::make_unique<ProbeDocument>();
        }
        void SelectTool(int id) override
        {
            selected = id;
        }
        void PlaceNote(IEditorContext &state, chart::MusicalPosition p) override
        {
            state.AddNote(p, selected);
            state.RequestRebuild();
        }
        bool HasPendingPlacement() const noexcept override
        {
            return false;
        }
        bool CancelInteraction() noexcept override
        {
            return false;
        }
        void CloseToolMenu() noexcept override
        {
        }
        bool OpenToolMenu(v::Point) override
        {
            return false;
        }
        void DrawTools(IEditorModeCanvas &canvas, IEditorContext &state,
                       const finger_drum::texts::TextCatalog &) override
        {
            canvas.Button({30, 130, 60, 40}, L"PROBE TOOL", [&state] { state.SelectTool(70); });
        }
        void DrawChart(IEditorModeCanvas &canvas, IEditorContext &, const finger_drum::texts::TextCatalog &) override
        {
            ++chartCalls;
            if (failDrawing)
                throw std::runtime_error("Injected chart drawing failure.");
            // Vertical rectangular lanes instead of Taiko circles/horizontal lane.
            canvas.Box({600, 200, 40, 500}, {.2F, .8F, .4F, 1});
            canvas.Box({650, 200, 40, 500}, {.2F, .4F, .8F, 1});
            canvas.Text({600, 160, 200, 30}, L"PROBE CHART");
        }
        void DrawToolMenu(IEditorModeCanvas &, IEditorContext &, const finger_drum::texts::TextCatalog &) override
        {
        }
        void EditScore(IEditorContext &state, v::Point point, bool erase) override
        {
            if (erase)
                state.DeleteNote(0);
            else
                PlaceNote(state, {static_cast<std::int64_t>(point.y / 100), {}});
        }
        void ScrollScore(IEditorContext &state, int direction) override
        {
            state.SetFirstMeasure(state.Score().firstMeasure + direction);
        }
        void DrawMetadata(IEditorModeCanvas &canvas, IEditorContext &, const finger_drum::texts::TextCatalog &) override
        {
            canvas.Text({600, 200, 300, 30}, L"PROBE FORMAT METADATA");
        }
        std::vector<EditorSoundChoice> SoundChoices(const finger_drum::texts::TextCatalog &) const override
        {
            return {{42, L"Probe key", L"PROBE SOUND"}};
        }
        std::wstring_view SoundEffectHelp(const finger_drum::texts::TextCatalog &) const override
        {
            return L"PROBE SOUND HELP";
        }
        std::map<std::string, std::filesystem::path> AudioFiles(
            const chart::IEditorDocument &, const finger_drum::GameplayLaunchRequest &) const override
        {
            return {};
        }
        std::vector<AudioMarker> AudioMarkers(const chart::IEditorDocument &) const override
        {
            ++markerCalls;
            return {{.5, "ProbeCue"}};
        }
    };

    void TestTaikoEditing()
    {
        for (const std::string_view id : {"", "unsupported"})
        {
            bool rejected = false;
            try
            {
                static_cast<void>(CreateEditorMode(id));
            }
            catch (const std::invalid_argument &)
            {
                rejected = true;
            }
            Check(rejected, "Empty or unknown modes must not silently use Taiko tools.");
        }
        for (const int id : {1, 2, 3, 4, 5, 11, 12, 13, 14, 15, 16, 17, 18})
        {
            EditorWorkspace state(CreateEditorMode("Taiko"),
                                  std::make_unique<chart::ChartEditor>(chart::PatternDocument{}));
            state.SelectTool(id);
            state.PlaceNote({0, {}});
            const bool isLong = id >= 11;
            Check(state.Mode().HasPendingPlacement() == isLong, "Taiko long-note placement changed.");
            if (isLong)
                state.PlaceNote({0, {1, 4}});
            const auto &notes = state.Document().Notes();
            Check(notes.size() == (isLong ? 2 : 1) && notes.front().note.keyType == (id == 18 ? 17 : id),
                  "Taiko tool persisted the wrong note type.");
            if (id == 18)
                Check(notes.front().note.extraData == std::vector<std::string>{"Action=Kat", "TickDivision=16"},
                      "Kat Buzz options changed.");
            const auto markers = state.Mode().AudioMarkers(state.Document());
            const bool hasTicks = id == 12 || id == 14 || id == 17 || id == 18;
            Check(markers.size() == (hasTicks ? 5 : id == 5 ? 2 : 1), "Taiko expected-hit count changed.");
            Check(markers.front().sound == (id == 2 || id == 18 ? "Taiko.Kat.Hit"
                                            : id == 4           ? "Taiko.BigKat.FirstHit"
                                            : id == 3           ? "Taiko.BigDon.FirstHit"
                                                                : "Taiko.Don.Hit"),
                  "Taiko hit-sound mapping changed.");
        }
        EditorWorkspace state(CreateEditorMode("Taiko"),
                              std::make_unique<chart::ChartEditor>(chart::PatternDocument{}));
        RecordingCanvas canvas;
        state.Mode().DrawChart(canvas, state, TestCatalog(finger_drum::texts::Language::Korean));
        state.Mode().EditScore(state, {283.75F, 174}, false);
        Check(state.Document().Notes().front().note.position.fraction == chart::Rational{1, 4},
              "Score click no longer snaps using quarter-note division.");
        state.Mode().DrawChart(canvas, state, TestCatalog(finger_drum::texts::Language::Korean));
        state.Mode().EditScore(state, {283.75F, 174}, true);
        Check(state.Document().Notes().empty(), "Mode-owned note hit testing did not delete the note.");
        state.SetRealtime(true);
        state.SelectTool(1);
        state.Mode().DrawChart(canvas, state, TestCatalog(finger_drum::texts::Language::Korean));
        for (const v::Point outside : {v::Point{120, 465}, {1849, 465}, {230, 350}, {230, 580}})
            state.Mode().EditScore(state, outside, false);
        Check(state.Document().Notes().empty(), "Clicks outside the realtime lane must not place notes.");
        state.Mode().EditScore(state, {230, 465}, false);
        Check(state.Document().Notes().size() == 1, "A realtime lane click must still place a snapped note.");
        state.Mode().DrawChart(canvas, state, TestCatalog(finger_drum::texts::Language::Korean));
        state.Mode().EditScore(state, {120, 465}, true);
        Check(state.Document().Notes().size() == 1, "Outside-lane deletion must not affect nearby notes.");
        state.Mode().EditScore(state, {230, 465}, true);
        Check(state.Document().Notes().empty(), "In-lane realtime deletion stopped working.");
        state.Seek(123);
        state.SelectTool(11);
        state.PlaceNote({0, {1, 4}});
        Check(state.TimeMilliseconds() == 123 && state.Mode().CancelInteraction() &&
                  !state.Mode().HasPendingPlacement(),
              "Realtime long-note placement must preserve time and support cancellation.");
        state.Mode().DrawTools(canvas, state, TestCatalog(finger_drum::texts::Language::English));
        canvas.actions.front()();
        state.PlaceNote({0, {}});
        Check(state.Document().Notes().empty(), "Selection tool must not place a note.");
    }

    void TestTaikoPlacementConflicts()
    {
        EditorWorkspace state(CreateEditorMode("Taiko"),
                              std::make_unique<chart::ChartEditor>(chart::PatternDocument{}));
        state.SelectTool(11);
        state.PlaceNote({1, {}});
        state.PlaceNote({3, {}});
        const auto revision = state.Document().Revision();
        const auto reject = [&state, revision](chart::MusicalPosition position)
        {
            bool rejected = false;
            try
            {
                state.PlaceNote(position);
            }
            catch (const std::invalid_argument &)
            {
                rejected = true;
            }
            Check(rejected && state.Document().Revision() == revision && state.Document().Notes().size() == 2,
                  "Conflicting placement must preserve the document and its revision.");
        };
        state.SelectTool(1);
        reject({1, {}});
        reject({2, {}});
        state.SelectTool(12);
        state.PlaceNote({2, {}});
        reject({4, {}});
        Check(state.Mode().HasPendingPlacement(),
              "Failed second endpoint must remain available for correction/cancel.");
        state.Mode().CancelInteraction();
        state.SelectTool(11);
        state.PlaceNote({3, {}});
        state.PlaceNote({4, {}});
        Check(state.Document().Notes().size() == 4, "Adjacent long notes must still be placeable.");
        state.SelectTool(1);
        state.PlaceNote({5, {}});
        state.SelectTool(11);
        state.PlaceNote({4, {1, 2}});
        bool rejected = false;
        try
        {
            state.PlaceNote({6, {}});
        }
        catch (const std::invalid_argument &)
        {
            rejected = true;
        }
        Check(rejected && state.Document().Notes().size() == 5,
              "New long notes must not swallow already-authored ordinary notes.");
        state.Mode().CancelInteraction();
        state.SetFirstMeasure(chart::MaximumMeasureCount - 1);
        RecordingCanvas canvas;
        state.Mode().DrawChart(canvas, state, TestCatalog(finger_drum::texts::Language::Korean));
        state.SetRealtime(true);
        state.Seek(state.Document().Timeline().Compile({chart::MaximumMeasureCount - 1, {}}).count() / 1000.0);
        state.Mode().DrawChart(canvas, state, TestCatalog(finger_drum::texts::Language::Korean));
    }

    void TestDelayViewsAndSnapping()
    {
        chart::PatternDocument pattern;
        pattern.mode = "Taiko";
        pattern.baseBpm = 120;
        pattern.notes = {{{0, {3, 4}}, 1}, {{1, {1, 4}}, 2}};
        pattern.timing = {{{0, {3, 4}}, chart::TimingDirectiveType::Bpm, 120},
                          {{1, {1, 4}}, chart::TimingDirectiveType::DelayMilliseconds, -1500}};
        EditorWorkspace state(CreateEditorMode("Taiko"), std::make_unique<chart::ChartEditor>(pattern));
        state.SelectTool(0);
        RecordingCanvas overview;
        state.Mode().DrawChart(overview, state, TestCatalog(finger_drum::texts::Language::English));
        Check(overview.images.size() == 2 && std::ranges::find(overview.labels, L"1  2/4") != overview.labels.end() &&
                  std::ranges::find(overview.labels, L"2  3/4") != overview.labels.end() &&
                  std::ranges::none_of(overview.labels, [](const auto &label) { return label.starts_with(L"BPM"); }),
              "Overview must show effective bar lengths, exclude overwritten taps/BPM, and retain the new endpoint.");
        state.Mode().EditScore(state, {520, 174}, false);
        RecordingCanvas selected;
        state.Mode().DrawChart(selected, state, TestCatalog(finger_drum::texts::Language::English));
        Check(std::ranges::find(selected.labels, L"1/4") != selected.labels.end(),
              "A note at the shortened measure start must select its unchanged source fraction.");
        state.Mode().CancelInteraction();
        state.SelectTool(-1);
        state.Mode().EditScore(state, {520, 174}, false);
        Check(state.TimeMilliseconds() == 1000 && state.timingForm.measure == "2" && state.timingForm.fraction == "1/4",
              "Overview snapping must use the visible delayed grid rather than the original uniform source spacing.");
        state.SetRealtime(true);
        chart::EffectDocument effects;
        effects.commands.push_back(
            {.position = {0, {}}, .type = chart::EffectCommandType::MeasureLineVisible, .endValue = 1});
        state.Replace(state.Document().Pattern(), effects);
        RecordingCanvas realtime;
        state.Mode().DrawChart(realtime, state, TestCatalog(finger_drum::texts::Language::English));
        Check(std::ranges::any_of(realtime.rectangles, [](const auto &rect)
                                  { return std::abs(rect.x - 230) < .01F && rect.y == 361 && rect.width == 3; }),
              "Realtime must draw the clipped measure start at the judgement line.");
        state.Mode().EditScore(state, {230, 465}, false);
        Check(state.timingForm.measure == "2" && state.timingForm.fraction == "1/4" && state.TimeMilliseconds() == 1000,
              "Realtime and overview must select the same surviving beat at the clipped bar boundary.");
    }

    void TestRealtimeZoomAndTransport()
    {
        for (const double zoom : {.25, .5, 1., 2., 4.})
        {
            chart::PatternDocument pattern;
            pattern.baseBpm = 120;
            EditorWorkspace state(CreateEditorMode("Taiko"), std::make_unique<chart::ChartEditor>(pattern));
            state.SetRealtime(true);
            state.SetRealtimeZoom(zoom);
            RecordingCanvas empty;
            state.Mode().DrawChart(empty, state, TestCatalog(finger_drum::texts::Language::English));
            Check(std::ranges::any_of(empty.rectangles, [](const auto &rect) {
                      return rect.x == 164 && rect.y == 351 && rect.width == 1324 && rect.height == 228;
                  }),
                  "Time zoom must not resize the realtime lane.");
            const float x = 230 + 244.8F * static_cast<float>(zoom);
            state.Mode().EditScore(state, {x, 465}, false);
            Check(state.Document().Notes().size() == 1 &&
                  state.Document().Notes().front().note.position.fraction == chart::Rational{1, 4},
                  "Realtime snapping must use the same zoomed time coordinates as drawing.");
            RecordingCanvas notes;
            state.Mode().DrawChart(notes, state, TestCatalog(finger_drum::texts::Language::English));
            Check(!notes.imageRectangles.empty() && std::abs(notes.imageRectangles.front().x + 36 - x) < .1F &&
                  notes.imageRectangles.front().width == 72,
                  "Zoom must scale note spacing without scaling the selected skin head.");
            const auto revision = state.Document().Revision();
            state.SetRealtimeZoom(1);
            Check(state.Document().Revision() == revision && state.Document().Notes().front().timing.count() == 500'000,
                  "Zoom must not edit source beats or compiled note times.");
            state.SeekFirstNote();
            Check(state.TimeMilliseconds() == 500, "First Z must seek to the first note.");
            state.SeekFirstNote();
            Check(state.TimeMilliseconds() == 0, "Second Z must seek to zero.");
            state.SeekFirstNote();
            Check(state.TimeMilliseconds() == 500, "Subsequent Z must return to the first note.");
            state.Seek(100);
            state.SeekFirstNote();
            Check(state.TimeMilliseconds() == 500, "A manual seek must reset the consecutive-Z toggle.");
            bool rejected = false;
            try { state.SetRealtimeZoom(std::numeric_limits<double>::infinity()); }
            catch (const std::invalid_argument &) { rejected = true; }
            Check(rejected && state.Score().realtimeZoom == 1, "Invalid zoom must preserve the current scale.");
        }
    }

    void TestLastNoteSeeking()
    {
        chart::PatternDocument pattern;
        pattern.baseBpm = 120;
        pattern.notes = {{{0, {1, 8}}, 1, 0}, {{0, {1, 4}}, 11, 1}, {{1, {1, 2}}, 11, 2}};
        EditorWorkspace state(CreateEditorMode("Taiko"), std::make_unique<chart::ChartEditor>(pattern));
        const auto revision = state.Document().Revision();
        state.SeekLastNote();
        Check(state.TimeMilliseconds() == 3000, "First X must include the last long-note tail.");
        bool unavailable = false;
        try
        {
            state.SeekLastNote();
        }
        catch (const std::runtime_error &)
        {
            unavailable = true;
        }
        Check(unavailable && state.TimeMilliseconds() == 3000,
              "Unavailable music duration must preserve the cursor and allow retrying the second X.");
        state.SeekFirstNote();
        Check(state.TimeMilliseconds() == 250, "Z must reset X and seek the first note.");
        state.SeekLastNote();
        Check(state.TimeMilliseconds() == 3000, "Changing from Z to X must restart at the last note.");
        state.SeekFirstNote();
        Check(state.TimeMilliseconds() == 250, "Changing from X to Z must restart at the first note.");
        state.Seek(100);
        state.SeekLastNote();
        Check(state.TimeMilliseconds() == 3000 && state.Document().Revision() == revision,
              "Manual seeking must reset X without editing the score.");
        EditorWorkspace empty(CreateEditorMode("Taiko"),
                              std::make_unique<chart::ChartEditor>(chart::PatternDocument{}));
        empty.SeekLastNote();
        Check(empty.TimeMilliseconds() == 0, "An empty chart must have a zero last-note destination.");
    }

    void TestPreviewMatchesPlay()
    {
        namespace rhythm = finger_drum::rhythm;
        namespace mode = finger_drum::mode;
        const auto parsed = chart::ChartParser{}.ParsePattern("Version: 1\nBase BPM: 180\nMode: Taiko\n[Pattern]\n"
                                                              "0/1,17,1,,Action = Kat,TickDivision = 64\n1/2,17,2\n");
        Check(parsed.Succeeded(), "Spaced Buzz regression chart failed to parse.");
        chart::ChartEditor document(parsed.document);
        auto editorMode = CreateEditorMode("Taiko");
        const auto markers = editorMode->AudioMarkers(document);
        auto play = mode::TaikoMode{}.CreateSession(document.Pattern());
        Check(play.Succeeded(), "Spaced options failed to load in play.");
        const auto hit = play.session->ProcessInput('D', rhythm::InputEdge::Pressed, {});
        const auto *presentation = play.session->FindNotePresentation(1);
        Check(markers.size() == 33 && markers.size() == presentation->tickTimes.size() + 1 &&
                  markers.front().sound == hit.audioCues.front().sound,
              "Editor/play must share spaced/case-insensitive Action and TickDivision interpretation.");
        for (std::size_t i = 1; i < markers.size(); ++i)
            Check(markers[i].seconds == presentation->tickTimes[i - 1].count() / 1e6 &&
                      markers[i].sound == mode::taiko_sound::KatHit,
                  "Preview Buzz ticks diverged from play times/action.");
        auto purple = chart::PatternDocument{};
        purple.mode = "Taiko";
        purple.notes.push_back({{0, {}}, 5, 0});
        chart::ChartEditor purpleDocument(purple);
        const auto purpleMarkers = editorMode->AudioMarkers(purpleDocument);
        auto purplePlay = mode::TaikoMode{}.CreateSession(purple);
        Check(purplePlay.Succeeded(), "Explicit Taiko Purple fixture failed to load.");
        const auto don = purplePlay.session->ProcessInput('F', rhythm::InputEdge::Pressed, {});
        const auto kat = purplePlay.session->ProcessInput('D', rhythm::InputEdge::Pressed, {});
        Check(purpleMarkers.size() == 2 && purpleMarkers[0].sound == don.audioCues.front().sound &&
                  purpleMarkers[1].sound == kat.audioCues.front().sound,
              "Purple preview must include both play sounds.");

        // Explicit table assignments beat timed defaults; timed overrides are
        // resolved independently for the two Purple input sounds.
        chart::EffectDocument effects;
        effects.hitSounds = {{"own", "own.wav"}, {"don", "don.wav"}, {"kat", "kat.wav"}};
        effects.hitSoundChanges = {{{0, {}}, "don", 1}, {{0, {}}, "kat", 2}};
        purpleDocument.Replace(purple, effects);
        const auto changed = editorMode->AudioMarkers(purpleDocument);
        auto changedPlay = mode::TaikoMode{}.CreateSession(purple, effects);
        Check(changedPlay.Succeeded(), "Explicit Taiko sound override fixture failed to load.");
        const auto changedDon = changedPlay.session->ProcessInput('F', rhythm::InputEdge::Pressed, {});
        const auto changedKat = changedPlay.session->ProcessInput('D', rhythm::InputEdge::Pressed, {});
        Check(changed[0].sound == changedDon.audioCues.front().sound &&
                  changed[1].sound == changedKat.audioCues.front().sound,
              "Timed sound overrides diverged between preview/play.");
        purple.notes.front().hitSound = "own";
        purpleDocument.Replace(purple, effects);
        const auto assigned = editorMode->AudioMarkers(purpleDocument);
        Check(assigned[0].sound == "Chart.HitSound.own" && assigned[1].sound == assigned[0].sound,
              "Explicit hit sound must override both default Purple sounds.");

        auto invalid = parsed.document;
        invalid.notes.front().extraData = {"Action = Kat", "TickDivision = 1025"};
        document.Replace(invalid, {});
        bool rejected = false;
        try
        {
            static_cast<void>(editorMode->AudioMarkers(document));
        }
        catch (const std::invalid_argument &)
        {
            rejected = true;
        }
        Check(rejected && !mode::TaikoMode{}.CreateSession(invalid).Succeeded(),
              "Editor/play must reject the same out-of-range TickDivision.");
    }

    void TestSeekAndEffectEdits()
    {
        EditorWorkspace state(CreateEditorMode("Taiko"),
                              std::make_unique<chart::ChartEditor>(chart::PatternDocument{}));
        state.FinishBuild();
        state.SetDivision(8);
        Check(state.Score().division == 8 && state.NeedsRebuild(), "Grid setting must request its own rebuild.");
        state.FinishBuild();
        bool badDivision = false;
        try
        {
            state.SetDivision(1025);
        }
        catch (const std::invalid_argument &)
        {
            badDivision = true;
        }
        Check(badDivision && state.Score().division == 8 && !state.NeedsRebuild(),
              "Rejected grid settings must not change state or request a rebuild.");
        state.SetRealtime(true);
        Check(state.Score().realtime && state.NeedsRebuild(), "View mode changes must request a rebuild.");
        state.Seek(-1000);
        for (const double invalid :
             {1e12, 1e100, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()})
        {
            bool rejected = false;
            try
            {
                state.Seek(invalid);
            }
            catch (const std::invalid_argument &)
            {
                rejected = true;
            }
            Check(rejected && state.TimeMilliseconds() == -1000, "Invalid seek must preserve the previous time.");
        }
        state.effectForm.selection = chart::EffectCommandType::ScrollSpeed;
        state.effectForm.beginValue = "2";
        state.effectForm.endMeasure = "2";
        state.effectForm.endFraction = "0/4";
        state.effectForm.endValue = "3";
        state.effectForm.curve = chart::AutomationCurve::Linear;
        state.ApplyEffect();
        state.effectForm.Load(state.Document().Effects(), 0);
        Check(state.effectForm.endMeasure == "2" && state.effectForm.endFraction == "0/1" &&
                  state.effectForm.curve == chart::AutomationCurve::Linear,
              "Named effect fields lost the region/curve during row selection.");
        const auto revision = state.Document().Revision();
        state.effectForm.beginValue = "0";
        bool rejected = false;
        try
        {
            state.ApplyEffect();
        }
        catch (const std::invalid_argument &)
        {
            rejected = true;
        }
        Check(rejected && state.Document().Revision() == revision &&
                  state.Document().Effects().commands.front().beginValue == 2,
              "Invalid effect draft must leave the document unchanged.");
        state.RemoveEffect(0);
        Check(state.Document().Effects().commands.empty(), "Effect row deletion failed.");
        state.timingForm.bpm = "180";
        state.ApplyBpm();
        Check(state.Document().Pattern().timing.front().value == 180, "Timing command failed.");
        state.RemoveTiming(0);
        Check(state.Document().Pattern().timing.empty(), "Timing deletion failed.");
    }

    void TestInspectorMusicalTiming()
    {
        EditorWorkspace state(CreateEditorMode("Taiko"),
                              std::make_unique<chart::ChartEditor>(chart::PatternDocument{}));
        state.AddNote({0, {}}, 12, chart::MusicalPosition{0, {1, 4}});
        const auto id = state.Document().Notes().front().note.sourceOrder;
        TaikoEditorNoteInspector inspector;
        inspector.Select(state.Document(), id);
        inspector.Form().tool = 1;
        const auto initialRevision = state.Document().Revision();
        bool groupRejected = false;
        try
        {
            inspector.Apply(state);
        }
        catch (const std::exception &)
        {
            groupRejected = true;
        }
        Check(groupRejected && state.Document().Revision() == initialRevision && state.Document().Notes().size() == 2,
              "Changing to another tool group must preserve the source note and its paired end.");
        inspector.Form().tool = 14;
        inspector.Form().startMeasure = "2";
        inspector.Form().startBeat = "1/8";
        inspector.Form().endMeasure = "3";
        inspector.Form().endBeat = "1/4";
        inspector.Apply(state);
        const auto selected = *state.Document().FindNote(id);
        Check(selected.head.note.position == chart::MusicalPosition{1, {1, 8}} && selected.tail &&
                  selected.tail->note.position == chart::MusicalPosition{2, {1, 4}} &&
                  selected.head.note.keyType == 14 && selected.tail->note.keyType == 14 &&
                  selected.head.timing == finger_drum::rhythm::RhythmTime{2'250'000} &&
                  selected.tail->timing == finger_drum::rhythm::RhythmTime{4'500'000},
              "Musical input must use one-based measures, exact whole-note fractions and preserve the paired end.");
        for (const auto language : {finger_drum::texts::Language::Korean, finger_drum::texts::Language::English})
        {
            RecordingCanvas panel;
            inspector.Draw(panel, state, TestCatalog(language));
            const auto &text = finger_drum::texts::TaikoEditor(TestCatalog(language));
            Check(std::ranges::find(panel.labels, std::wstring(text.beatPosition)) != panel.labels.end() &&
                      std::ranges::find(panel.labels, L"2250.000 ms") != panel.labels.end() &&
                      std::ranges::none_of(panel.buttonLabels,
                                           [](const auto &label) { return label.find(L"ms") != std::wstring::npos; }),
                  "The inspector must localize musical fields and leave derived milliseconds read-only.");
        }
        // BPM edits recompile time without changing the selected musical source or its IDs.
        state.timingForm.measure = "2";
        state.timingForm.fraction = "0/1";
        state.timingForm.bpm = "180";
        state.ApplyBpm();
        RecordingCanvas refreshed;
        inspector.Draw(refreshed, state, TestCatalog(finger_drum::texts::Language::English));
        Check(inspector.Form().startMeasure == "2" && inspector.Form().startBeat == "1/8" &&
                  state.Document().FindNote(id)->tail->note.sourceOrder == selected.tail->note.sourceOrder &&
                  state.Document().FindNote(id)->head.timing == finger_drum::rhythm::RhythmTime{2'166'667},
              "Timeline changes must refresh derived times while retaining source coordinates and paired IDs.");
        const auto revision = state.Document().Revision();
        for (int invalid = 0; invalid < 5; ++invalid)
        {
            inspector.Select(state.Document(), id);
            auto &draft = inspector.Form();
            if (invalid == 0)
                draft.startMeasure = "0";
            if (invalid == 1)
                draft.startMeasure = "100001";
            if (invalid == 2)
                draft.startBeat = "1/1";
            if (invalid == 3)
                draft.startBeat = "250ms";
            if (invalid == 4)
            {
                draft.endMeasure = draft.startMeasure;
                draft.endBeat = draft.startBeat;
            }
            bool rejected = false;
            try
            {
                inspector.Apply(state);
            }
            catch (const std::exception &)
            {
                rejected = true;
            }
            Check(rejected && state.Document().Revision() == revision &&
                      state.Document().FindNote(id)->head.note.position == selected.head.note.position &&
                      state.Document().FindNote(id)->tail->note.position == selected.tail->note.position,
                  "Invalid musical fields must preserve both endpoints and all document caches.");
        }
        inspector.Select(state.Document(), id);
        inspector.Form().tool = 12;
        inspector.Form().startMeasure = "4";
        inspector.Form().startBeat = "3/8";
        inspector.Form().endMeasure = "5";
        inspector.Apply(state);
        Check(state.Document().Notes().size() == 2 && state.Document().FindNote(id)->tail &&
                  state.Document().FindNote(id)->head.note.position == chart::MusicalPosition{3, {3, 8}} &&
                  state.Document().FindNote(id)->tail->note.sourceOrder == selected.tail->note.sourceOrder &&
                  state.Document().NotesInMeasures(1, 3).empty() && state.Document().NotesInMeasures(3, 5).size() == 2,
              "Changing variants must retain the paired end IDs and update both musical positions in the index.");
    }

    void TestCustomSeparateRealtimeVisibility()
    {
        auto pattern = chart::PatternDocument{};
        pattern.baseBpm = 120;
        pattern.notes.push_back({{4, {}}, 1});
        const auto parsed = chart::ChartParser{}.ParseEffect(R"([Interpolation]
Dip: 1-3*u*(1-u)
[Speed]
1,0/1,Area,9,0/1,#ScrollSpeed Separate,From=1,To=1,Curve=Dip
)");
        Check(parsed.Succeeded(), "Custom Separate regression fixture must parse.");
        EditorWorkspace state(CreateEditorMode("Taiko"),
                              std::make_unique<chart::ChartEditor>(pattern, parsed.document));
        state.SetRealtime(true);
        state.SelectTool(1);
        RecordingCanvas canvas;
        state.Mode().DrawChart(canvas, state, TestCatalog(finger_drum::texts::Language::Korean));
        const auto drawn = std::ranges::any_of(
            canvas.rectangles, [](const auto &rect)
            { return std::abs(rect.x + rect.width / 2 - 1209.2F) < .01F && rect.width == 72 && rect.height == 72; });
        Check(drawn, "The custom curve's slow 8s note must be drawn inside the realtime lane.");
        state.Mode().EditScore(state, {1209.2F, 465}, true);
        Check(state.Document().Notes().empty(), "The visible slow note must participate in deletion hit testing.");
        // With no authored slow note left, the endpoint bound becomes 1.
        // The same custom curve's visible grid still needs to be selectable.
        canvas.rectangles.clear();
        state.Mode().DrawChart(canvas, state, TestCatalog(finger_drum::texts::Language::Korean));
        state.Mode().EditScore(state, {1209.2F, 465}, false);
        Check(state.Document().Notes().size() == 1 &&
                  state.Document().Notes().front().note.position == chart::MusicalPosition{4, {}},
              "A custom Separate grid must remain visible/snappable without an authored minimum-speed note.");
    }
    void TestTaikoToolGroups(finger_drum::texts::TextCatalog &texts)
    {
        constexpr std::array keys{'W', 'E', 'R', 'T', 'Y', 'U'};
        const std::array<std::vector<int>, 6> groups{{{1, 2}, {3, 4, 5}, {11, 13}, {12, 14}, {15, 16}, {17, 18}}};
        for (std::size_t group = 0; group < groups.size(); ++group)
        {
            EditorWorkspace state(CreateEditorMode("Taiko"),
                                  std::make_unique<chart::ChartEditor>(chart::PatternDocument{}));
            state.SelectTool(0);
            for (std::size_t variant = 0; variant <= groups[group].size(); ++variant)
            {
                Check(static_cast<TaikoEditorMode &>(state.Mode())
                          .HandleToolKey(state, static_cast<std::uint16_t>(keys[group])),
                      "A grouped hotkey must be handled by the mode.");
                state.PlaceNote({static_cast<std::int64_t>(variant), {}});
                const int expected = groups[group][variant % groups[group].size()];
                if (expected >= 11)
                    state.PlaceNote({static_cast<std::int64_t>(variant), {1, 4}});
                const auto notes = state.Document().NotesInMeasures(static_cast<std::int64_t>(variant),
                                                                    static_cast<std::int64_t>(variant + 1));
                Check(notes.front().note.keyType == (expected == 18 ? 17 : expected),
                      "Group cycling selected the wrong persisted note type.");
                if (expected == 18)
                    Check(notes.front().note.extraData[0] == "Action=Kat", "Kat Buzz cycle lost its action.");
            }
            state.SelectTool(groups[group].front());
            RecordingCanvas palette;
            state.Mode().DrawTools(palette, state, texts);
            palette.actions[1 + group]();
            const auto measure = static_cast<std::int64_t>(groups[group].size() + 1);
            state.PlaceNote({measure, {}});
            const auto expected = groups[group][1];
            if (expected >= 11)
                state.PlaceNote({measure, {1, 4}});
            const auto head = state.Document().NotesInMeasures(measure, measure + 1).front().note;
            Check(head.keyType == (expected == 18 ? 17 : expected),
                  "Pressing an active tool button must cycle its group.");

            // The inspector must share precisely the same six categories as the palette.
            TaikoEditorNoteInspector inspector;
            inspector.Select(state.Document(), head.sourceOrder);
            for (std::size_t variant = 0; variant < groups[group].size(); ++variant)
            {
                RecordingCanvas panel;
                inspector.Draw(panel, state, texts);
                panel.actions.front()();
                const int next = groups[group][(variant + 2) % groups[group].size()];
                Check(inspector.Form().tool == next, "The inspector cycled outside its note category.");
                inspector.Apply(state);
                Check(state.Document().FindNote(head.sourceOrder)->head.note.keyType == (next == 18 ? 17 : next),
                      "The inspector must apply each variant within its category.");
            }
            const auto revision = state.Document().Revision();
            inspector.Form().tool = groups[(group + 1) % groups.size()].front();
            bool rejected = false;
            try { inspector.Apply(state); }
            catch (const std::invalid_argument &) { rejected = true; }
            Check(rejected && state.Document().Revision() == revision,
                  "Cross-category inspector drafts must not modify the chart.");

            state.SelectTool(11);
            state.PlaceNote({100, {}});
            Check(state.Mode().HasPendingPlacement(), "Q test requires an unfinished long note.");
            Check(static_cast<TaikoEditorMode &>(state.Mode()).HandleToolKey(state, 'Q') &&
                      !state.Mode().HasPendingPlacement(),
                  "Q must activate selection and cancel an unfinished placement.");
            const auto notesBefore = state.Document().Notes().size();
            state.PlaceNote({100, {}});
            Check(state.Document().Notes().size() == notesBefore && !state.Mode().HasPendingPlacement(),
                  "Q selection must not place another note or start another long note.");
        }
    }
} // namespace

void TestEditorModes(v::ScreenVisual2DManager &visuals, const mrg::EngineServices &services,
                     finger_drum::texts::TextCatalog &texts,
                     const std::function<const std::vector<v::DrawPacket> &()> &packets)
{
    {
        TestTaikoToolGroups(texts);
        EditorWorkspace state(CreateEditorMode("Taiko"),
                              std::make_unique<chart::ChartEditor>(chart::PatternDocument{}));
        state.AddNote({0, {}}, 15, chart::MusicalPosition{0, {1, 2}}, {"HitCount=4", "Custom=keep"});
        const auto id = state.Document().Notes().front().note.sourceOrder;
        TaikoEditorNoteInspector inspector;
        inspector.Select(state.Document(), id + 1);
        inspector.Form().tool = 16;
        inspector.Form().hitCount = "6";
        inspector.Apply(state);
        Check(state.Document().FindNote(id)->head.note.keyType == 16 &&
                  state.Document().FindNote(id)->tail->note.keyType == 16 &&
                  state.Document().FindNote(id)->head.note.extraData ==
                      std::vector<std::string>{"Custom=keep", "HitCount=6"},
              "The inspector must update the paired type/count and preserve unrelated options.");
        state.SelectTool(0);
        RecordingCanvas score;
        state.Mode().DrawChart(score, state, TestCatalog(finger_drum::texts::Language::Korean));
        state.Mode().EditScore(state, {205, 174}, false);
        score.labels.clear();
        state.Mode().DrawChart(score, state, TestCatalog(finger_drum::texts::Language::Korean));
        Check(std::ranges::find(
                  score.labels,
                  std::wstring(
                      finger_drum::texts::TaikoEditor(TestCatalog(finger_drum::texts::Language::Korean)).noteInfo)) !=
                  score.labels.end(),
              "Selecting a note must expose the note inspector.");
        Check(std::ranges::any_of(score.images, [](const auto &path) { return path.filename() == L"DengDeng.png"; }),
              "Score must draw the focus-note skin sprite.");
        state.SetRealtime(true);
        state.Mode().DrawChart(score, state, TestCatalog(finger_drum::texts::Language::Korean));
        Check(std::ranges::any_of(score.labels,
                                  [](const auto &label) { return label.find(L"6 연타") != std::wstring::npos; }),
              "A long-note head must show its required hit count in both views.");
        inspector.Form().hitCount = "0";
        const auto revision = state.Document().Revision();
        bool rejected = false;
        try
        {
            inspector.Apply(state);
        }
        catch (const std::invalid_argument &)
        {
            rejected = true;
        }
        Check(rejected && state.Document().Revision() == revision,
              "Invalid inspector edits must preserve the document.");
    }
    {
        chart::PatternDocument pattern;
        pattern.notes = {{{0, {}}, 11, 1, {}, {"HitCount=6"}}, {{40, {}}, 11, 2}};
        EditorWorkspace state(CreateEditorMode("Taiko"), std::make_unique<chart::ChartEditor>(pattern));
        state.SelectTool(0);
        state.SetFirstMeasure(4);
        RecordingCanvas score;
        state.Mode().DrawChart(score, state, TestCatalog(finger_drum::texts::Language::English));
        state.Mode().EditScore(state, {800, 174}, false);
        score.labels.clear();
        state.Mode().DrawChart(score, state, TestCatalog(finger_drum::texts::Language::English));
        Check(std::ranges::find(score.labels, L"6") != score.labels.end() && state.TimeMilliseconds() == 0,
              "A long body spanning the viewport must select its source without moving the cursor.");
        state.SetRealtime(true);
        state.Seek(8000);
        state.Mode().DrawChart(score, state, TestCatalog(finger_drum::texts::Language::English));
        state.Mode().EditScore(state, {800, 465}, false);
        score.labels.clear();
        state.Mode().DrawChart(score, state, TestCatalog(finger_drum::texts::Language::English));
        Check(std::ranges::find(score.labels, L"6") != score.labels.end() && state.TimeMilliseconds() == 8000,
              "Realtime body selection must preserve the audio cursor and resolve the head.");
    }
    TestTaikoEditing();
    TestTaikoPlacementConflicts();
    TestPreviewMatchesPlay();
    TestRealtimeZoomAndTransport();
    TestLastNoteSeeking();
    TestDelayViewsAndSnapping();
    TestSeekAndEffectEdits();
    TestInspectorMusicalTiming();
    TestCustomSeparateRealtimeVisibility();
    auto mode = std::make_unique<ProbeMode>();
    auto *probe = mode.get();
    EditorWorkspace state(finger_drum::GameplayLaunchRequest{}, std::move(mode));
    state.Initialize();
    state.UpdateAnalysis();
    Check(!state.Analysis().Running(), "A mode without audio sources must not start the worker.");
    RecordingCanvas palette;
    state.Mode().DrawTools(palette, state, texts);
    palette.actions.front()();
    state.Mode().EditScore(state, {625, 280}, false);
    Check(state.Document().Notes().front().note.keyType == 70 &&
              state.Document().Notes().front().note.position.measure == 2,
          "Alternative mode tools/vertical coordinates did not reach the document adapter.");
    state.Save();
    Check(static_cast<const ProbeDocument &>(state.Document()).saved, "Save bypassed the selected format adapter.");
    state.Analysis().CacheAudioMarkers(state.Document(), state.Mode());
    state.Analysis().CacheAudioMarkers(state.Document(), state.Mode());
    Check(probe->markerCalls == 1 && state.Analysis().Markers().front().sound == "ProbeCue",
          "Common analysis must use the mode marker policy and cache the document revision.");
    state.PlaceNote({3, {}});
    state.Analysis().CacheAudioMarkers(state.Document(), state.Mode());
    Check(probe->markerCalls == 2, "An edit must invalidate the mode marker cache.");
    EditorView view(visuals, state, texts);
    view.Initialize(services);
    const auto hasText = [&](std::wstring_view text)
    { return std::ranges::any_of(packets(), [text](const auto &p) { return p.text && p.text->value == text; }); };
    visuals.Render({});
    Check(hasText(L"PROBE TOOL") && hasText(L"PROBE CHART") && !hasText(L"DON"),
          "Common view rendered Taiko instead of the selected mode.");
    state.SelectTab(EditorTab::Metadata);
    view.Build();
    visuals.Render({});
    Check(hasText(L"PROBE FORMAT METADATA") && !hasText(L"YMM RELATIVE PATH"),
          "Metadata view is still tied to YMP/YMM.");
    state.SelectTab(EditorTab::Effects);
    state.effectForm.selection = EditorHitSoundTarget{42};
    view.Build();
    visuals.Render({});
    Check(hasText(L"PROBE SOUND"), "Effects view still assumes exactly Don/Kat sound choices.");
    state.SelectTab(EditorTab::Pattern);
    probe->failDrawing = true;
    const auto oldCalls = probe->chartCalls;
    mrg::platform::InputState input;
    const mrg::UpdateContext update{0, 0, 1, input, services.audio, {}};
    Check(!view.Update(update), "Drawing error must not leave the editor.");
    visuals.Render({});
    Check(state.Status() == "Injected chart drawing failure." && hasText(L"Injected chart drawing failure.") &&
              !state.NeedsRebuild() && probe->chartCalls == oldCalls + 1,
          "Drawing failure must render its status and stop automatic retry.");
    static_cast<void>(view.Update(update));
    Check(probe->chartCalls == oldCalls + 1, "A failed build retried without a new action.");
    probe->failDrawing = false;
    state.SelectTab(EditorTab::Metadata);
    static_cast<void>(view.Update(update));
    visuals.Render({});
    Check(hasText(L"PROBE FORMAT METADATA"), "Navigation must recover from a drawing failure.");
    view.Shutdown();
    Check(visuals.CanvasCount() == 0, "Mode replacement retained the previous editor Canvas.");
}
