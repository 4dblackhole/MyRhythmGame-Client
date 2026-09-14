#include "EditorScene.h"

#include "GameFlow/FingerDrumSceneIds.h"

#include <Windows.h>

#include <stdexcept>

void EditorScene::Update(
    const mrg::UpdateContext& context,
    mrg::scene::SceneManager& scenes)
{
    // This initial empty workspace has no focusable controls. Once editor
    // controls are added, they must consume Escape before this route runs.
    if (context.input.WasKeyPressed(VK_ESCAPE) &&
        !scenes.ChangeScene(finger_drum::scene_ids::EditorSongSelect))
    {
        throw std::runtime_error(
            "Failed to return to the editor song selection scene.");
    }
}

void EditorScene::Render(const mrg::graphics::RenderContext&)
{
}
