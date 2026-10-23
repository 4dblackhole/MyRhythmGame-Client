#pragma once
#include "GameScene/ResultScene/Submodules/IResultView.h"
#include "Taiko/Results/TaikoPlayResult.h"
#include "Texts/TextCatalog.h"
namespace taiko_result_ui
{
    struct GraphData;
}
class TaikoResultView final : public IResultView
{
  public:
    TaikoResultView(const TaikoPlayResult &result, ScreenVisual2DManager &visuals,
                    TextCatalog &texts, ResultViewContext context = {});
    ~TaikoResultView() override;
    void Initialize(const EngineServices &services) override;
    void SetVisible(bool visible) noexcept override;
    ResultAction Update(const InputState &input) override;
    void OnResize(std::uint32_t width, std::uint32_t height) override;
    void Shutdown() noexcept override;

  private:
    void Build();
    void LoadAssets();
    void BuildBackground();
    void BuildSongBanner(Visual2DNode &root);
    void BuildSummary(Visual2DNode &root);
    void BuildHpGraph(Visual2DNode &root);
    void BuildJudgements(Visual2DNode &root);
    void BuildGroups(Visual2DNode &root);
    void BuildMetadata(Visual2DNode &root);
    void BuildOptions(Visual2DNode &root);
    void BuildGraphs(Visual2DNode &root);
    void BuildHistogramLabels(Visual2DNode &root, const taiko_result_ui::GraphData &data);
    void BuildScatterLabels(Visual2DNode &root, const taiko_result_ui::GraphData &data);
    void BuildGraphLegend(Visual2DNode &root);
    void BuildActions(Visual2DNode &root);
    Visual2DNode &Label(Visual2DNode &root, Rect designBounds,
                                       std::wstring text, float size, std::uint16_t weight = 500,
                                       TextAlignment alignment = TextAlignment::Leading,
                                       Color color = {40.0F / 255, 83.0F / 255, 116.0F / 255, 1});
    void Panel(Visual2DNode &root, Rect designBounds, Color fill,
               float radius, float border = 0,
               Color borderColor = {79.0F / 255, 159.0F / 255, 223.0F / 255, 1});
    Visual2DNode &Button(Visual2DNode &root, Rect designBounds,
                                        std::string name, std::wstring title, bool outlined);
    void ProcessPointer(const InputState &input);
    const TaikoPlayResult &result_;
    ScreenVisual2DManager &visuals_;
    TextCatalog &texts_;
    ResultViewContext context_;
    TextFont font_;
    ImageHandle sky_, clouds_, modeIcon_, rankFrame_, rankImage_;
    ScreenCanvasHandle canvasHandle_;
    Visual2DCanvas *canvas_{};
    Visual2DInputRouter router_;
    NodeId contentId_{}, backId_{}, retryId_{}, backgroundId_{}, footerId_{};
    std::uint64_t textRevision_{};
    std::uint32_t width_{}, height_{};
};
