#pragma once
#include "MRG_Core.h"
#include <array>
#include <stdexcept>

namespace logo_ui
{
    constexpr mrg::visual2d::Size LeftFadeSourceSize{1680.0F, 1280.0F};
    constexpr mrg::visual2d::Size CenterLogoSourceSize{2400.0F, 1280.0F};
    constexpr mrg::visual2d::Size RightFadeSourceSize{1680.0F, 1280.0F};
    constexpr float StripSourceHeight = 1280.0F;
    constexpr mrg::visual2d::Size MenuSize{360.0F, 280.0F};
    constexpr float MenuCenterOffsetY = -190.0F;
    constexpr float ButtonLeft = 60.0F;
    constexpr float ButtonWidth = 300.0F;
    constexpr float ButtonHeight = 52.0F;
    constexpr float ButtonVerticalStep = 76.0F;
    constexpr mrg::visual2d::Size CursorSize{48.0F, 48.0F};
    constexpr float CursorCenterX = 28.0F;
    constexpr std::size_t GameStartIndex = 0;
    constexpr std::size_t EditorIndex = 1;
    constexpr std::size_t OptionIndex = 2;
    constexpr std::size_t ExitIndex = 3;

    template <typename ComponentType>
    [[nodiscard]] inline ComponentType &RequireComponent(mrg::visual2d::Visual2DNode &node)
    {
        ComponentType *component = node.GetComponent<ComponentType>();
        if (component == nullptr)
        {
            throw std::logic_error("The FingerDrum menu node is missing a component.");
        }
        return *component;
    }

    inline void SetCenteredImageLayout(mrg::visual2d::Visual2DNode &node, const float centerX,
                                       const float centerY, const mrg::visual2d::Size size)
    {
        node.SetPivot({0.5F, 0.5F});
        node.SetSize(size);
        node.SetPosition({centerX, centerY});
    }

    inline void ApplyButtonSelectionStyle(mrg::visual2d::Visual2DNode &button, const bool selected)
    {
        mrg::visual2d::VisualStyle style{};
        if (selected)
        {
            style.normal = {0.22F, 0.56F, 0.88F, 0.94F};
            style.hovered = {0.30F, 0.66F, 0.96F, 1.0F};
            style.pressed = {0.12F, 0.39F, 0.72F, 1.0F};
            style.disabled = {0.20F, 0.35F, 0.52F, 0.55F};
        }
        else
        {
            style.normal = {0.78F, 0.89F, 0.98F, 0.88F};
            style.hovered = style.normal;
            style.pressed = {0.20F, 0.48F, 0.78F, 1.0F};
            style.disabled = {0.55F, 0.64F, 0.72F, 0.50F};
        }

        RequireComponent<mrg::visual2d::SpriteVisualComponent>(button).SetStyle(style);
        RequireComponent<mrg::visual2d::TextVisualComponent>(button).SetTextColor(
            selected ? mrg::visual2d::Color{1.0F, 1.0F, 1.0F, 1.0F}
                     : mrg::visual2d::Color{0.08F, 0.28F, 0.52F, 1.0F});
    }
} // namespace logo_ui
