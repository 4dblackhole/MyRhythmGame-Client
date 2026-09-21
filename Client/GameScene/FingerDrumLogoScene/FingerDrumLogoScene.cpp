#include "FingerDrumLogoScene.h"
#include "Submodules/LogoView.h"
#include "GameFlow/FingerDrumSceneIds.h"
#include <stdexcept>

FingerDrumLogoScene::FingerDrumLogoScene(mrg::visual2d::ScreenVisual2DManager &visuals)
    : view_(std::make_unique<LogoView>(visuals))
{
}
FingerDrumLogoScene::~FingerDrumLogoScene() = default;
void FingerDrumLogoScene::Initialize(const mrg::EngineServices &services)
{
    view_->Initialize(services);
}
void FingerDrumLogoScene::BeginScene()
{
    view_->BeginScene();
}
void FingerDrumLogoScene::EndScene() noexcept
{
    view_->EndScene();
}
void FingerDrumLogoScene::Update(const mrg::UpdateContext &context,
                                 mrg::scene::SceneManager &scenes)
{
    const auto selected = view_->Update(context.input);
    if (!selected)
        return;
    const auto index = *selected;
    constexpr std::size_t GameStartIndex = 0, EditorIndex = 1, OptionIndex = 2, ExitIndex = 3;

    if (index == GameStartIndex)
    {
        if (!scenes.ChangeScene(finger_drum::scene_ids::Lobby))
        {
            throw std::runtime_error("Failed to enter the temporary FingerDrum Lobby Scene.");
        }
        return;
    }
    if (index == EditorIndex)
    {
        if (!scenes.ChangeScene(finger_drum::scene_ids::EditorSongSelect))
        {
            throw std::runtime_error("Failed to enter the editor song-select Scene.");
        }
        return;
    }
    if (index == OptionIndex)
    {
        return;
    }
    if (index == ExitIndex)
    {
        scenes.Quit();
    }
}
void FingerDrumLogoScene::Render(const mrg::graphics::RenderContext &)
{
}
void FingerDrumLogoScene::OnResize(std::uint32_t width, std::uint32_t height)
{
    view_->OnResize(width, height);
}
void FingerDrumLogoScene::Shutdown() noexcept
{
    view_->Shutdown();
}
