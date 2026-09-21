#pragma once

#include "MRG_Core.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

// Presents the FingerDrum title screen. The authored image panels remain
// centered at every viewport aspect ratio, while one Canvas tree owns the
// mouse/keyboard menu and its movable selection cursor.
class LogoView final
{
  public:
    explicit LogoView(mrg::visual2d::ScreenVisual2DManager &screenVisuals) noexcept;

    void Initialize(const mrg::EngineServices &services);
    void BeginScene();
    void EndScene() noexcept;
    std::optional<std::size_t> Update(const mrg::platform::InputState &input);
    void OnResize(std::uint32_t width, std::uint32_t height);
    void Shutdown() noexcept;

  private:
    std::optional<std::size_t> command_;
    void CreateLogoStrip();
    void CreateMenu();
    void UpdateLogoStripLayout();
    void ProcessPointer(const mrg::platform::InputState &input);
    void UpdateSelectionFromPointer(const mrg::platform::InputState &input);
    void HandleKeyboard(const mrg::platform::InputState &input);
    [[nodiscard]] bool ApplyMenuActions();
    void SetSelectedMenuItem(std::size_t index);
    void ActivateMenuItem(std::size_t index);
    [[nodiscard]] static bool HasPointerActivity(const mrg::platform::InputState &input) noexcept;
    [[nodiscard]] static std::int64_t LatestPointerTimestamp(
        const mrg::platform::InputState &input) noexcept;

    std::uint32_t width_{1280};
    std::uint32_t height_{720};
    mrg::visual2d::ScreenVisual2DManager &screenVisuals_;
    mrg::visual2d::ScreenCanvasHandle canvasHandle_;
    mrg::visual2d::Visual2DCanvas *canvas_{};
    mrg::visual2d::Visual2DInputRouter inputRouter_;
    mrg::visual2d::Visual2DNode *logoStrip_{};
    mrg::visual2d::Visual2DNode *leftFade_{};
    mrg::visual2d::Visual2DNode *centerLogo_{};
    mrg::visual2d::Visual2DNode *rightFade_{};
    std::array<mrg::visual2d::Visual2DNode *, 4> menuButtons_{};
    std::array<mrg::visual2d::NodeId, 4> menuButtonIds_{};
    mrg::visual2d::Visual2DNode *selectionCursor_{};
    std::size_t selectedMenuIndex_{};
};
