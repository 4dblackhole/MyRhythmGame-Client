#pragma once
#include "GameplayPauseAction.h"
#include "MRG_Core.h"
#include "Texts/TextCatalog.h"
#include <array>
#include <optional>
#include <string>

// Owns only the modal Canvas, focus and parameter editing. Session, judgement
// and audio changes remain in GameplaySessionController.
class GameplayPauseMenu final
{
  public:
    GameplayPauseMenu(mrg::visual2d::ScreenVisual2DManager &visuals, finger_drum::texts::TextCatalog &texts)
        : visuals_(visuals), texts_(texts)
    {
    }
    ~GameplayPauseMenu()
    {
        Shutdown();
    }
    void Show(std::uint32_t width, std::uint32_t height, bool canResume, std::size_t measureCount);
    void Hide() noexcept;
    GameplayPauseChoice Update(const mrg::platform::InputState &input);
    GameplayPauseChoice HandleKey(std::uint16_t virtualKey);
    void OnResize(std::uint32_t width, std::uint32_t height);
    void Shutdown() noexcept;

  private:
    void Build();
    void RefreshRows();
    void ProcessPointer(const mrg::platform::InputState &input);
    mrg::visual2d::Visual2DNode &Label(mrg::visual2d::Visual2DNode &parent, mrg::visual2d::Rect bounds,
                                       std::wstring text, float size, mrg::visual2d::Color color);
    mrg::visual2d::ScreenVisual2DManager &visuals_;
    finger_drum::texts::TextCatalog &texts_;
    mrg::visual2d::ScreenCanvasHandle handle_;
    mrg::visual2d::Visual2DInputRouter router_;
    std::array<mrg::visual2d::NodeId, 4> rows_{}, descriptions_{};
    mrg::visual2d::NodeId rootId_{};
    std::optional<mrg::visual2d::Point> lastPointer_;
    std::size_t focus_{}, maxMeasures_{4};
    GameplayPauseChoice choice_;
    std::uint32_t width_{}, height_{};
    std::uint64_t textRevision_{};
    bool visible_{}, canResume_{};
};
