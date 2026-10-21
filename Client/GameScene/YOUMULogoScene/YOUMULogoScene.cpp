#include "YOUMULogoScene.h"
#include "Submodules/LogoView.h"
#include "GameFlow/YOUMUSceneIds.h"
#include <stdexcept>

YOUMULogoScene::YOUMULogoScene(ScreenVisual2DManager &visuals,
                                         TextCatalog &texts,
                                         OptionSettings &options)
    : view_(std::make_unique<LogoView>(visuals, texts, options))
{
}
YOUMULogoScene::~YOUMULogoScene() = default;
void YOUMULogoScene::Initialize(const EngineServices &services)
{
    view_->Initialize(services);
}
void YOUMULogoScene::BeginScene()
{
    view_->BeginScene();
}
void YOUMULogoScene::EndScene() noexcept
{
    view_->EndScene();
}
void YOUMULogoScene::Update(const UpdateContext &context,
                                 SceneManager &scenes)
{
    const auto selected = view_->Update(context.input, context.deltaSeconds);
    if (!selected)
        return;
    const auto index = *selected;
    constexpr std::size_t GameStartIndex = 0, EditorIndex = 1, OptionIndex = 2, ExitIndex = 3;

    if (index == GameStartIndex)
    {
        if (!scenes.ChangeScene(LobbySceneId))
        {
            throw std::runtime_error("Failed to enter the song select Scene.");
        }
        return;
    }
    if (index == EditorIndex)
    {
        if (!scenes.ChangeScene(EditorSongSelectSceneId))
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
void YOUMULogoScene::Render(const RenderContext &)
{
}
void YOUMULogoScene::OnResize(std::uint32_t width, std::uint32_t height)
{
    view_->OnResize(width, height);
}
void YOUMULogoScene::Shutdown() noexcept
{
    view_->Shutdown();
}
