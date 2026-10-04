#include "EditorModeTests.h"
#include "Editing/ChartEditor.h"
#include "EditorScene/Submodules/EditorView.h"
#include "EditorScene/Submodules/Modes/EditorModeFactory.h"
#include "Taiko/TaikoMode.h"
#include <limits>
#include <stdexcept>

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
        void Box(v::Rect r, v::Color, float) override
        {
            rectangles.push_back(r);
        }
        void Text(v::Rect, std::wstring, float, v::Color) override
        {
        }
        void Button(v::Rect, std::wstring, std::function<void()> action, bool) override
        {
            actions.push_back(std::move(action));
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
        void DrawTools(IEditorModeCanvas &canvas, IEditorContext &state, finger_drum::texts::Language) override
        {
            canvas.Button({30, 130, 60, 40}, L"PROBE TOOL", [&state] { state.SelectTool(70); });
        }
        void DrawChart(IEditorModeCanvas &canvas, IEditorContext &) override
        {
            ++chartCalls;
            if (failDrawing)
                throw std::runtime_error("Injected chart drawing failure.");
            // Vertical rectangular lanes instead of Taiko circles/horizontal lane.
            canvas.Box({600, 200, 40, 500}, {.2F, .8F, .4F, 1});
            canvas.Box({650, 200, 40, 500}, {.2F, .4F, .8F, 1});
            canvas.Text({600, 160, 200, 30}, L"PROBE CHART");
        }
        void DrawToolMenu(IEditorModeCanvas &, IEditorContext &, finger_drum::texts::Language) override
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
        void DrawMetadata(IEditorModeCanvas &canvas, IEditorContext &, finger_drum::texts::Language) override
        {
            canvas.Text({600, 200, 300, 30}, L"PROBE FORMAT METADATA");
        }
        std::vector<EditorSoundChoice> SoundChoices(finger_drum::texts::Language) const override
        {
            return {{42, L"Probe key", L"PROBE SOUND"}};
        }
        std::wstring_view SoundEffectHelp(finger_drum::texts::Language) const override
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
        Check(CreateEditorMode("")->Id() == "Taiko", "Legacy empty mode must select Taiko.");
        bool rejected = false;
        try
        {
            static_cast<void>(CreateEditorMode("unsupported"));
        }
        catch (const std::invalid_argument &)
        {
            rejected = true;
        }
        Check(rejected, "Unknown modes must not silently use Taiko tools.");
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
            Check(markers.size() == (hasTicks ? 4 : id == 5 ? 2 : 1), "Taiko expected-hit count changed.");
            Check(markers.front().sound == (id == 2 || id == 18 ? "Taiko.Kat.Hit"
                                            : id == 4           ? "Taiko.BigKat.FirstHit"
                                            : id == 3           ? "Taiko.BigDon.FirstHit"
                                                                : "Taiko.Don.Hit"),
                  "Taiko hit-sound mapping changed.");
        }
        EditorWorkspace state(CreateEditorMode("Taiko"),
                              std::make_unique<chart::ChartEditor>(chart::PatternDocument{}));
        RecordingCanvas canvas;
        state.Mode().DrawChart(canvas, state);
        state.Mode().EditScore(state, {305.25F, 174}, false);
        Check(state.Document().Notes().front().note.position.fraction == chart::Rational{1, 4},
              "Score click no longer snaps using quarter-note division.");
        state.Mode().DrawChart(canvas, state);
        state.Mode().EditScore(state, {305.25F, 174}, true);
        Check(state.Document().Notes().empty(), "Mode-owned note hit testing did not delete the note.");
        state.SetRealtime(true);
        state.SelectTool(1);
        state.Mode().DrawChart(canvas, state);
        for (const v::Point outside : {v::Point{120, 465}, {1849, 465}, {230, 350}, {230, 580}})
            state.Mode().EditScore(state, outside, false);
        Check(state.Document().Notes().empty(), "Clicks outside the realtime lane must not place notes.");
        state.Mode().EditScore(state, {230, 465}, false);
        Check(state.Document().Notes().size() == 1, "A realtime lane click must still place a snapped note.");
        state.Mode().DrawChart(canvas, state);
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
        state.Mode().DrawTools(canvas, state, finger_drum::texts::Language::English);
        canvas.actions.front()();
        state.PlaceNote({0, {}});
        Check(state.Document().Notes().empty(), "Selection tool must not place a note.");
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
        Check(markers.size() == 32 && markers.size() == presentation->tickTimes.size() + 1 &&
                  markers.front().sound == hit.audioCues.front().sound,
              "Editor/play must share spaced/case-insensitive Action and TickDivision interpretation.");
        for (std::size_t i = 1; i < markers.size(); ++i)
            Check(markers[i].seconds == presentation->tickTimes[i - 1].count() / 1e6 &&
                      markers[i].sound == mode::taiko_sound::KatHit,
                  "Preview Buzz ticks diverged from play times/action.");
        auto purple = chart::PatternDocument{};
        purple.notes.push_back({{0, {}}, 5, 0});
        chart::ChartEditor purpleDocument(purple);
        const auto purpleMarkers = editorMode->AudioMarkers(purpleDocument);
        auto purplePlay = mode::TaikoMode{}.CreateSession(purple);
        const auto don = purplePlay.session->ProcessInput('F', rhythm::InputEdge::Pressed, {});
        const auto kat = purplePlay.session->ProcessInput('D', rhythm::InputEdge::Pressed, {});
        Check(purpleMarkers.size() == 2 && purpleMarkers[0].sound == don.audioCues.front().sound &&
                  purpleMarkers[1].sound == kat.audioCues.front().sound,
              "Purple preview must include both play sounds.");

        // Explicit table assignments beat timed defaults; timed overrides are
        // resolved independently for the two Purple input sounds.
        purple.hitSounds = {{"own", "own.wav"}, {"don", "don.wav"}, {"kat", "kat.wav"}};
        chart::EffectDocument effects;
        effects.hitSoundChanges = {{{0, {}}, "don", 1}, {{0, {}}, "kat", 2}};
        purpleDocument.Replace(purple, effects);
        const auto changed = editorMode->AudioMarkers(purpleDocument);
        auto changedPlay = mode::TaikoMode{}.CreateSession(purple, effects);
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
        state.effectForm.curve = chart::AutomationCurve::Smoothstep;
        state.ApplyEffect();
        state.effectForm.Load(state.Document().Effects(), 0);
        Check(state.effectForm.endMeasure == "2" && state.effectForm.endFraction == "0/1" &&
                  state.effectForm.curve == chart::AutomationCurve::Smoothstep,
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
} // namespace

void TestEditorModes(v::ScreenVisual2DManager &visuals, const mrg::EngineServices &services,
                     finger_drum::texts::TextCatalog &texts,
                     const std::function<const std::vector<v::DrawPacket> &()> &packets)
{
    TestTaikoEditing();
    TestPreviewMatchesPlay();
    TestSeekAndEffectEdits();
    auto mode = std::make_unique<ProbeMode>();
    auto *probe = mode.get();
    EditorWorkspace state(finger_drum::GameplayLaunchRequest{}, std::move(mode));
    state.Initialize();
    state.UpdateAnalysis();
    Check(!state.Analysis().Running(), "A mode without audio sources must not start the worker.");
    RecordingCanvas palette;
    state.Mode().DrawTools(palette, state, texts.CurrentLanguage());
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
    const auto hasText = [&](std::wstring_view text) {
        return std::ranges::any_of(packets(), [text](const auto &p) { return p.text == text; });
    };
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
