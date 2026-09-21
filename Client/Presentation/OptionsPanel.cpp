#include "OptionsPanel.h"

#include "Texts/Options/OptionTexts.h"

#include <algorithm>
#include <string>
#include <vector>

namespace finger_drum::presentation
{
    namespace
    {
        constexpr mrg::visual2d::Color PanelColor{0.965F, 0.984F, 1.0F, 0.985F};
        constexpr mrg::visual2d::Color HeadingColor{0.08F, 0.28F, 0.52F, 1.0F};
        constexpr mrg::visual2d::Color LabelColor{0.20F, 0.38F, 0.57F, 1.0F};
        constexpr mrg::visual2d::Color FieldColor{0.78F, 0.89F, 0.98F, 1.0F};
        constexpr mrg::visual2d::Color FieldHoverColor{0.64F, 0.82F, 0.96F, 1.0F};

        template <typename Component>
        [[nodiscard]] Component &RequireComponent(mrg::visual2d::Visual2DNode &node)
        {
            Component *component = node.GetComponent<Component>();
            if (component == nullptr)
            {
                throw std::logic_error("The options panel is missing a required component.");
            }
            return *component;
        }

        void SetPanelColor(mrg::visual2d::Visual2DNode &node,
                           const mrg::visual2d::Color color)
        {
            mrg::visual2d::VisualStyle style{};
            style.normal = style.hovered = style.pressed = style.disabled = color;
            RequireComponent<mrg::visual2d::SpriteVisualComponent>(node).SetStyle(style);
        }
    } // namespace

    void OptionsPanel::Initialize(mrg::visual2d::Visual2DCanvas &canvas)
    {
        auto &leftAnchor = canvas.AnchorNode(mrg::visual2d::Anchor::MiddleLeft);
        leftAnchor.SetZIndex(100);
        panel_ = &mrg::visual2d::CreatePanel(
            leftAnchor,
            {-PanelWidth, -PanelHeight * 0.5F, PanelWidth, PanelHeight}, "Options.Panel");
        panel_->SetZIndex(100);
        SetPanelColor(*panel_, PanelColor);
        panel_->AddComponent<mrg::visual2d::RectangleCollider2DComponent>();
        panel_->AddComponent<mrg::visual2d::PointerReceiverComponent>();
        panelId_ = panel_->Id();

        title_ = &mrg::visual2d::CreateLabel(
            *panel_, {24.0F, PanelHeight - 76.0F, PanelWidth - 48.0F, 44.0F}, L"",
            "Options.Title");
        auto &titleText = RequireComponent<mrg::visual2d::TextVisualComponent>(*title_);
        titleText.SetFontSize(30.0F);
        titleText.SetTextColor(HeadingColor);

        auto &viewport = panel_->CreateChild("Options.Viewport");
        viewport.SetBounds({0.0F, 24.0F, PanelWidth, ContentViewportHeight});
        viewport.SetClipRect({0.0F, 0.0F, PanelWidth, ContentViewportHeight});
        content_ = &viewport.CreateChild("Options.Content");
        content_->SetSize({PanelWidth, ContentHeight});
        content_->SetPosition({0.0F, ContentViewportHeight - ContentHeight});

        languageLabel_ = &mrg::visual2d::CreateLabel(
            *content_, {24.0F, ContentHeight - 38.0F, PanelWidth - 48.0F, 28.0F}, L"",
            "Options.Language.Label");
        auto &languageText =
            RequireComponent<mrg::visual2d::TextVisualComponent>(*languageLabel_);
        languageText.SetFontSize(18.0F);
        languageText.SetTextColor(LabelColor);

        std::vector<std::wstring> languageNames;
        for (const texts::LanguageProfile &profile : texts_.Languages())
        {
            languageNames.emplace_back(profile.displayName);
        }
        languageCombo_ = &mrg::visual2d::CreateComboBox(
            *content_, {24.0F, ContentHeight - 96.0F, PanelWidth - 48.0F, 46.0F},
            std::move(languageNames), "Options.Language.Combo");
        languageComboId_ = languageCombo_->Id();
        languageCombo_->SetZIndex(10);
        mrg::visual2d::VisualStyle comboStyle{};
        comboStyle.normal = FieldColor;
        comboStyle.hovered = FieldHoverColor;
        comboStyle.pressed = {0.22F, 0.56F, 0.88F, 1.0F};
        comboStyle.disabled = {0.62F, 0.70F, 0.78F, 0.55F};
        RequireComponent<mrg::visual2d::SpriteVisualComponent>(*languageCombo_)
            .SetStyle(comboStyle);
        auto &combo =
            RequireComponent<mrg::visual2d::ComboBoxBehaviorComponent>(*languageCombo_);
        combo.SetMaxVisibleItems(2);
        combo.SetItemHeight(42.0F);
        combo.SetFontSize(19.0F);
        combo.SetTextColor(HeadingColor);
        combo.SetSelectedTextColor({1.0F, 1.0F, 1.0F, 1.0F});
        combo.SetPopupBackgroundColor(PanelColor);

        RefreshTexts();
        panel_->SetVisible(false);
    }

