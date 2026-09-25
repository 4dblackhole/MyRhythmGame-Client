#pragma once

#include "MRG_Core.h"
#include "Texts/TextCatalog.h"
#include "App/SkinSetSelection.h"

#include <optional>
#include <vector>

namespace finger_drum::presentation
{
    class OptionsPanel final
    {
      public:
        explicit OptionsPanel(texts::TextCatalog &texts) noexcept : texts_(texts) {}

        void Initialize(mrg::visual2d::Visual2DCanvas &canvas);
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
        void RefreshSkinSets();

        static constexpr float PanelWidth = 320.0F;
        static constexpr float PanelHeight = 720.0F;
        static constexpr float AnimationSeconds = 0.2F;
        static constexpr float ContentViewportHeight = 608.0F;
        static constexpr float ContentHeight = 720.0F;

        texts::TextCatalog &texts_;
        mrg::visual2d::Visual2DNode *panel_{};
        mrg::visual2d::Visual2DNode *title_{};
        mrg::visual2d::Visual2DNode *languageLabel_{};
        mrg::visual2d::Visual2DNode *skinLabel_{};
        mrg::visual2d::Visual2DNode *skinStatus_{};
        mrg::visual2d::Visual2DNode *content_{};
        mrg::visual2d::Visual2DNode *languageCombo_{};
        mrg::visual2d::Visual2DNode *skinCombo_{};
        mrg::visual2d::NodeId panelId_{};
        mrg::visual2d::NodeId languageComboId_{};
        mrg::visual2d::NodeId skinComboId_{};
        std::vector<std::wstring> skinNames_;
        std::optional<mrg::visual2d::Point> previousDragPoint_;
        float animationProgress_{};
        float scrollOffset_{};
        bool targetOpen_{};
        bool dragging_{};
        bool skinSaveFailed_{};
    };
} // namespace finger_drum::presentation
