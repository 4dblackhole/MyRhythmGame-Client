#include "OptionsPanelStyle.h"
#include "Presentation/OptionsPanel.h"
#include <algorithm>

namespace finger_drum::presentation
{
    using namespace options_ui;
    void OptionsPanel::CreateDisplayControls()
    {
        resolutionLabel_ = &CreateFieldLabel(*content_, contentHeight_ - 694, PanelWidth, "Options.Resolution.Label");
        resolutionCombo_ = &mrg::visual2d::CreateComboBox(*content_, {24, contentHeight_ - 752, PanelWidth - 48, 46},
                                                          {}, "Options.Resolution.Combo");
        ConfigureCombo(*resolutionCombo_, 4);
        resolutionCombo_->SetZIndex(9);
    }

    void OptionsPanel::RefreshDisplayControls()
    {
        auto &label = RequireComponent<mrg::visual2d::TextVisualComponent>(*resolutionLabel_);
        label.SetText(std::wstring(texts::Options(texts_).resolution));
        texts_.ApplyFont(label);
        resolutions_.clear();
        const auto current =
            window_ ? window_->ClientSize()
                    : mrg::platform::WindowResolution{options_.Values().windowWidth, options_.Values().windowHeight};
        try
        {
            if (window_)
                resolutions_ = window_->DisplayResolutions();
        }
        catch (const std::exception &error)
        {
            status_ = Status::WindowFailed;
            statusDetail_ = error.what();
        }
        if (std::ranges::find(resolutions_, current) == resolutions_.end())
            resolutions_.push_back(current);
        std::vector<std::wstring> names;
        for (const auto size : resolutions_)
            names.push_back(std::to_wstring(size.width) + L" × " + std::to_wstring(size.height));
        auto &combo = RequireComponent<mrg::visual2d::ComboBoxBehaviorComponent>(*resolutionCombo_);
        combo.SetItems(std::move(names));
        combo.SetSelectedIndex(
            static_cast<std::size_t>(std::ranges::find(resolutions_, current) - resolutions_.begin()));
        texts_.ApplyFont(combo);
        resolutionCombo_->SetEnabled(window_ != nullptr);
    }

    void OptionsPanel::SelectResolution(std::size_t index)
    {
        if (!window_ || index >= resolutions_.size())
            return;
        const auto previous = window_->ClientSize();
        const auto size = resolutions_[index];
        if (!window_->SetClientSize(size, statusDetail_))
            status_ = Status::WindowFailed;
        else
        {
            auto candidate = options_.Values();
            candidate.windowWidth = size.width;
            candidate.windowHeight = size.height;
            if (!SaveOptions(candidate))
            {
                std::string error;
                if (!window_->SetClientSize(previous, error))
                    statusDetail_ += " | Restore failed: " + error;
            }
        }
        RefreshDisplayControls();
        RefreshStatus();
    }
} // namespace finger_drum::presentation
