#pragma once
#include "PhantomCore.h"
#include "Presentation/UiColors.h"
#include <array>
#include <format>
#include <string>
namespace taiko_result_ui::layout
{

    inline constexpr float Width = 1920, Height = 1080;
    // Penpot uses top-left / Y-down; convert once at UI construction.
    constexpr Rect Bounds(Rect source)
    {
        return {source.x, Height - source.y - source.height, source.width, source.height};
    }
    constexpr Color ResultColor(unsigned rgb, float alpha = 1)
    {
        return ui_colors::Rgb(rgb, alpha);
    }

    inline constexpr auto Ink = ui_colors::result::Ink, Muted = ui_colors::result::Muted, Accent = ui_colors::result::Accent;
    inline constexpr auto Surface = ui_colors::result::Surface, Plot = ui_colors::result::Plot, Keyline = ui_colors::result::Keyline;
    inline constexpr std::array<Color, 8> Markers{ResultColor(0xFFFFFF), ResultColor(0x2DDFFF), ResultColor(0x28FF28),
                                                     ResultColor(0xFFBF20), ResultColor(0x4018FF), Accent,
                                                     ResultColor(0x829AB3), ResultColor(0xFF3A26)};
    inline constexpr Rect Banner{0, 0, 1920, 88};
    inline constexpr Rect HistogramPanel{1344, 104, 528, 352}, ScatterPanel{1344, 476, 528, 416};
    inline constexpr Rect Scatter{1434, 620, 410, 188}, Histogram{1420, 182, 424, 206};
    inline constexpr Rect ReplayButton{1344, 914, 256, 62}, RetryButton{1616, 914, 256, 62};
    template <typename... Args> std::wstring Format(std::wstring_view value, Args &&...args)
    {
        return std::vformat(value, std::make_wformat_args(args...));
    }
} // namespace taiko_result_ui::layout
