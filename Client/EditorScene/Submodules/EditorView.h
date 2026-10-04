#pragma once
#include "EditorSupport.h"
#include "EditorVisual.h"
#include "EditorWorkspace.h"
#include "Texts/EditorScene/EditorTexts.h"
#include <functional>
#include <vector>

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
    void DrawScore();
    void DrawTools();
    void UpdateSliders(const mrg::UpdateContext &context);
    void SyncDivisionSlider() noexcept;
    void DrawTiming();
    void DrawEffects();
    void DrawAudio();
    void DrawAudioWaveform(const std::vector<finger_drum::editor::SpectrumFrame> &columns);
    void DrawAudioTimeRuler(double begin);
    void DrawAudioSpectrum(v::Rect rect, const std::vector<finger_drum::editor::SpectrumFrame> &columns,
                           double minimumHz, double maximumHz);
    void DrawTimeline();
    void SyncTimelineSlider();
    void Box(v::Rect r, v::Color color, float radius = 0) override;
    void Text(v::Rect r, std::wstring text, float size = 20, v::Color color = editor_ui::Ink) override;
    void Button(v::Rect r, std::wstring text, std::function<void()> action, bool selected = false) override;
    void Field(v::Rect r, const wchar_t *label, std::string &value);
    [[nodiscard]] const finger_drum::texts::EditorTextSet &Texts() const noexcept;
    mrg::visual2d::ScreenVisual2DManager &visuals;
    EditorWorkspace &state_;
    finger_drum::texts::TextCatalog &texts_;
    std::uint64_t textRevision_{};
    mrg::visual2d::ScreenCanvasHandle canvas;
    mrg::visual2d::Visual2DNode *node{};
    mrg::visual2d::Visual2DNode *divisionSliderNode_{};
    mrg::visual2d::Visual2DNode *timelineSliderNode_{};
    mrg::visual2d::Visual2DInputRouter sliderInput_;
    EditorVisual *visual{};
    static constexpr v::Rect DivisionSliderRect{1240, 6, 480, 26};
    static constexpr v::Rect TimelineSliderRect{205, 915, 1645, 44};
    std::vector<Control> controls;
    std::uint32_t width{1280}, height{720};
};