    void OptionsPanel::Shutdown() noexcept
    {
        languageCombo_ = nullptr;
        content_ = nullptr;
        languageLabel_ = nullptr;
        title_ = nullptr;
        panel_ = nullptr;
        panelId_ = {};
        languageComboId_ = {};
        previousDragPoint_.reset();
        animationProgress_ = 0.0F;
        scrollOffset_ = 0.0F;
        targetOpen_ = false;
        dragging_ = false;
    }

    void OptionsPanel::Toggle()
    {
        if (panel_ == nullptr)
        {
            return;
        }
        targetOpen_ = !targetOpen_;
        panel_->SetVisible(true);
        dragging_ = false;
        previousDragPoint_.reset();
    }

    void OptionsPanel::Update(const double deltaSeconds)
    {
        if (panel_ == nullptr)
        {
            return;
        }
        const float direction = targetOpen_ ? 1.0F : -1.0F;
        animationProgress_ = std::clamp(
            animationProgress_ + direction * static_cast<float>(deltaSeconds / AnimationSeconds),
            0.0F, 1.0F);
        ApplyAnimationPosition();
        if (!targetOpen_ && animationProgress_ <= 0.0F)
        {
            panel_->SetVisible(false);
        }
    }

    void OptionsPanel::ProcessInput(
        const mrg::platform::InputState &input,
        const std::optional<mrg::visual2d::Point> canvasPointer,
        const mrg::visual2d::NodeId hoveredNode,
        mrg::visual2d::Visual2DInputRouter &inputRouter)
    {
        if (!IsVisible() || panel_ == nullptr || !canvasPointer.has_value() ||
            !panel_->BoundsInCanvas().Contains(*canvasPointer))
        {
            dragging_ = false;
            previousDragPoint_.reset();
            return;
        }

        if (input.MouseWheelDelta() != 0.0F)
        {
            scrollOffset_ -= input.MouseWheelDelta() * 36.0F;
            ApplyScrollOffset(inputRouter);
        }

        const bool middlePressed =
            input.WasMouseButtonPressed(mrg::platform::MouseButton::Middle);
        const bool blankLeftPressed =
            hoveredNode == panelId_ &&
            input.WasMouseButtonPressed(mrg::platform::MouseButton::Left);
        if (middlePressed || blankLeftPressed)
        {
            dragging_ = true;
            previousDragPoint_ = canvasPointer;
        }
        if (dragging_ && previousDragPoint_.has_value())
        {
            scrollOffset_ += canvasPointer->y - previousDragPoint_->y;
            previousDragPoint_ = canvasPointer;
            ApplyScrollOffset(inputRouter);
        }
        if (input.WasMouseButtonReleased(mrg::platform::MouseButton::Middle) ||
            input.WasMouseButtonReleased(mrg::platform::MouseButton::Left))
        {
            dragging_ = false;
            previousDragPoint_.reset();
        }
    }

    bool OptionsPanel::ProcessAction(const mrg::visual2d::Action &action)
    {
        if (action.type != mrg::visual2d::ActionType::SelectionChanged ||
            action.source != languageComboId_)
        {
            return false;
        }
        const auto languages = texts_.Languages();
        if (action.selectedIndex >= languages.size())
        {
            return false;
        }
        texts_.SetLanguage(languages[action.selectedIndex].language);
        RefreshTexts();
        return true;
    }

    void OptionsPanel::RefreshTexts()
    {
        if (panel_ == nullptr)
        {
            return;
        }
        const texts::OptionTextSet &text = texts::Options(texts_.CurrentLanguage());
        auto &title = RequireComponent<mrg::visual2d::TextVisualComponent>(*title_);
        title.SetText(std::wstring(text.title));
        texts_.ApplyFont(title);
        auto &language = RequireComponent<mrg::visual2d::TextVisualComponent>(*languageLabel_);
        language.SetText(std::wstring(text.language));
        texts_.ApplyFont(language);
        auto &combo =
            RequireComponent<mrg::visual2d::ComboBoxBehaviorComponent>(*languageCombo_);
        texts_.ApplyFont(combo);
        const auto languages = texts_.Languages();
        for (std::size_t index = 0; index < languages.size(); ++index)
        {
            if (languages[index].language == texts_.CurrentLanguage())
            {
                combo.SetSelectedIndex(index);
                break;
            }
        }
    }

    bool OptionsPanel::IsVisible() const noexcept
    {
        return panel_ != nullptr && (targetOpen_ || animationProgress_ > 0.0F);
    }

    void OptionsPanel::ApplyAnimationPosition()
    {
        const float eased = animationProgress_ * animationProgress_ *
                            (3.0F - 2.0F * animationProgress_);
        panel_->SetBounds(
            {-PanelWidth * (1.0F - eased), -PanelHeight * 0.5F, PanelWidth, PanelHeight});
    }

    void OptionsPanel::ApplyScrollOffset(mrg::visual2d::Visual2DInputRouter &inputRouter)
    {
        const float maximum = std::max(ContentHeight - ContentViewportHeight, 0.0F);
        scrollOffset_ = std::clamp(scrollOffset_, 0.0F, maximum);
        if (content_ != nullptr)
        {
            content_->SetPosition(
                {0.0F, ContentViewportHeight - ContentHeight + scrollOffset_});
        }
        inputRouter.InvalidateHitTest();
    }
} // namespace finger_drum::presentation
