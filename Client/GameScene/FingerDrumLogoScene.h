#pragma once

#include "MRG_Core.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

// Presents the FingerDrum title screen. The authored image panels remain
// centered at every viewport aspect ratio, while one Canvas tree owns the
// mouse/keyboard menu and its movable selection cursor.
class FingerDrumLogoScene final : public mrg::scene::GameScene
{
public:
    explicit FingerDrumLogoScene(
        mrg::visual2d::ScreenVisual2DManager& screenVisuals) noexcept;

    void Initialize(const mrg::EngineServices& services) override;
    void BeginScene() override;
    void EndScene() noexcept override;
    void Update(
        const mrg::UpdateContext& context,
        mrg::scene::SceneManager& scenes) override;
    void Render(const mrg::graphics::RenderContext& context) override;
    void OnResize(std::uint32_t width, std::uint32_t height) override;
    void Shutdown() noexcept override;

private:
    void CreateLogoStrip();
    void CreateMenu();
    void UpdateLogoStripLayout();
    void ProcessPointer(const mrg::platform::InputState& input);
    void UpdateSelectionFromPointer(
        const mrg::platform::InputState& input);
    void HandleKeyboard(
        const mrg::platform::InputState& input,
        mrg::scene::SceneManager& scenes);
    [[nodiscard]] bool ApplyMenuActions(
        mrg::scene::SceneManager& scenes);
    void SetSelectedMenuItem(std::size_t index);
    void ActivateMenuItem(
        std::size_t index,
        mrg::scene::SceneManager& scenes);
    [[nodiscard]] static bool HasPointerActivity(
        const mrg::platform::InputState& input) noexcept;
    [[nodiscard]] static std::int64_t LatestPointerTimestamp(
        const mrg::platform::InputState& input) noexcept;

    std::uint32_t width_{1280};
    std::uint32_t height_{720};
    mrg::visual2d::ScreenVisual2DManager& screenVisuals_;
    mrg::visual2d::ScreenCanvasHandle canvasHandle_;
    mrg::visual2d::Visual2DCanvas* canvas_{};
    mrg::visual2d::Visual2DInputRouter inputRouter_;
    mrg::visual2d::Visual2DNode* logoStrip_{};
    mrg::visual2d::Visual2DNode* leftFade_{};
    mrg::visual2d::Visual2DNode* centerLogo_{};
    mrg::visual2d::Visual2DNode* rightFade_{};
    std::array<mrg::visual2d::Visual2DNode*, 2> menuButtons_{};
    std::array<mrg::visual2d::NodeId, 2> menuButtonIds_{};
    mrg::visual2d::Visual2DNode* selectionCursor_{};
    std::size_t selectedMenuIndex_{};
};
