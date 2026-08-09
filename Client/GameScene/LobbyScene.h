#pragma once

#include "MRG_Core.h"

// Temporary destination for Game Start. The Engine clear pass supplies the
// empty AliceBlue frame until the real lobby UI is implemented.
class LobbyScene final : public mrg::scene::GameScene
{
public:
    void Update(
        const mrg::UpdateContext& context,
        mrg::scene::SceneManager& scenes) override;
    void Render(const mrg::graphics::RenderContext& context) override;
};
