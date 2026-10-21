#pragma once
#include "PhantomCore.h"
#include "Presentation/SkinVisualComponent.h"
#include "Presentation/UiColors.h"
#include <array>
#include <stdexcept>

namespace logo_ui
{
    constexpr Size LeftFadeSourceSize{1680.0F, 1280.0F};
    constexpr Size CenterLogoSourceSize{2400.0F, 1280.0F};
    constexpr Size RightFadeSourceSize{1680.0F, 1280.0F};
    constexpr float StripSourceHeight = 1280.0F;
    constexpr Size MenuSize{360.0F, 280.0F};
    constexpr float MenuCenterOffsetY = -190.0F;
    constexpr float ButtonLeft = 60.0F;
    constexpr float ButtonWidth = 300.0F;
    constexpr float ButtonHeight = 52.0F;
    constexpr float ButtonVerticalStep = 76.0F;
    constexpr Size CursorSize{48.0F, 48.0F};
    constexpr float CursorCenterX = 28.0F;
    constexpr std::size_t GameStartIndex = 0;
    constexpr std::size_t EditorIndex = 1;
    constexpr std::size_t OptionIndex = 2;
    constexpr std::size_t ExitIndex = 3;

    template <typename ComponentType>
    [[nodiscard]] inline ComponentType &RequireComponent(Visual2DNode &node)
    {
        ComponentType *component = node.GetComponent<ComponentType>();
        if (component == nullptr)
        {
            throw std::logic_error("The title menu node is missing a component.");
        }
        return *component;
    }

    inline void SetCenteredImageLayout(Visual2DNode &node, const float centerX,
                                       const float centerY, const Size size)
    {
        node.SetPivot({0.5F, 0.5F});
        node.SetSize(size);
        node.SetPosition({centerX, centerY});
    }

    inline void ApplyButtonSelectionStyle(Visual2DNode &button, const bool selected)
    {

        VisualStyle style{};
        if (selected)
        {
            style.normal = ui_colors::logo::Selected;
            style.hovered = ui_colors::logo::SelectedHover;
            style.pressed = ui_colors::logo::SelectedPressed;
            style.disabled = ui_colors::logo::SelectedDisabled;
        }
        else
        {
            style.normal = ui_colors::logo::Button;
            style.hovered = style.normal;
            style.pressed = ui_colors::logo::Pressed;
            style.disabled = ui_colors::logo::Disabled;
        }

        SetSkinBaseStyle(button, style);
        RequireComponent<TextVisualComponent>(button).SetTextColor(selected ? ui_colors::White
                                                                                           : ui_colors::Ink);
    }
} // namespace logo_ui
