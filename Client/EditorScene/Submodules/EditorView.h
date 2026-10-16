#pragma once
#include "EditorSupport.h"
#include "EditorMenuBar.h"
#include "EditorVisual.h"
#include "EditorWorkspace.h"
#include "Texts/EditorScene/EditorTexts.h"
#include <functional>
#include <vector>
#include <tuple>

class EditorView final : public IEditorModeCanvas
{
  public:
    EditorView(mrg::visual2d::ScreenVisual2DManager &manager, EditorWorkspace &state,
               finger_drum::texts::TextCatalog &texts)
        : visuals(manager), state_(state), texts_(texts)
    {
    }
    void Initialize(const mrg::EngineServices &services);
    void Resize(std::uint32_t w, std::uint32_t h)
    {
        width = w;
        height = h;
        if (node)
            ReloadSize();
    }
    void Shutdown() noexcept
    {
        controls.clear();
        if (canvas.Get())
            sliderInput_.Reset(*canvas.Get());
        menuBar_.Shutdown();
        for (auto &tiles : audioTiles_)
            tiles.clear();
        audioRoots_.fill(nullptr);
        audioOverlays_.fill(nullptr);
        audioColumnsKey_.reset();
        canvas.Reset();
        node = nullptr;
        divisionSliderNode_ = nullptr;
        timelineSliderNode_ = nullptr;
        visual = nullptr;
    }
    void Build();
    void ReloadSize();
    bool Update(const mrg::UpdateContext &context);

  private:
    void BuildContent();
    void DrawNavigation();
    void DrawFooter();
    void DrawTimeInput(v::Rect);
    void DrawEffectRows();
    void DrawEffectFields();
    std::wstring EffectLabel(const EditorEffectSelection &) const;
    void SelectEffectRow(std::size_t);
    void CycleEffectType();
    void DrawScore();
    void DrawTools();
    void UpdateSliders(const mrg::UpdateContext &context);
    EditorMenuInputResult UpdateMenu(const mrg::platform::InputState &input);
    bool UpdateKeyboard(const mrg::platform::InputState &);
    void UpdatePointer(const mrg::UpdateContext &);
    void SyncDivisionSlider() noexcept;
    void DrawTiming();
    void DrawEffects();
    void DrawAudio();
    void UpdateAudioTiles(double begin, double minimumHz, double maximumHz, double longestSound);
    void PrepareAudioTrack(std::size_t track, v::Rect rect, std::int64_t first, std::int64_t last, bool invalidate);
    void UpdateWaveformColumns(double begin, double columnSeconds);
    void DrawAudioWaveform(const std::vector<finger_drum::editor::SpectrumFrame> &columns);
    void DrawAudioTimeRuler(double begin);
    void DrawAudioSpectrum(v::Rect rect, double minimumHz, double maximumHz);
    void DrawTimeline();
    void SyncTimelineSlider();
    void Image(v::Rect, const std::filesystem::path &, v::Color) override;
    void Box(v::Rect r, v::Color color, float radius = 0) override;
    void Text(v::Rect r, std::wstring text, float size = 20, v::Color color = editor_ui::Ink) override;
    void Button(v::Rect r, std::wstring text, std::function<void()> action, bool selected = false) override;
    void Field(v::Rect r, const wchar_t *label, std::string &value);
    [[nodiscard]] finger_drum::texts::EditorTextSet Texts() const noexcept;
    mrg::visual2d::ScreenVisual2DManager &visuals;
    EditorWorkspace &state_;
    finger_drum::texts::TextCatalog &texts_;
    std::uint64_t textRevision_{};
    mrg::visual2d::ScreenCanvasHandle canvas;
    mrg::visual2d::Visual2DNode *node{};
    mrg::visual2d::Visual2DNode *divisionSliderNode_{};
    mrg::visual2d::Visual2DNode *timelineSliderNode_{};
    mrg::visual2d::Visual2DInputRouter sliderInput_;
    EditorMenuBar menuBar_;
    EditorVisual *visual{};
    bool buildFailed_{};
    static constexpr v::Rect DivisionSliderRect{1240, 6 + EditorMenuBar::Height, 480, 26};
    static constexpr v::Rect TimelineSliderRect{205, 915, 1645, 44};
    std::vector<Control> controls;
    using AudioColumnsKey = std::tuple<double, double, double, std::uint64_t>;
    std::optional<AudioColumnsKey> audioColumnsKey_;
    const finger_drum::chart::IEditorDocument *audioColumnsDocument_{};
    std::uint64_t audioColumnsDocumentRevision_{};
    std::vector<finger_drum::editor::SpectrumFrame> musicColumns_;
    struct AudioTile
    {
        v::Visual2DNode *node{};
        std::vector<finger_drum::editor::SpectrumFrame> columns;
    };
    std::array<v::Visual2DNode *, 2> audioRoots_{};
    std::array<EditorVisual *, 2> audioOverlays_{};
    std::array<std::map<std::int64_t, AudioTile>, 2> audioTiles_;
    std::uint32_t width{1280}, height{720};
};
