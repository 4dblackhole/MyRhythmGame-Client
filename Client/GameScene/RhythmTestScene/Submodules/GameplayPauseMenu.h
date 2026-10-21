#pragma once
#include "GameplayPauseAction.h"
#include "PhantomCore.h"
#include "Texts/TextCatalog.h"
#include <array>
#include <optional>
#include <string>

// Owns only the modal Canvas, focus and parameter editing. Session, judgement
// and audio changes remain in GameplaySessionController.
class GameplayPauseMenu final
{
  public:
    GameplayPauseMenu(ScreenVisual2DManager &visuals, TextCatalog &texts)
        : visuals_(visuals), texts_(texts)
    {
    }
    ~GameplayPauseMenu()
    {
        Shutdown();
    }
    void Show(std::uint32_t width, std::uint32_t height, bool canResume, std::size_t measureCount);
    void Hide() noexcept;
    GameplayPauseChoice Update(const InputState &input);
    GameplayPauseChoice HandleKey(std::uint16_t virtualKey);
    void OnResize(std::uint32_t width, std::uint32_t height);
    void Shutdown() noexcept;

  private:
    void Build();
    void BuildRow(Visual2DNode &parent, std::size_t index);
    void BuildSelector(Visual2DNode &parent, std::size_t index);
    void RefreshRows();
    void AdjustValue(std::size_t index, int direction);
    void ProcessActions(GameplayPauseChoice &result);
    void ProcessPointer(const InputState &input);
    Visual2DNode &Label(Visual2DNode &parent, Rect bounds,
                                       std::wstring text, float size, Color color);
    ScreenVisual2DManager &visuals_;
    TextCatalog &texts_;
    ScreenCanvasHandle handle_;
    Visual2DInputRouter router_;
    std::array<NodeId, 4> rows_{}, frames_{}, fills_{}, captions_{};
    std::array<NodeId, 2> values_{}, previous_{}, next_{};
    NodeId rootId_{};
    std::optional<Point> lastPointer_;
    std::size_t focus_{}, maxMeasures_{4};
    GameplayPauseChoice choice_;
    std::uint32_t width_{}, height_{};
    std::uint64_t textRevision_{};
    bool visible_{}, canResume_{};
};
