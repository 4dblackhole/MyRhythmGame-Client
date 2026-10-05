#include "OptionsPanel.h"

#include "Texts/Options/OptionTexts.h"
#include <Windows.h>

#include <algorithm>
#include <array>
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

        void ConfigureCombo(mrg::visual2d::Visual2DNode &node, const std::size_t visibleItems)
        {
            node.SetZIndex(10);
            mrg::visual2d::VisualStyle style{};
            style.normal = FieldColor;
            style.hovered = FieldHoverColor;
            style.pressed = {0.22F, 0.56F, 0.88F, 1.0F};
            style.disabled = {0.62F, 0.70F, 0.78F, 0.55F};
            RequireComponent<mrg::visual2d::SpriteVisualComponent>(node).SetStyle(style);
            auto &combo = RequireComponent<mrg::visual2d::ComboBoxBehaviorComponent>(node);
            combo.SetMaxVisibleItems(visibleItems);
            combo.SetItemHeight(42.0F);
            combo.SetFontSize(19.0F);
            combo.SetTextColor(HeadingColor);
            combo.SetSelectedTextColor({1.0F, 1.0F, 1.0F, 1.0F});
            combo.SetPopupBackgroundColor(PanelColor);
        }

        std::wstring WideText(const std::string &value)
        {
            if (value.empty()) return {};
            const int length = MultiByteToWideChar(CP_UTF8, 0, value.data(),
                                                   static_cast<int>(value.size()), nullptr, 0);
            std::wstring result(static_cast<std::size_t>(length), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
                                result.data(), length);
            return result;
        }

        mrg::visual2d::Visual2DNode &CreateFieldLabel(mrg::visual2d::Visual2DNode &parent,
                                                     float y, float width, std::string name)
        {
            auto &node = mrg::visual2d::CreateLabel(parent, {24.0F, y, width - 48.0F, 28.0F},
                                                   L"", std::move(name));
            auto &text = RequireComponent<mrg::visual2d::TextVisualComponent>(node);
            text.SetFontSize(18.0F);
            text.SetTextColor(LabelColor);
            return node;
        }

        std::wstring OutputName(mrg::audio::AudioOutputBackend output,
                                 const texts::OptionTextSet &text)
        {
            using mrg::audio::AudioOutputBackend;
            switch (output)
            {
            case AudioOutputBackend::Automatic: return std::wstring(text.automatic);
            case AudioOutputBackend::Wasapi: return L"WASAPI";
            case AudioOutputBackend::Asio: return L"ASIO";
            default: return std::wstring(text.noSound);
            }
        }

        constexpr std::array Outputs{mrg::audio::AudioOutputBackend::Automatic,
                                      mrg::audio::AudioOutputBackend::Wasapi,
                                      mrg::audio::AudioOutputBackend::Asio};
    } // namespace

    void OptionsPanel::Initialize(mrg::visual2d::Visual2DCanvas &canvas, mrg::audio::AudioSystem &audio)
    {
        audio_ = &audio;
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

        CreateLanguageControls();
        CreateSkinControls();
        CreateAudioControls();
        RefreshTexts();
        panel_->SetVisible(false);
    }

    void OptionsPanel::CreateLanguageControls()
    {
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
        ConfigureCombo(*languageCombo_, 2);
        languageCombo_->SetZIndex(50);
    }

    void OptionsPanel::CreateSkinControls()
    {
        skinLabel_ = &mrg::visual2d::CreateLabel(
            *content_, {24.0F, ContentHeight - 174.0F, PanelWidth - 48.0F, 28.0F}, L"",
            "Options.Skin.Label");
        auto &label = RequireComponent<mrg::visual2d::TextVisualComponent>(*skinLabel_);
        label.SetFontSize(18.0F);
        label.SetTextColor(LabelColor);

        skinNames_ = mrg_client::SkinSetSelection::Instance().AvailableNames();
        skinCombo_ = &mrg::visual2d::CreateComboBox(
            *content_, {24.0F, ContentHeight - 232.0F, PanelWidth - 48.0F, 46.0F},
            skinNames_, "Options.Skin.Combo");
        skinComboId_ = skinCombo_->Id();
        ConfigureCombo(*skinCombo_, 4);
        skinCombo_->SetZIndex(40);

        statusLabel_ = &mrg::visual2d::CreateLabel(
            *content_, {24.0F, ContentHeight - 784.0F, PanelWidth - 48.0F, 78.0F}, L"",
            "Options.Status");
        auto &status = RequireComponent<mrg::visual2d::TextVisualComponent>(*statusLabel_);
        status.SetFontSize(14.0F);
        status.SetTextColor({0.75F, 0.17F, 0.21F, 1.0F});
        statusLabel_->SetVisible(false);
    }

    void OptionsPanel::CreateAudioControls()
    {
        middlewareLabel_ = &CreateFieldLabel(*content_, ContentHeight - 310.0F, PanelWidth,
                                             "Options.Middleware.Label");
        outputLabel_ = &CreateFieldLabel(*content_, ContentHeight - 438.0F, PanelWidth,
                                         "Options.Output.Label");
        driverLabel_ = &CreateFieldLabel(*content_, ContentHeight - 566.0F, PanelWidth,
                                         "Options.Driver.Label");
        middlewareCombo_ = &mrg::visual2d::CreateComboBox(
            *content_, {24.0F, ContentHeight - 368.0F, PanelWidth - 48.0F, 46.0F},
            {L"FMOD"}, "Options.Middleware.Combo");
        outputCombo_ = &mrg::visual2d::CreateComboBox(
            *content_, {24.0F, ContentHeight - 496.0F, PanelWidth - 48.0F, 46.0F},
            {}, "Options.Output.Combo");
        driverCombo_ = &mrg::visual2d::CreateComboBox(
            *content_, {24.0F, ContentHeight - 624.0F, PanelWidth - 48.0F, 46.0F},
            {}, "Options.Driver.Combo");
        ConfigureCombo(*middlewareCombo_, 1);
        ConfigureCombo(*outputCombo_, 3);
        ConfigureCombo(*driverCombo_, 3);
        // Expanded lists must receive clicks before the fields underneath them.
        middlewareCombo_->SetZIndex(30);
        outputCombo_->SetZIndex(20);
        outputComboId_ = outputCombo_->Id();
        driverComboId_ = driverCombo_->Id();
    }

    void OptionsPanel::Shutdown() noexcept
    {
        skinNames_.clear();
        skinCombo_ = nullptr;
        statusLabel_ = nullptr;
        middlewareLabel_ = outputLabel_ = driverLabel_ = nullptr;
        middlewareCombo_ = outputCombo_ = driverCombo_ = nullptr;
        skinLabel_ = nullptr;
        languageCombo_ = nullptr;
        content_ = nullptr;
        languageLabel_ = nullptr;
        title_ = nullptr;
        panel_ = nullptr;
        panelId_ = {};
        languageComboId_ = {};
        skinComboId_ = {};
        outputComboId_ = driverComboId_ = {};
        driverIndices_.clear();
        previousDragPoint_.reset();
        animationProgress_ = 0.0F;
        scrollOffset_ = 0.0F;
        targetOpen_ = false;
        dragging_ = false;
        optionRevision_ = 0;
        audio_ = nullptr;
        status_ = Status::None;
        statusDetail_.clear();
    }

    void OptionsPanel::Toggle()
    {
        if (panel_ == nullptr)
        {
            return;
        }
        targetOpen_ = !targetOpen_;
        if (targetOpen_) RefreshSkinSets();
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
        if (optionRevision_ != options_.Revision()) RefreshTexts();
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
        if (panel_ == nullptr || action.type != mrg::visual2d::ActionType::SelectionChanged)
        {
            return false;
        }
        if (action.source == languageComboId_)
        {
            const auto languages = texts_.Languages();
            if (action.selectedIndex >= languages.size()) return false;
            auto candidate = options_.Values();
            candidate.language = languages[action.selectedIndex].language;
            if (SaveOptions(candidate)) texts_.SetLanguage(candidate.language);
            RefreshTexts();
            return true;
        }
        if (action.source == skinComboId_)
        {
            if (action.selectedIndex >= skinNames_.size()) return false;
            auto candidate = options_.Values();
            candidate.skinSet = skinNames_[action.selectedIndex];
            if (SaveOptions(candidate))
            {
                std::string error;
                if (!mrg_client::SkinSetSelection::Instance().Select(candidate.skinSet, error))
                {
                    status_ = Status::SaveFailed;
                    statusDetail_ = std::move(error);
                }
            }
            RefreshTexts();
            return true;
        }
        if (action.source == middlewareCombo_->Id()) return true; // FMOD is the sole installed middleware.
        if (action.source == outputComboId_ && action.selectedIndex < Outputs.size())
        {
            SelectOutput(Outputs[action.selectedIndex]);
            RefreshTexts();
            return true;
        }
        if (action.source == driverComboId_ && action.selectedIndex < driverIndices_.size())
        {
            SelectDriver(driverIndices_[action.selectedIndex]);
            RefreshTexts();
            return true;
        }
        return false;
    }

    void OptionsPanel::RefreshSkinSets()
    {
        skinNames_ = mrg_client::SkinSetSelection::Instance().AvailableNames();
        if (skinCombo_ == nullptr) return;
        auto &combo = RequireComponent<mrg::visual2d::ComboBoxBehaviorComponent>(*skinCombo_);
        combo.SetItems(skinNames_);
        RefreshTexts();
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
        auto &skin = RequireComponent<mrg::visual2d::TextVisualComponent>(*skinLabel_);
        skin.SetText(std::wstring(text.skin));
        texts_.ApplyFont(skin);
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
        auto &skinCombo =
            RequireComponent<mrg::visual2d::ComboBoxBehaviorComponent>(*skinCombo_);
        texts_.ApplyFont(skinCombo);
        const auto selected = std::ranges::find(
            skinNames_, mrg_client::SkinSetSelection::Instance().CurrentName());
        if (selected != skinNames_.end())
            skinCombo.SetSelectedIndex(static_cast<std::size_t>(selected - skinNames_.begin()));
        RefreshAudioControls();
        RefreshStatus();
        optionRevision_ = options_.Revision();
    }

    void OptionsPanel::RefreshAudioControls()
    {
        const auto &text = texts::Options(texts_.CurrentLanguage());
        const std::array labels{std::pair{middlewareLabel_, text.middleware},
                                 std::pair{outputLabel_, text.output},
                                 std::pair{driverLabel_, text.driver}};
        for (const auto &[node, value] : labels)
        {
            auto &label = RequireComponent<mrg::visual2d::TextVisualComponent>(*node);
            label.SetText(std::wstring(value));
            texts_.ApplyFont(label);
        }
        for (auto *node : {middlewareCombo_, outputCombo_, driverCombo_})
            texts_.ApplyFont(RequireComponent<mrg::visual2d::ComboBoxBehaviorComponent>(*node));

        auto &output = RequireComponent<mrg::visual2d::ComboBoxBehaviorComponent>(*outputCombo_);
        std::vector<std::wstring> names;
        for (auto backend : Outputs) names.push_back(OutputName(backend, text));
        output.SetItems(std::move(names));
        const auto requested = std::ranges::find(Outputs, audio_->RequestedOutput());
        if (requested != Outputs.end())
            output.SetSelectedIndex(static_cast<std::size_t>(requested - Outputs.begin()));

        names.clear();
        driverIndices_.clear();
        for (const auto &driver : audio_->OutputDrivers())
        {
            driverIndices_.push_back(driver.driverIndex);
            names.push_back(std::to_wstring(driver.driverIndex) + L": " + WideText(driver.name));
        }
        auto &driver = RequireComponent<mrg::visual2d::ComboBoxBehaviorComponent>(*driverCombo_);
        if (names.empty()) names.emplace_back(text.noDrivers);
        driver.SetItems(std::move(names));
        driverCombo_->SetEnabled(!driverIndices_.empty());
        const auto selected = std::ranges::find(driverIndices_, audio_->ActiveDriverIndex());
        if (selected != driverIndices_.end())
            driver.SetSelectedIndex(static_cast<std::size_t>(selected - driverIndices_.begin()));

    }

    bool OptionsPanel::SaveOptions(const mrg_client::OptionValues &candidate)
    {
        const bool saved = options_.Save(candidate, statusDetail_);
        status_ = saved ? Status::None : Status::SaveFailed;
        return saved;
    }

    std::string OptionsPanel::DriverName(const int index) const
    {
        for (const auto &driver : audio_->OutputDrivers())
            if (driver.driverIndex == index) return driver.name;
        return {};
    }

    void OptionsPanel::RestoreAudio(const mrg::audio::AudioOutputBackend output, const int driver,
                                    std::string &error)
    {
        std::string restorationError;
        if (!audio_->SetOutputBackend(output, restorationError) ||
            (driver >= 0 && !audio_->SetOutputDriver(driver, restorationError)))
            error += " | Could not restore the previous audio selection: " + restorationError;
    }

    void OptionsPanel::SelectOutput(const mrg::audio::AudioOutputBackend output)
    {
        const auto previousOutput = audio_->RequestedOutput();
        const int previousDriver = audio_->ActiveDriverIndex();
        auto candidate = options_.Values();
        candidate.audioOutput = output;
        if (!audio_->SetOutputBackend(output, statusDetail_))
        {
            RestoreAudio(previousOutput, previousDriver, statusDetail_);
            status_ = Status::AudioFailed;
            return;
        }
        candidate.driverIndex = audio_->ActiveDriverIndex();
        candidate.driverName = DriverName(candidate.driverIndex);
        if (!SaveOptions(candidate)) RestoreAudio(previousOutput, previousDriver, statusDetail_);
    }

    void OptionsPanel::SelectDriver(const int index)
    {
        const int previousDriver = audio_->ActiveDriverIndex();
        auto candidate = options_.Values();
        candidate.audioOutput = audio_->RequestedOutput();
        candidate.driverIndex = index;
        candidate.driverName = DriverName(index);
        if (!audio_->SetOutputDriver(index, statusDetail_))
        {
            status_ = Status::AudioFailed;
            return;
        }
        if (!SaveOptions(candidate))
            RestoreAudio(audio_->RequestedOutput(), previousDriver, statusDetail_);
    }

    void OptionsPanel::RefreshStatus()
    {
        const auto &text = texts::Options(texts_.CurrentLanguage());
        std::wstring value;
        const auto &saved = options_.Values();
        if (status_ == Status::SaveFailed) value = text.saveError;
        else if (status_ == Status::AudioFailed) value = text.audioError;
        else if (audio_->RequestedOutput() != saved.audioOutput)
            value = std::wstring(text.outputFallback) + L" " + OutputName(audio_->ActiveOutput(), text);
        else if ((!saved.driverName.empty() && saved.driverName != DriverName(audio_->ActiveDriverIndex())) ||
                 (saved.driverName.empty() && saved.driverIndex >= 0 && saved.driverIndex != audio_->ActiveDriverIndex()))
            value = text.driverUnavailable;
        if (!statusDetail_.empty()) value += L"\n" + WideText(statusDetail_);
        auto &label = RequireComponent<mrg::visual2d::TextVisualComponent>(*statusLabel_);
        label.SetText(value);
        texts_.ApplyFont(label);
        statusLabel_->SetVisible(!value.empty());
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
