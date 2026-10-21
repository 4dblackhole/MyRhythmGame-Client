#pragma once
#include "PhantomCore.h"
#include <string>

// A node owns its skin binding. Removing a virtualized song card also
// removes the binding; no external registry keeps dead node references.
class SkinVisualComponent final : public Visual2DComponent
{
  public:
    SkinVisualComponent(ScreenVisual2DManager &visuals, std::string scene, std::string key,
                        VisualStyle base, float radius);
    void Configure(std::string key, std::string decoration = {});
    void SetBaseStyle(const VisualStyle &style);
    void Update(double elapsedSeconds) override;

  private:
    bool Refresh();
    void Apply();
    ScreenVisual2DManager &visuals_;
    std::string scene_, key_, decoration_;
    VisualStyle base_, images_;
    float radius_{};
    std::uint64_t revision_{};
    bool loaded_{};
};
void BindSkinImage(Visual2DNode &node, ScreenVisual2DManager &visuals,
                   std::string scene, std::string key, std::string decoration = {});
// Dynamic selection styles update the saved procedural default as well.
void SetSkinBaseStyle(Visual2DNode &node, const VisualStyle &style);
