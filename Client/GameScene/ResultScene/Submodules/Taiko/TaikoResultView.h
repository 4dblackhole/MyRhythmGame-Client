#pragma once
#include "GameScene/ResultScene/Submodules/IResultView.h"
#include "Taiko/Results/TaikoPlayResult.h"
#include "Texts/TextCatalog.h"
class TaikoResultView final : public IResultView
{
  public:
    TaikoResultView(const finger_drum::mode::TaikoPlayResult &result, mrg::visual2d::ScreenVisual2DManager &visuals,
                    finger_drum::texts::TextCatalog &texts);
    ~TaikoResultView() override;
    void Initialize(const mrg::EngineServices &services) override;
    void SetVisible(bool visible) noexcept override;
    bool Update(const mrg::platform::InputState &input) override;
    void OnResize(std::uint32_t width, std::uint32_t height) override;
    void Shutdown() noexcept override;

  private:
    void Build();
    void BuildSummary(mrg::visual2d::Visual2DNode &root, float width);
    void BuildGraphs(mrg::visual2d::Visual2DNode &root, float width);
    void Label(mrg::visual2d::Visual2DNode &root, mrg::visual2d::Rect bounds, std::wstring text, float size = 18);
    void ProcessPointer(const mrg::platform::InputState &input);
    const finger_drum::mode::TaikoPlayResult &result_;
    mrg::visual2d::ScreenVisual2DManager &visuals_;
    finger_drum::texts::TextCatalog &texts_;
    mrg::visual2d::ScreenCanvasHandle canvasHandle_;
    mrg::visual2d::Visual2DCanvas *canvas_{};
    mrg::visual2d::Visual2DInputRouter router_;
    mrg::visual2d::NodeId contentId_{}, backId_{};
    std::uint64_t textRevision_{};
    std::uint32_t width_{}, height_{};
};
