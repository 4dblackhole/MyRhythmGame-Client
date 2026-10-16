#pragma once
#include "MRG_Core.h"
#include "Presentation/SkinVisualComponent.h"
#include "Presentation/UiColors.h"
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
            throw std::logic_error("The title menu node is missing a component.");
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
        namespace colors = finger_drum::presentation::ui_colors;
        mrg::visual2d::VisualStyle style{};
        if (selected)
        {
            style.normal = colors::logo::Selected;
            style.hovered = colors::logo::SelectedHover;
            style.pressed = colors::logo::SelectedPressed;
            style.disabled = colors::logo::SelectedDisabled;
        }
        else
        {
            style.normal = colors::logo::Button;
            style.hovered = style.normal;
            style.pressed = colors::logo::Pressed;
            style.disabled = colors::logo::Disabled;
        }

        finger_drum::presentation::SetSkinBaseStyle(button, style);
        RequireComponent<mrg::visual2d::TextVisualComponent>(button).SetTextColor(selected ? colors::White
                                                                                           : colors::Ink);
    }
} // namespace logo_ui
