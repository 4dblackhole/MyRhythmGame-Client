#pragma once

#include "PhantomCore.h"
#include "Texts/TextCatalog.h"
#include "App/OptionSettings.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

// Presents the YOUMU title screen. The authored image panels remain
// centered at every viewport aspect ratio, while one Canvas tree owns the
// mouse/keyboard menu and its movable selection cursor.
class LogoView;
class YOUMULogoScene final : public GameScene
{
  public:
    YOUMULogoScene(ScreenVisual2DManager &screenVisuals,
                        TextCatalog &texts,
                        OptionSettings &options);

    ~YOUMULogoScene() override;
    void Initialize(const EngineServices &services) override;
    void BeginScene() override;
    void EndScene() noexcept override;
    void Update(const UpdateContext &context, SceneManager &scenes) override;
    void Render(const RenderContext &context) override;
    void OnResize(std::uint32_t width, std::uint32_t height) override;
    void Shutdown() noexcept override;

  private:
    std::unique_ptr<LogoView> view_;
};
