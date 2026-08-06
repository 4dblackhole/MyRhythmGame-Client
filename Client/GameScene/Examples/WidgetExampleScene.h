#pragma once

#include "MRG_Core.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// Demonstrates retained UI ownership and runtime child insertion/removal.
// The Canvas owns every widget; this Scene keeps stable IDs instead of raw
// pointers across mutations.
class WidgetExampleScene final : public mrg::scene::GameScene
{
public:
    void Initialize(const mrg::EngineServices& services) override;
    void Update(
        const mrg::UpdateContext& context,
        mrg::scene::SceneManager& scenes) override;
    void Render(const mrg::graphics::RenderContext& context) override;
    void OnResize(std::uint32_t width, std::uint32_t height) override;
    void Shutdown() noexcept override;

private:
    void BuildCanvas();
    void ProcessPointer(const mrg::platform::InputState& input);
    void ApplyUiActions();
    void AddDynamicWidget();
    void RemoveLastDynamicWidget();
    void RefreshControlState();
    void SetStatus(std::wstring text);
    [[nodiscard]] mrg::ui::UiPoint CanvasOrigin() const noexcept;
    [[nodiscard]] std::int64_t LatestPointerTimestamp(
        const mrg::platform::InputState& input) const noexcept;

    std::uint32_t width_{1280};
    std::uint32_t height_{720};
    std::unique_ptr<mrg::ui::UiCanvas> canvas_;
    mrg::ui::UiInputRouter inputRouter_;
    mrg::graphics::D3D12UiRenderer uiRenderer_;
    mrg::ui::UiElementId dynamicContainerId_{};
    mrg::ui::UiElementId addButtonId_{};
    mrg::ui::UiElementId removeButtonId_{};
    mrg::ui::UiElementId statusLabelId_{};
    std::vector<mrg::ui::UiElementId> dynamicWidgetIds_;
    std::uint32_t nextWidgetNumber_{1};
};
