#pragma once
#include "MRG_Core.h"
#include "Presentation/UiColors.h"
#include "Texts/Options/OptionTexts.h"
#include <Windows.h>
#include <array>
#include <stdexcept>

namespace finger_drum::presentation
{
    namespace options_ui
    {
        constexpr auto PanelColor = ui_colors::options::Background;
        constexpr auto HeadingColor = ui_colors::Ink, LabelColor = ui_colors::options::Label;
        constexpr auto FieldColor = ui_colors::options::Field, FieldHoverColor = ui_colors::options::Hover;

        template <typename Component> [[nodiscard]] Component &RequireComponent(mrg::visual2d::Visual2DNode &node)
        {
            Component *component = node.GetComponent<Component>();
            if (component == nullptr)
            {
                throw std::logic_error("The options panel is missing a required component.");
            }
            return *component;
        }

        inline void SetPanelColor(mrg::visual2d::Visual2DNode &node, const mrg::visual2d::Color color)
        {
            mrg::visual2d::VisualStyle style{};
            style.normal = style.hovered = style.pressed = style.disabled = color;
            RequireComponent<mrg::visual2d::SpriteVisualComponent>(node).SetStyle(style);
        }

        inline void ConfigureCombo(mrg::visual2d::Visual2DNode &node, const std::size_t visibleItems)
        {
            node.SetZIndex(10);
            mrg::visual2d::VisualStyle style{};
            style.normal = FieldColor;
            style.hovered = FieldHoverColor;
            style.pressed = ui_colors::options::Pressed;
            style.disabled = ui_colors::options::Disabled;
            RequireComponent<mrg::visual2d::SpriteVisualComponent>(node).SetStyle(style);
            auto &combo = RequireComponent<mrg::visual2d::ComboBoxBehaviorComponent>(node);
            combo.SetMaxVisibleItems(visibleItems);
            combo.SetItemHeight(42.0F);
            combo.SetFontSize(19.0F);
            combo.SetTextColor(HeadingColor);
            combo.SetSelectedTextColor(ui_colors::White);
            combo.SetPopupBackgroundColor(PanelColor);
        }

        inline std::wstring WideText(const std::string &value)
        {
            if (value.empty())
                return {};
            const int length =
                MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
            std::wstring result(static_cast<std::size_t>(length), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), length);
            return result;
        }

        inline mrg::visual2d::Visual2DNode &CreateFieldLabel(mrg::visual2d::Visual2DNode &parent, float y, float width,
                                                             std::string name)
        {
            auto &node = mrg::visual2d::CreateLabel(parent, {24.0F, y, width - 48.0F, 28.0F}, L"", std::move(name));
            auto &text = RequireComponent<mrg::visual2d::TextVisualComponent>(node);
            text.SetFontSize(18.0F);
            text.SetTextColor(LabelColor);
            return node;
        }

        inline std::wstring OutputName(mrg::audio::AudioOutputBackend output, const texts::OptionTextSet &text)
        {
            using mrg::audio::AudioOutputBackend;
            switch (output)
            {
            case AudioOutputBackend::Automatic:
                return std::wstring(text.automatic);
            case AudioOutputBackend::Wasapi:
                return std::wstring(text.outputWasapi);
            case AudioOutputBackend::Asio:
                return std::wstring(text.outputAsio);
            default:
                return std::wstring(text.noSound);
            }
        }

        constexpr std::array Outputs{mrg::audio::AudioOutputBackend::Automatic, mrg::audio::AudioOutputBackend::Wasapi,
                                     mrg::audio::AudioOutputBackend::Asio};
    } // namespace options_ui

} // namespace finger_drum::presentation
