#pragma once
#include "MRG_Core.h"

// Shared UI colors. Scene layouts map these tokens to their local roles;
// note/judgement colors remain owned by their gameplay presentation policy.
namespace finger_drum::presentation::ui_colors
{
    using Color = mrg::visual2d::Color;
    constexpr Color Rgb(unsigned rgb, float alpha = 1)
    {
        return {static_cast<float>((rgb >> 16) & 255) / 255, static_cast<float>((rgb >> 8) & 255) / 255,
                static_cast<float>(rgb & 255) / 255, alpha};
    }
    inline constexpr Color White{1, 1, 1, 1}, Ink{.08F, .28F, .52F, 1}, Error{.75F, .17F, .21F, 1};
    inline constexpr auto ClearBackground = Rgb(0xF0F8FF);
    namespace sky
    {
        inline constexpr Color Background{.918F, .965F, 1, 1}, Surface{.976F, .992F, 1, .98F};
        inline constexpr Color Accent{.310F, .624F, .878F, 1}, AccentHover{.365F, .690F, .930F, 1};
        inline constexpr Color Border{.569F, .769F, .890F, 1}, PaleBorder{.725F, .851F, .929F, 1};
        inline constexpr Color Pale{.937F, .973F, .996F, 1}, Viewport{.933F, .973F, .992F, .62F};
        inline constexpr Color Focus{.906F, .965F, 1, 1}, Row{.929F, .973F, .996F, 1};
        inline constexpr Color Ink{.157F, .325F, .455F, 1}, Muted{.290F, .471F, .592F, 1};
        inline constexpr Color SoftText{.455F, .659F, .784F, 1}, Disabled{.70F, .78F, .84F, .55F};
    } // namespace sky
    namespace logo
    {
        inline constexpr Color Selected{.22F, .56F, .88F, .94F}, SelectedHover{.30F, .66F, .96F, 1};
        inline constexpr Color SelectedPressed{.12F, .39F, .72F, 1}, SelectedDisabled{.20F, .35F, .52F, .55F};
        inline constexpr Color Button{.78F, .89F, .98F, .88F}, Pressed{.20F, .48F, .78F, 1};
        inline constexpr Color Disabled{.55F, .64F, .72F, .50F};
    } // namespace logo
    namespace options
    {
        // Reuse the existing sky border shade as a stronger panel surface.
        inline constexpr auto Background = sky::PaleBorder;
        inline constexpr auto Field = sky::Pale, Hover = sky::Focus, Pressed = sky::Accent;
        inline constexpr auto Disabled = sky::Disabled, Label = sky::Ink;
    } // namespace options
    namespace editor
    {
        inline constexpr Color Background{.914F, .961F, 1, 1}, Paper{.973F, .988F, 1, 1};
        inline constexpr Color Ink{.17F, .34F, .47F, 1}, Accent{.31F, .62F, .88F, 1}, Pale{.82F, .91F, .96F, 1};
    } // namespace editor
    namespace result
    {
        inline constexpr auto Ink = Rgb(0x285374), Muted = Rgb(0x5783A0), Accent = Rgb(0x4F9FDF);
        inline constexpr auto Surface = Rgb(0xF9FDFF), Plot = Rgb(0xEAF6FF), Keyline = Rgb(0xBEDFF3);
    } // namespace result
    namespace pause
    {
        inline constexpr Color Muted{.65F, .8F, .95F, 1}, Surface{.07F, .14F, .24F, 1};
        inline constexpr Color Row{.11F, .22F, .35F, 1}, Focus{.15F, .38F, .62F, 1};
        inline constexpr Color Hover{.18F, .43F, .68F, 1}, Pressed{.08F, .28F, .48F, 1};
        inline constexpr Color Disabled{.11F, .18F, .25F, 1};
    } // namespace pause
} // namespace finger_drum::presentation::ui_colors
