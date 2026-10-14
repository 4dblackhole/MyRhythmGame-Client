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
    TaikoResultView(const finger_drum::mode::TaikoPlayResult &result, mrg::visual2d::ScreenVisual2DManager &visuals,
                    finger_drum::texts::TextCatalog &texts, ResultViewContext context = {});
    ~TaikoResultView() override;
    void Initialize(const mrg::EngineServices &services) override;
    void SetVisible(bool visible) noexcept override;
    ResultAction Update(const mrg::platform::InputState &input) override;
    void OnResize(std::uint32_t width, std::uint32_t height) override;
    void Shutdown() noexcept override;

  private:
    void Build();
    void LoadAssets();
    void BuildBackground(mrg::visual2d::Visual2DNode &root);
    void BuildSummary(mrg::visual2d::Visual2DNode &root);
    void BuildHpGraph(mrg::visual2d::Visual2DNode &root);
    void BuildJudgements(mrg::visual2d::Visual2DNode &root);
    void BuildGroups(mrg::visual2d::Visual2DNode &root);
    void BuildMetadata(mrg::visual2d::Visual2DNode &root);
    void BuildGraphs(mrg::visual2d::Visual2DNode &root);
    void BuildHistogramLabels(mrg::visual2d::Visual2DNode &root, const taiko_result_ui::GraphData &data);
    void BuildScatterLabels(mrg::visual2d::Visual2DNode &root, const taiko_result_ui::GraphData &data);
    void BuildGraphLegend(mrg::visual2d::Visual2DNode &root);
    void BuildActions(mrg::visual2d::Visual2DNode &root);
    mrg::visual2d::Visual2DNode &Label(mrg::visual2d::Visual2DNode &root, mrg::visual2d::Rect designBounds,
                                       std::wstring text, float size, std::uint16_t weight = 500,
                                       mrg::visual2d::TextAlignment alignment = mrg::visual2d::TextAlignment::Leading,
                                       mrg::visual2d::Color color = {40.0F / 255, 83.0F / 255, 116.0F / 255, 1});
    void Panel(mrg::visual2d::Visual2DNode &root, mrg::visual2d::Rect designBounds, mrg::visual2d::Color fill,
               float radius, float border = 0,
               mrg::visual2d::Color borderColor = {79.0F / 255, 159.0F / 255, 223.0F / 255, 1});
    mrg::visual2d::Visual2DNode &Button(mrg::visual2d::Visual2DNode &root, mrg::visual2d::Rect designBounds,
                                        std::string name, std::wstring title, bool outlined);
    void ProcessPointer(const mrg::platform::InputState &input);
    const finger_drum::mode::TaikoPlayResult &result_;
    mrg::visual2d::ScreenVisual2DManager &visuals_;
    finger_drum::texts::TextCatalog &texts_;
    ResultViewContext context_;
    mrg::visual2d::TextFont font_;
    mrg::visual2d::ImageHandle sky_, clouds_, modeIcon_, rankFrame_;
    mrg::visual2d::ScreenCanvasHandle canvasHandle_;
    mrg::visual2d::Visual2DCanvas *canvas_{};
    mrg::visual2d::Visual2DInputRouter router_;
    mrg::visual2d::NodeId contentId_{}, backId_{}, retryId_{}, backgroundId_{}, footerId_{};
    std::uint64_t textRevision_{};
    std::uint32_t width_{}, height_{};
};
