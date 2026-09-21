#pragma once
#include "EditorWorkspace.h"
#include "EditorVisual.h"
#include "EditorSupport.h"
#include <functional>
#include <vector>

class EditorView final
{
  public:
    EditorView(mrg::visual2d::ScreenVisual2DManager &manager, EditorWorkspace &state)
        : visuals(manager), state_(state)
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
        canvas.Reset();
        node = nullptr;
        visual = nullptr;
    }
    void Build();
    void ReloadSize();
    bool Update(const mrg::UpdateContext &context);

  private:
    void DrawScore();
    void DrawTools();
    void DrawChartContent();
    void DrawVariantMenu();
    void EditScore(v::Point point, bool erase);
    void DrawTiming();
    void DrawMetadata();
    void DrawEffects();
    void DrawAudio();
    void Box(v::Rect r, v::Color color, float radius = 0);
    void Text(v::Rect r, std::wstring text, float size = 20, v::Color color = editor_ui::Ink);
    void Button(v::Rect r, std::wstring text, std::function<void()> action, bool selected = false);
    void Field(v::Rect r, const wchar_t *label, std::string &value);
    void Circle(float x, float y, int type, float radius);
    mrg::visual2d::ScreenVisual2DManager &visuals;
    EditorWorkspace &state_;
    mrg::visual2d::ScreenCanvasHandle canvas;
    mrg::visual2d::Visual2DNode *node{};
    EditorVisual *visual{};
    std::vector<Control> controls;
    std::vector<NoteHit> noteHits;
    std::vector<std::pair<float, finger_drum::chart::MusicalPosition>> realtimeGrid;
    std::uint32_t width{1280}, height{720};
};
