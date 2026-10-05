#pragma once

#include "MRG_Core.h"
#include "Texts/TextCatalog.h"
#include "App/SkinSetSelection.h"
#include "App/OptionSettings.h"

#include <optional>
#include <vector>

namespace finger_drum::presentation
{
    class OptionsPanel final
    {
      public:
        OptionsPanel(texts::TextCatalog &texts, mrg_client::OptionSettings &options) noexcept
            : texts_(texts), options_(options) {}

        void Initialize(mrg::visual2d::Visual2DCanvas &canvas, mrg::audio::AudioSystem &audio);
        void Shutdown() noexcept;
        void Toggle();
        void Update(double deltaSeconds);
        void ProcessInput(const mrg::platform::InputState &input,
                          std::optional<mrg::visual2d::Point> canvasPointer,
                          mrg::visual2d::NodeId hoveredNode,
                          mrg::visual2d::Visual2DInputRouter &inputRouter);
        [[nodiscard]] bool ProcessAction(const mrg::visual2d::Action &action);
        void RefreshTexts();
        [[nodiscard]] bool IsVisible() const noexcept;

      private:
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
        static constexpr float ContentHeight = 860.0F;

        texts::TextCatalog &texts_;
        mrg_client::OptionSettings &options_;
        mrg::audio::AudioSystem *audio_{};
        enum class Status { None, SaveFailed, AudioFailed };
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
