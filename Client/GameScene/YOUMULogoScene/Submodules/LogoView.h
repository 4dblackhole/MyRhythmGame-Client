#pragma once

#include "PhantomCore.h"
#include "Presentation/OptionsPanel.h"
#include "Texts/TextCatalog.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

// Presents the YOUMU title screen. The authored image panels remain
// centered at every viewport aspect ratio, while one Canvas tree owns the
// mouse/keyboard menu and its movable selection cursor.
class LogoView final
{
  public:
    LogoView(ScreenVisual2DManager &screenVisuals,
             TextCatalog &texts, OptionSettings &options) noexcept;

    void Initialize(const EngineServices &services);
    void BeginScene();
    void EndScene() noexcept;
    std::optional<std::size_t> Update(const InputState &input,
                                      double deltaSeconds);
    void OnResize(std::uint32_t width, std::uint32_t height);
    void Shutdown() noexcept;

  private:
    std::optional<std::size_t> command_;
    void CreateLogoStrip();
    void CreateMenu();
    void ApplyTexts();
    void UpdateLogoStripLayout();
    void RefreshSkinImages();
    void ProcessPointer(const InputState &input);
    void UpdateSelectionFromPointer(const InputState &input);
    void HandleKeyboard(const InputState &input);
    [[nodiscard]] bool ApplyMenuActions();
    void SetSelectedMenuItem(std::size_t index);
    void ActivateMenuItem(std::size_t index);
    [[nodiscard]] static bool HasPointerActivity(const InputState &input) noexcept;
    [[nodiscard]] static std::int64_t LatestPointerTimestamp(
        const InputState &input) noexcept;

    std::uint32_t width_{1280};
    std::uint32_t height_{720};
    ScreenVisual2DManager &screenVisuals_;
    TextCatalog &texts_;
    OptionsPanel options_;
    std::uint64_t textRevision_{};
    std::uint64_t skinRevision_{};
    ScreenCanvasHandle canvasHandle_;
    Visual2DCanvas *canvas_{};
    Visual2DInputRouter inputRouter_;
    std::optional<Point> canvasPointer_;
    Visual2DNode *logoStrip_{};
    Visual2DNode *background_{};
    Visual2DNode *leftFade_{};
    Visual2DNode *centerLogo_{};
    Visual2DNode *rightFade_{};
    std::array<Visual2DNode *, 4> menuButtons_{};
    std::array<NodeId, 4> menuButtonIds_{};
    Visual2DNode *selectionCursor_{};
    std::size_t selectedMenuIndex_{};
};
