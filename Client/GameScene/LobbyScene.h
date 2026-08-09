#pragma once

#include "MRG_Core.h"

#include <cstdint>
#include <memory>
#include <string>

// Penpot의 Music Select · Sky 레이아웃을 사용하되, 아직 곡 catalog와
// 기록 저장소가 없는 현재 상태를 정직한 빈 화면으로 표현한다.
class LobbyScene final : public mrg::scene::GameScene
{
public:
    void Initialize(const mrg::EngineServices& services) override;
    void Update(
        const mrg::UpdateContext& context,
        mrg::scene::SceneManager& scenes) override;
    void Render(const mrg::graphics::RenderContext& context) override;
    void OnResize(std::uint32_t width, std::uint32_t height) override;
    void Shutdown() noexcept override;

private:
    void CreateHeader(const mrg::EngineServices& services);
    void CreateCategoryBar();
    void CreateRecordPanel();
    void CreateSongInformationPanel();
    void CreatePatternPanel();
    void CreateSongList();
    void CreateFooter();

    mrg::visual2d::Visual2DNode& AddPanel(
        mrg::visual2d::Visual2DNode& parent,
        mrg::visual2d::Rect penpotBounds,
        mrg::visual2d::Color color,
        std::string name);
    mrg::visual2d::Visual2DNode& AddLabel(
        mrg::visual2d::Visual2DNode& parent,
        mrg::visual2d::Rect penpotBounds,
        std::wstring text,
        float penpotFontSize,
        mrg::visual2d::Color color,
        std::string name,
        mrg::visual2d::TextAlignment alignment =
            mrg::visual2d::TextAlignment::Leading);

    std::uint32_t width_{1280};
    std::uint32_t height_{720};
    std::unique_ptr<mrg::visual2d::Visual2DCanvas> canvas_;
    mrg::visual2d::Visual2DNode* board_{};
};
