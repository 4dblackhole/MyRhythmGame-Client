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
    [[nodiscard]] std::int64_t LatestPointerTimestamp(
        const mrg::platform::InputState& input) const noexcept;

    std::uint32_t width_{1280};
    std::uint32_t height_{720};
    std::unique_ptr<mrg::visual2d::Visual2DCanvas> canvas_;
    mrg::visual2d::Visual2DInputRouter inputRouter_;
    mrg::graphics::D3D12Visual2DRenderer uiRenderer_;
    mrg::visual2d::NodeId dynamicContainerId_{};
    mrg::visual2d::NodeId addButtonId_{};
    mrg::visual2d::NodeId removeButtonId_{};
    mrg::visual2d::NodeId statusLabelId_{};
    std::vector<mrg::visual2d::NodeId> dynamicWidgetIds_;
    std::uint32_t nextWidgetNumber_{1};
};
