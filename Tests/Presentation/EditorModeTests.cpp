#include "EditorModeTests.h"
#include "Editing/ChartEditor.h"
#include "EditorScene/Submodules/EditorView.h"
#include "EditorScene/Submodules/Modes/EditorModeFactory.h"
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
        void PlaceNote(EditorWorkspace &state, chart::MusicalPosition p) override
        {
            state.editor->AddNote(p, selected);
            state.rebuild = true;
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
        void DrawTools(IEditorModeCanvas &canvas, EditorWorkspace &state, finger_drum::texts::Language) override
        {
            canvas.Button({30, 130, 60, 40}, L"PROBE TOOL", [&state] { state.SelectTool(70); });
        }
        void DrawChart(IEditorModeCanvas &canvas, EditorWorkspace &) override
        {
            // Vertical rectangular lanes instead of Taiko circles/horizontal lane.
            canvas.Box({600, 200, 40, 500}, {.2F, .8F, .4F, 1});
            canvas.Box({650, 200, 40, 500}, {.2F, .4F, .8F, 1});
            canvas.Text({600, 160, 200, 30}, L"PROBE CHART");
        }
        void DrawToolMenu(IEditorModeCanvas &, EditorWorkspace &, finger_drum::texts::Language) override
        {
        }
        void EditScore(EditorWorkspace &state, v::Point point, bool erase) override
        {
            if (erase)
                state.editor->DeleteNote(0);
            else
                state.PlaceNote({static_cast<std::int64_t>(point.y / 100), {}});
        }
        void ScrollScore(EditorWorkspace &state, int direction) override
        {
            state.firstMeasure += direction;
        }
        void DrawMetadata(IEditorModeCanvas &canvas, EditorWorkspace &, finger_drum::texts::Language) override
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
        std::map<std::string, std::filesystem::path> AudioFiles(const chart::IEditorDocument &,
                                                                finger_drum::GameplayLaunchRequest &) const override
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
            EditorWorkspace state({});
            state.mode = CreateEditorMode("Taiko");
            state.editor = std::make_unique<chart::ChartEditor>(chart::PatternDocument{});
            state.SelectTool(id);
            state.PlaceNote({0, {}});
            const bool isLong = id >= 11;
            Check(state.mode->HasPendingPlacement() == isLong, "Taiko long-note placement changed.");
            if (isLong)
                state.PlaceNote({0, {1, 4}});
            const auto &notes = state.editor->Notes();
            Check(notes.size() == (isLong ? 2 : 1) && notes.front().note.keyType == (id == 18 ? 17 : id),
                  "Taiko tool persisted the wrong note type.");
            if (id == 18)
                Check(notes.front().note.extraData == std::vector<std::string>{"Action=Kat", "TickDivision=16"},
                      "Kat Buzz options changed.");
            const auto markers = state.mode->AudioMarkers(*state.editor);
            const bool hasTicks = id == 12 || id == 14 || id == 17 || id == 18;
            Check(markers.size() == (hasTicks ? 4 : 1), "Taiko expected-hit count changed.");
            Check(markers.front().sound == (id == 2 || id == 18 ? "Kat"
                                            : id == 4           ? "BigKat"
                                            : id == 3           ? "BigDon"
                                                                : "Don"),
                  "Taiko hit-sound mapping changed.");
        }
        EditorWorkspace state({});
        state.mode = CreateEditorMode("Taiko");
        state.editor = std::make_unique<chart::ChartEditor>(chart::PatternDocument{});
        RecordingCanvas canvas;
        state.mode->DrawChart(canvas, state);
        state.mode->EditScore(state, {305.25F, 174}, false);
        Check(state.editor->Notes().front().note.position.fraction == chart::Rational{1, 4},
              "Score click no longer snaps using quarter-note division.");
        state.mode->DrawChart(canvas, state);
        state.mode->EditScore(state, {305.25F, 174}, true);
        Check(state.editor->Notes().empty(), "Mode-owned note hit testing did not delete the note.");
        state.realtime = true;
        state.timeMs = 123;
        state.SelectTool(11);
        state.PlaceNote({0, {1, 4}});
        Check(state.timeMs == 123 && state.mode->CancelInteraction() && !state.mode->HasPendingPlacement(),
              "Realtime long-note placement must preserve time and support cancellation.");
        state.mode->DrawTools(canvas, state, finger_drum::texts::Language::English);
        canvas.actions.front()();
        state.PlaceNote({0, {}});
        Check(state.editor->Notes().empty(), "Selection tool must not place a note.");
    }
} // namespace

void TestEditorModes(v::ScreenVisual2DManager &visuals, const mrg::EngineServices &services,
                     finger_drum::texts::TextCatalog &texts,
                     const std::function<const std::vector<v::DrawPacket> &()> &packets)
{
    TestTaikoEditing();
    EditorWorkspace state({});
    auto mode = std::make_unique<ProbeMode>();
    auto *probe = mode.get();
    state.mode = std::move(mode);
    state.Initialize();
    state.UpdateAnalysis();
    Check(!state.analysis.Running(), "A mode without audio sources must not start the worker.");
    RecordingCanvas palette;
    state.mode->DrawTools(palette, state, texts.CurrentLanguage());
    palette.actions.front()();
    state.mode->EditScore(state, {625, 280}, false);
    Check(state.editor->Notes().front().note.keyType == 70 && state.editor->Notes().front().note.position.measure == 2,
          "Alternative mode tools/vertical coordinates did not reach the document adapter.");
    state.Save();
    Check(static_cast<ProbeDocument &>(*state.editor).saved, "Save bypassed the selected format adapter.");
    state.analysis.CacheAudioMarkers(*state.editor, *state.mode);
    state.analysis.CacheAudioMarkers(*state.editor, *state.mode);
    Check(probe->markerCalls == 1 && state.analysis.Markers().front().sound == "ProbeCue",
          "Common analysis must use the mode marker policy and cache the document revision.");
    state.PlaceNote({3, {}});
    state.analysis.CacheAudioMarkers(*state.editor, *state.mode);
    Check(probe->markerCalls == 2, "An edit must invalidate the mode marker cache.");
    EditorView view(visuals, state, texts);
    view.Initialize(services);
    const auto hasText = [&](std::wstring_view text) {
        return std::ranges::any_of(packets(), [text](const auto &p) { return p.text == text; });
    };
    visuals.Render({});
    Check(hasText(L"PROBE TOOL") && hasText(L"PROBE CHART") && !hasText(L"DON"),
          "Common view rendered Taiko instead of the selected mode.");
    state.tab = 2;
    view.Build();
    visuals.Render({});
    Check(hasText(L"PROBE FORMAT METADATA") && !hasText(L"YMM RELATIVE PATH"),
          "Metadata view is still tied to YMP/YMM.");
    state.tab = 3;
    state.effectType = 4;
    view.Build();
    visuals.Render({});
    Check(hasText(L"PROBE SOUND"), "Effects view still assumes exactly Don/Kat sound choices.");
    view.Shutdown();
    Check(visuals.CanvasCount() == 0, "Mode replacement retained the previous editor Canvas.");
}
