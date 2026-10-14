#pragma once
#include "MRG_Core.h"
#include "Presentation/UiColors.h"
#include <array>
#include <format>
#include <string>
namespace taiko_result_ui::layout
{
    namespace v = mrg::visual2d;
    inline constexpr float Width = 1920, Height = 1080;
    // Penpot uses top-left / Y-down; convert once at UI construction.
    constexpr v::Rect Bounds(v::Rect source)
    {
        return {source.x, Height - source.y - source.height, source.width, source.height};
    }
    constexpr v::Color Color(unsigned rgb, float alpha = 1)
    {
        return finger_drum::presentation::ui_colors::Rgb(rgb, alpha);
    }
    namespace palette = finger_drum::presentation::ui_colors::result;
    inline constexpr auto Ink = palette::Ink, Muted = palette::Muted, Accent = palette::Accent;
    inline constexpr auto Surface = palette::Surface, Plot = palette::Plot, Keyline = palette::Keyline;
    inline constexpr std::array<v::Color, 8> Markers{Color(0xFFFFFF), Color(0x2DDFFF), Color(0x28FF28),
                                                     Color(0xFFBF20), Color(0x4018FF), Accent,
                                                     Color(0x829AB3), Color(0xFF3A26)};
    inline constexpr v::Rect Scatter{1242, 620, 602, 188}, Histogram{1228, 182, 616, 206};
    template <typename... Args> std::wstring Format(std::wstring_view value, Args &&...args)
    {
        return std::vformat(value, std::make_wformat_args(args...));
    }
} // namespace taiko_result_ui::layout
