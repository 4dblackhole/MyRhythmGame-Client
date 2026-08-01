#include "BlankScene.h"

#include "GameFlow/SceneIds.h"

#include <Windows.h>

#include <stdexcept>

void BlankScene::Update(
    const mrg::UpdateContext& context,
    mrg::scene::SceneManager& scenes)
{
    if (context.input.WasKeyPressed(VK_ESCAPE))
    {
        scenes.Quit();
        return;
    }

    if (context.input.WasKeyPressed(VK_SPACE) &&
        !scenes.ChangeScene(game::scene_ids::ColoredCube))
    {
        throw std::runtime_error("Failed to queue the next scene.");
    }
}

void BlankScene::Render(const mrg::graphics::RenderContext&)
{
}
