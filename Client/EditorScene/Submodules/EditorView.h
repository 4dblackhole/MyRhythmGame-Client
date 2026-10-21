#pragma once
#include "EditorSupport.h"
#include "EditorMenuBar.h"
#include "EditorVisual.h"
#include "EditorWorkspace.h"
#include "Texts/EditorScene/EditorTexts.h"
#include <array>
#include <functional>
#include <optional>
#include <vector>
#include <tuple>

class EditorView final : public IEditorModeCanvas
{
  public:
    EditorView(ScreenVisual2DManager &manager, EditorWorkspace &state,
               TextCatalog &texts)
        : visuals(manager), state_(state), texts_(texts)
    {
    }
    void Initialize(const EngineServices &services);
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
        playbackRateSliderNode_ = nullptr;
        audioSelectionNodes_.fill(nullptr);
        audioSelectionAnchor_.reset();
        visual = nullptr;
    }
    void Build();
    void ReloadSize();
    bool Update(const UpdateContext &context);

  private:
    void BuildContent();
    void SyncInputControls();
    void DrawNavigation();
    void DrawFooter();
    void DrawTimeInput(Rect);
    void DrawEffectRows();
    void DrawEffectFields();
    std::wstring EffectLabel(const EditorEffectSelection &) const;
    void SelectEffectRow(std::size_t);
    void CycleEffectType();
    void DrawScore();
    void DrawTools();
    void UpdateSliders(const UpdateContext &context);
    EditorMenuInputResult UpdateMenu(const InputState &input);
    bool UpdateKeyboard(const InputState &);
    void UpdatePointer(const UpdateContext &);
    void SyncDivisionSlider() noexcept;
    void DrawTiming();
    void DrawEffects();
    void DrawAudio();
    void UpdateAudioTiles(double begin, double minimumHz, double maximumHz, double longestSound);
    void PrepareAudioTrack(std::size_t track, Rect rect, std::int64_t first, std::int64_t last, bool invalidate);
    void UpdateWaveformColumns(double begin, double columnSeconds);
    void DrawAudioWaveform(const std::vector<SpectrumFrame> &columns);
    void DrawAudioTimeRuler(double begin);
    void DrawAudioAlignment(double begin);
    void DrawAudioControls();
    void InitializeAudioSelection();
    void UpdateAudioSelection(Visual2DNode &, const PointerEvent &);
    void DrawAudioSelection(double begin);
    void DrawPlaybackRateControl();
    void DrawAudioSpectrum(Rect rect, double minimumHz, double maximumHz);
    void DrawTimeline();
    void SyncTimelineSlider();
    void Image(Rect, const std::filesystem::path &, Color) override;
    void Box(Rect r, Color color, float radius = 0) override;
    void Text(Rect r, std::wstring text, float size = 20, Color color = editor_ui::Ink) override;
    void Button(Rect r, std::wstring text, std::function<void()> action, bool selected = false) override;
    void Field(Rect r, const wchar_t *label, std::string &value);
    [[nodiscard]] EditorTextSet Texts() const noexcept;
    ScreenVisual2DManager &visuals;
    EditorWorkspace &state_;
    TextCatalog &texts_;
    std::uint64_t textRevision_{};
    ScreenCanvasHandle canvas;
    Visual2DNode *node{};
    Visual2DNode *divisionSliderNode_{};
    Visual2DNode *timelineSliderNode_{};
    Visual2DNode *playbackRateSliderNode_{};
    std::array<Visual2DNode *, 3> audioSelectionNodes_{};
    std::optional<RhythmTime> audioSelectionAnchor_;
    double audioSelectionViewBegin_{}, audioSelectionViewWindow_{};
    Visual2DInputRouter sliderInput_;
    EditorMenuBar menuBar_;
    EditorVisual *visual{};
    bool buildFailed_{};
    bool toggleSelection_{}, rangeSelection_{};
    static constexpr Rect DivisionSliderRect{1240, 6 + EditorMenuBar::Height, 480, 26};
    static constexpr Rect TimelineSliderRect{205, 915, 1645, 44};
    static constexpr Rect PlaybackRateSliderRect{1260, 88, 340, 26};
    static constexpr Rect Waveform{205, 153, 1565, 166};
    static constexpr Rect MusicSpectrum{205, 375, 1565, 212};
    static constexpr Rect HitSpectrum{205, 640, 1565, 212};
    std::vector<Control> controls;
    using AudioColumnsKey = std::tuple<double, double, double, std::uint64_t>;
    std::optional<AudioColumnsKey> audioColumnsKey_;
    const IEditorDocument *audioColumnsDocument_{};
    std::uint64_t audioColumnsDocumentRevision_{};
    std::vector<SpectrumFrame> musicColumns_;
    struct AudioTile
    {
        Visual2DNode *node{};
        std::vector<SpectrumFrame> columns;
    };
    std::array<Visual2DNode *, 2> audioRoots_{};
    std::array<EditorVisual *, 2> audioOverlays_{};
    std::array<std::map<std::int64_t, AudioTile>, 2> audioTiles_;
    std::uint32_t width{1280}, height{720};
};
