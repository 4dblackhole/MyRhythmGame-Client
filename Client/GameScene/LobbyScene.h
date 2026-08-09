#pragma once

#include "MRG_Core.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

// Penpot의 "Music Select · Sky" 화면을 엔진의 Visual2D 트리로 옮긴
// 곡 선택 Scene이다. 디자인 좌표는 1920x1080에서 1280x720으로
// 환산하며, 화면비가 바뀌어도 전체 보드는 화면 중앙에 유지된다.
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
    void CreateHeader();
    void CreateCategoryBar();
    void CreateRecordPanel();
    void CreateSongInformationPanel();
    void CreatePatternPanel();
    void CreateSongList();
    void CreateBottomBar();
    void CreateModifierPopup();
    void ProcessPointer(const mrg::platform::InputState& input);
    void ProcessActions(mrg::scene::SceneManager& scenes);
    void ProcessKeyboard(
        const mrg::platform::InputState& input,
        mrg::scene::SceneManager& scenes);
    void SelectSong(std::size_t index);
    void ToggleModifierPopup();
    void EnterSelectedPattern(mrg::scene::SceneManager& scenes);

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
    mrg::visual2d::Visual2DNode& AddButton(
        mrg::visual2d::Visual2DNode& parent,
        mrg::visual2d::Rect penpotBounds,
        std::wstring text,
        float penpotFontSize,
        mrg::visual2d::Color color,
        mrg::visual2d::Color textColor,
        std::string name);

    std::uint32_t width_{1280};
    std::uint32_t height_{720};
    std::unique_ptr<mrg::visual2d::Visual2DCanvas> canvas_;
    mrg::visual2d::Visual2DInputRouter inputRouter_;
    mrg::visual2d::Visual2DNode* board_{};
    mrg::visual2d::Visual2DNode* modifierPopup_{};
    mrg::visual2d::Visual2DNode* selectedSongTitle_{};
    std::array<mrg::visual2d::Visual2DNode*, 8> songRows_{};
    std::array<mrg::visual2d::NodeId, 8> songRowIds_{};
    mrg::visual2d::NodeId playButtonId_{};
    mrg::visual2d::NodeId modifierButtonId_{};
    mrg::visual2d::NodeId closeModifierButtonId_{};
    std::size_t selectedSongIndex_{2};
};
