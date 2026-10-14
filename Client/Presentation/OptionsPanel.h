#pragma once

#include "App/OptionSettings.h"
#include "App/SkinSetSelection.h"
#include "MRG_Core.h"
#include "Mode/ModeInputLayout.h"
#include "Texts/TextCatalog.h"

#include <optional>
#include <vector>

namespace finger_drum::presentation
{
    class OptionsPanel final
    {
      public:
        OptionsPanel(texts::TextCatalog &texts, mrg_client::OptionSettings &options) noexcept
            : texts_(texts), options_(options) {}

        void Initialize(mrg::visual2d::Visual2DCanvas &canvas, mrg::audio::AudioSystem &audio,
                        mrg::platform::IWindowControl *window = nullptr);
        void Shutdown() noexcept;
        void Toggle();
        void Update(double deltaSeconds);
        // True consumes a panel keyboard command, including the closing frame.
        bool ProcessInput(const mrg::platform::InputState &input, std::optional<mrg::visual2d::Point> canvasPointer,
                          mrg::visual2d::NodeId hoveredNode, mrg::visual2d::Visual2DInputRouter &inputRouter);
        bool HandleKey(std::uint16_t virtualKey, mrg::visual2d::Visual2DInputRouter &inputRouter);
        [[nodiscard]] bool ProcessAction(const mrg::visual2d::Action &action);
        void RefreshTexts();
        [[nodiscard]] bool OwnsKeyboard() const noexcept
        {
            return captureRow_.has_value() || editingOffset_;
        }
        [[nodiscard]] bool IsVisible() const noexcept;

      private:
        void CreateDisplayControls();
        void RefreshDisplayControls();
        void SelectResolution(std::size_t index);
        void CreateKeyControls();
        void RefreshKeyControls();
        void CaptureKey(std::uint16_t key);
        void CreateOffsetControls();
        void RefreshOffsetControls();
        void EditOffset(std::uint16_t key);
        void ApplyOffset();
        void CancelKeyboardEdit();
        bool ProcessFieldAction(const mrg::visual2d::Action &action);
        void ApplyAnimationPosition();
        void ApplyScrollOffset(mrg::visual2d::Visual2DInputRouter &inputRouter);
        void CreateLanguageControls();
        void CreateSkinControls();
        void CreateAudioControls();
        void RefreshAudioControls();
        void RefreshStatus();
        void RefreshSkinSets();
        bool SaveOptions(const mrg_client::OptionValues &candidate);
        void SelectOutput(mrg::audio::AudioOutputBackend output);
        void SelectDriver(int index);
        void RestoreAudio(mrg::audio::AudioOutputBackend output, int driver, std::string &error);
        [[nodiscard]] std::string DriverName(int index) const;

        static constexpr float PanelWidth = 320.0F;
        static constexpr float PanelHeight = 720.0F;
        static constexpr float AnimationSeconds = 0.2F;
        static constexpr float ContentViewportHeight = 608.0F;
        float contentHeight_{};
        struct KeyRow
        {
            std::string mode, binding, displayName;
            std::size_t slot{};
            mrg::visual2d::Visual2DNode *label{}, *button{};
        };
        std::vector<mode::ModeInputLayout> inputLayouts_;
        std::vector<KeyRow> keyRows_;
        std::vector<mrg::visual2d::Visual2DNode *> modeLabels_;
        std::vector<mrg::platform::WindowResolution> resolutions_;
        mrg::platform::IWindowControl *window_{};
        mrg::visual2d::Visual2DNode *resolutionLabel_{}, *resolutionCombo_{};
        mrg::visual2d::Visual2DNode *offsetLabel_{}, *offsetField_{}, *offsetHint_{};
        std::optional<std::size_t> captureRow_;
        std::wstring offsetDraft_;
        bool editingOffset_{}, replaceOffsetDraft_{};

        texts::TextCatalog &texts_;
        mrg_client::OptionSettings &options_;
        mrg::audio::AudioSystem *audio_{};
        mrg::visual2d::Visual2DCanvas *canvas_{};
        enum class Status
        {
            None,
            SaveFailed,
            AudioFailed,
            WindowFailed,
            InvalidInput
        };
        Status status_{};
        std::string statusDetail_;
        mrg::visual2d::Visual2DNode *panel_{};
        mrg::visual2d::Visual2DNode *title_{};
        mrg::visual2d::Visual2DNode *languageLabel_{};
        mrg::visual2d::Visual2DNode *skinLabel_{};
        mrg::visual2d::Visual2DNode *statusLabel_{};
        mrg::visual2d::Visual2DNode *middlewareLabel_{};
        mrg::visual2d::Visual2DNode *outputLabel_{};
        mrg::visual2d::Visual2DNode *driverLabel_{};
        mrg::visual2d::Visual2DNode *content_{};
        mrg::visual2d::Visual2DNode *languageCombo_{};
        mrg::visual2d::Visual2DNode *skinCombo_{};
        mrg::visual2d::Visual2DNode *middlewareCombo_{};
        mrg::visual2d::Visual2DNode *outputCombo_{};
        mrg::visual2d::Visual2DNode *driverCombo_{};
        mrg::visual2d::NodeId panelId_{};
        mrg::visual2d::NodeId languageComboId_{};
        mrg::visual2d::NodeId skinComboId_{};
        mrg::visual2d::NodeId outputComboId_{};
        mrg::visual2d::NodeId driverComboId_{};
        std::vector<int> driverIndices_;
        std::vector<std::wstring> skinNames_;
        std::optional<mrg::visual2d::Point> previousDragPoint_;
        float animationProgress_{};
        float scrollOffset_{};
        bool targetOpen_{};
        bool dragging_{};
        std::uint64_t optionRevision_{};
    };
} // namespace finger_drum::presentation
