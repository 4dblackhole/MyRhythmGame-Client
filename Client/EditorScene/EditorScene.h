#pragma once

#include "MRG_Core.h"

class EditorScene final : public mrg::scene::GameScene
{
public:
    void Update(
        const mrg::UpdateContext& context,
        mrg::scene::SceneManager& scenes) override;
    void Render(const mrg::graphics::RenderContext& context) override;
};
