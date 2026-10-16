#pragma once
#include "MRG_Core.h"
#include <string>

namespace finger_drum::presentation
{
    // A node owns its skin binding. Removing a virtualized song card also
    // removes the binding; no external registry keeps dead node references.
    class SkinVisualComponent final : public mrg::visual2d::Visual2DComponent
    {
      public:
        SkinVisualComponent(mrg::visual2d::ScreenVisual2DManager &visuals, std::string scene, std::string key,
                            mrg::visual2d::VisualStyle base, float radius);
        void Configure(std::string key, std::string decoration = {});
        void SetBaseStyle(const mrg::visual2d::VisualStyle &style);
        void Update(double elapsedSeconds) override;

      private:
        bool Refresh();
        void Apply();
        mrg::visual2d::ScreenVisual2DManager &visuals_;
        std::string scene_, key_, decoration_;
        mrg::visual2d::VisualStyle base_, images_;
        float radius_{};
        std::uint64_t revision_{};
        bool loaded_{};
    };
    void BindSkinImage(mrg::visual2d::Visual2DNode &node, mrg::visual2d::ScreenVisual2DManager &visuals,
                       std::string scene, std::string key, std::string decoration = {});
    // Dynamic selection styles update the saved procedural default as well.
    void SetSkinBaseStyle(mrg::visual2d::Visual2DNode &node, const mrg::visual2d::VisualStyle &style);
} // namespace finger_drum::presentation
