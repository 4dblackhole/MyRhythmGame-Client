#include "ColoredCubeScene.h"

#include "App/AssetPaths.h"
#include "Examples/ColoredCube/GameFlow/SceneIds.h"

#include <Windows.h>

#include <DirectXMath.h>

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using namespace DirectX;

namespace
{
    template <typename ComponentType>
    [[nodiscard]] ComponentType& RequireComponent(
        mrg::visual2d::Visual2DNode& node)
    {
        ComponentType* component = node.GetComponent<ComponentType>();
        if (component == nullptr)
        {
            throw std::logic_error("The Visual2D node is missing a component.");
        }
        return *component;
    }

    [[nodiscard]] std::wstring Utf8ToWide(const std::string_view value)
    {
        if (value.empty())
        {
            return {};
        }
        const int length = MultiByteToWideChar(
            CP_UTF8,
            0,
            value.data(),
            static_cast<int>(value.size()),
            nullptr,
            0);
        if (length <= 0)
        {
            return L"(unreadable device name)";
        }

        std::wstring result(static_cast<std::size_t>(length), L'\0');
        MultiByteToWideChar(
            CP_UTF8,
            0,
            value.data(),
            static_cast<int>(value.size()),
            result.data(),
            length);
        return result;
    }

    [[nodiscard]] std::wstring AudioBackendName(
        const mrg::audio::AudioOutputBackend backend)
    {
        switch (backend)
        {
        case mrg::audio::AudioOutputBackend::Automatic:
            return L"SYSTEM DEFAULT";
        case mrg::audio::AudioOutputBackend::Wasapi:
            return L"WASAPI";
        case mrg::audio::AudioOutputBackend::Asio:
            return L"ASIO";
        case mrg::audio::AudioOutputBackend::NoSound:
            return L"NO SOUND";
        default:
            return L"AUTO";
        }
    }

    [[nodiscard]] std::wstring AudioBackendSelectorName(
        const mrg::audio::AudioOutputBackend backend)
    {
        switch (backend)
        {
        case mrg::audio::AudioOutputBackend::Automatic:
            return L"SYSTEM DEFAULT (FMOD AUTO)";
        case mrg::audio::AudioOutputBackend::Wasapi:
            return L"WASAPI";
        case mrg::audio::AudioOutputBackend::Asio:
            return L"ASIO";
        default:
            return L"UNAVAILABLE";
        }
    }

    [[nodiscard]] std::wstring AudioDeviceSelectorName(
        const mrg::audio::AudioDeviceInfo& device)
    {
        if (device.driverIndex < 0)
        {
            return L"WINDOWS DEFAULT OUTPUT";
        }

        std::wstring result = L"[" +
            std::to_wstring(device.driverIndex) + L"] " +
            Utf8ToWide(device.name);
        if (device.sampleRate > 0)
        {
            result += L"  " + std::to_wstring(device.sampleRate) + L" Hz";
        }
        return result;
    }

    constexpr std::array AudioBackendChoices{
        mrg::audio::AudioOutputBackend::Automatic,
        mrg::audio::AudioOutputBackend::Wasapi,
        mrg::audio::AudioOutputBackend::Asio};

    constexpr std::array<std::uint32_t, 7> AudioBufferLengthChoices{
        64,
        128,
        256,
        512,
        1024,
        2048,
        4096};

    [[nodiscard]] std::optional<std::size_t> FindAudioBackendChoice(
        const mrg::audio::AudioOutputBackend backend) noexcept
    {
        for (std::size_t index = 0;
             index < AudioBackendChoices.size();
             ++index)
        {
            if (AudioBackendChoices[index] == backend)
            {
                return index;
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<std::size_t> FindAudioBufferLengthChoice(
        const std::uint32_t bufferLength) noexcept
    {
        for (std::size_t index = 0;
             index < AudioBufferLengthChoices.size();
             ++index)
        {
            if (AudioBufferLengthChoices[index] == bufferLength)
            {
                return index;
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] std::wstring AudioBufferLengthSelectorName(
        const std::uint32_t bufferLength)
    {
        std::wstring result = std::to_wstring(bufferLength) + L" SAMPLES";
        if (bufferLength == 256)
        {
            result += L" (DEFAULT)";
        }
        return result;
    }

    [[nodiscard]] mrg::visual2d::VisualStyle MakeWidgetStyle(
        const mrg::visual2d::Color normal,
        const mrg::visual2d::Color hovered,
        const mrg::visual2d::Color pressed,
        const mrg::visual2d::Color disabled) noexcept
    {
        mrg::visual2d::VisualStyle style{};
        style.normal = normal;
        style.hovered = hovered;
        style.pressed = pressed;
        style.disabled = disabled;
        return style;
    }

    [[nodiscard]] mrg::visual2d::VisualStyle MakeStaticStyle(
        const mrg::visual2d::Color color) noexcept
    {
        // Labels may have a readable background, but all interaction states
        // stay identical so they are not mistaken for clickable controls.
        return MakeWidgetStyle(color, color, color, color);
    }

    constexpr mrg::visual2d::Size AudioPanelSize{480.0F, 340.0F};
    constexpr float AudioPanelContentWidth = AudioPanelSize.width - 32.0F;
    constexpr float AudioPanelVisibleX = 20.0F;
    constexpr std::uint32_t OptionsCanvasZOrder = 0;
    constexpr std::uint32_t AudioCanvasZOrder = 1;
    constexpr std::int32_t ComboBoxPopupZIndex = 100;

    [[nodiscard]] constexpr mrg::visual2d::Rect TopLeftBounds(
        const mrg::visual2d::Rect bounds,
        const float parentHeight) noexcept
    {
        return {
            bounds.x,
            parentHeight - bounds.y - bounds.height,
            bounds.width,
            bounds.height};
    }
}

ColoredCubeScene::ColoredCubeScene(
    const bool startWithWorldSpaceUi) noexcept
    : worldSpaceUi_(startWithWorldSpaceUi),
      validateMultipleVisual2DPasses_(startWithWorldSpaceUi)
{
}

void ColoredCubeScene::Initialize(const mrg::EngineServices& services)
{
    width_ = services.windowWidth;
    height_ = services.windowHeight;

    camera_.SetPosition(0.0F, 0.0F, -6.0F);
    camera_.SetPerspective(
        XMConvertToRadians(60.0F),
        height_ > 0
            ? static_cast<float>(width_) / static_cast<float>(height_)
            : 1.0F,
        0.1F,
        1000.0F);

    // Each PNG remains an independently sized GPU Texture2D.  TextureSet is a
    // contiguous array of SRV descriptors, not a same-size Texture2DArray.
    const std::array texturePaths{
        mrg::platform::ResolveExecutableRelativePath(
            mrg_client::asset_paths::unused_examples::WhiteCubeTexture),
        mrg::platform::ResolveExecutableRelativePath(
            mrg_client::asset_paths::unused_examples::BlackCubeTexture)};
    const mrg::graphics::TextureSetHandle textures =
        services.meshRendering.Textures().LoadTextureSet(texturePaths);

    // Shape remains CPU/backend-neutral.  All cubes share one GPU mesh and one
    // material so the renderer can issue one four-instance draw call.
    const mrg::geometry::CubeShape cubeShape;
    const mrg::graphics::GpuMeshHandle cubeMesh =
        services.meshRendering.CreateMesh<
            mrg::geometry::VertexPositionUvColor>(cubeShape);
    const mrg::graphics::MaterialInstanceHandle material =
        services.meshRendering.CreateMaterial(
            mrg::graphics::BuiltInMaterial::
                UnlitVertexColorTextureArray);
    material->SetTextureSet(textures);

    std::random_device randomDevice;
    std::mt19937 randomEngine(randomDevice());
    std::uniform_real_distribution<float> positionJitter(-0.25F, 0.25F);
    std::uniform_real_distribution<float> depth(0.0F, 1.0F);
    std::array<std::uniform_real_distribution<float>, 3> scale(std::uniform_real_distribution < float>(0.65F, 1.0F), std::uniform_real_distribution < float>(0.65F, 1.0F));
    std::uniform_real_distribution<float> angle(
        -XM_PI,
        XM_PI);
    std::uniform_real_distribution<float> speed(0.35F, 0.8F);
    std::bernoulli_distribution rotationDirection(0.5);

    constexpr std::array basePositions{
        XMFLOAT2{-1.7F, 1.05F},
        XMFLOAT2{1.7F, 1.05F},
        XMFLOAT2{-1.7F, -1.05F},
        XMFLOAT2{1.7F, -1.05F}};

    for (std::size_t index = 0; index < cubes_.size(); ++index)
    {
        mrg::scene::MeshInstance& cube = cubes_[index];
        cube.SetMesh(cubeMesh);
        cube.SetMaterial(material);

        const float cubeScaleX = scale[0](randomEngine);
        const float cubeScaleY = scale[1](randomEngine);
        const float cubeScaleZ = scale[2](randomEngine);
        cube.Transform().SetScale(
            cubeScaleX,
            cubeScaleY,
            cubeScaleZ);
        cube.Transform().SetPosition(
            basePositions[index].x + positionJitter(randomEngine),
            basePositions[index].y + positionJitter(randomEngine),
            depth(randomEngine));

        initialRotations_[index] = {
            angle(randomEngine),
            angle(randomEngine),
            angle(randomEngine)};
        const float signedSpeed =
            speed(randomEngine) *
            (rotationDirection(randomEngine) ? 1.0F : -1.0F);
        angularVelocities_[index] = {
            signedSpeed * 0.37F,
            signedSpeed,
            signedSpeed * -0.21F};
    }

    // The first texture is square, while the second is 512x337.  Centered
    // cover transforms crop only the excess axis so every cube face is filled.
    cubes_[0].SetTextureIndex(0);
    cubes_[0].SetUvTransform(textures->MakeCoverUvTransform(0));
    cubes_[1].SetTextureIndex(1);
    cubes_[1].SetUvTransform(textures->MakeCoverUvTransform(1));
    cubes_[2].ClearTexture();
    cubes_[3].ClearTexture();

    InitializeOptionsUi(services);
    InitializeAudioOptionsUi(services);
}

void ColoredCubeScene::Update(
    const mrg::UpdateContext& context,
    mrg::scene::SceneManager& scenes)
{
    if (context.input.WasKeyPressed(VK_ESCAPE))
    {
        scenes.Quit();
        return;
    }

    const bool audioConsumedPointer = UpdateAudioOptionsUi(context);
    UpdateOptionsUi(context, !audioConsumedPointer);

    std::string_view selectedSceneId;
    if (context.input.WasKeyPressed(
        static_cast<std::uint16_t>('1')))
    {
        selectedSceneId = game::scene_ids::MeshExample;
    }
    else if (context.input.WasKeyPressed(
        static_cast<std::uint16_t>('2')))
    {
        selectedSceneId = game::scene_ids::CollisionExample;
    }
    else if (context.input.WasKeyPressed(
        static_cast<std::uint16_t>('3')))
    {
        selectedSceneId = game::scene_ids::WidgetExample;
    }

    if (!selectedSceneId.empty())
    {
        if (!scenes.ChangeScene(selectedSceneId))
        {
            throw std::runtime_error(
                "Failed to queue an engine feature example Scene route.");
        }
        return;
    }

    if (context.input.WasKeyPressed(VK_SPACE))
    {
        if (!scenes.ChangeScene(game::scene_ids::Blank))
        {
            throw std::runtime_error("Failed to queue the next scene.");
        }
        return;
    }

    UpdateCamera(context, audioConsumedPointer);
    if (rotationEnabled_)
    {
        animationSeconds_ += static_cast<float>(context.deltaSeconds) *
            rotationSpeedScale_;
    }
    for (std::size_t index = 0; index < cubes_.size(); ++index)
    {
        const XMFLOAT3& initial = initialRotations_[index];
        const XMFLOAT3& velocity = angularVelocities_[index];
        cubes_[index].Transform().SetRotationRollPitchYaw(
            initial.x + velocity.x * animationSeconds_,
            initial.y + velocity.y * animationSeconds_,
            initial.z + velocity.z * animationSeconds_);
    }
}

void ColoredCubeScene::Render(
    const mrg::graphics::RenderContext& context)
{
    // Submit is CPU-side only.  D3D12Renderer batches matching Mesh/Material
    // pairs and records DrawIndexedInstanced before EndFrame presents.
    for (mrg::scene::MeshInstance& cube : cubes_)
    {
        cube.Submit(context, camera_);
    }

    if (optionsUi_ != nullptr && !worldSpaceUi_)
    {
        context.visual2DRendering->SubmitScreen(
            optionsUi_->Canvas(),
            context,
            {20.0F, 20.0F},
            OptionsCanvasZOrder);
    }
    else if (optionsUi_ != nullptr && optionsCanvasTexture_ != nullptr)
    {
        // The complete Canvas, including images and DirectWrite glyphs, is
        // first drawn into a texture. The segmented mesh bends that texture;
        // MeshUvVisual2DSurface uses the same UVs for pointer input.
        context.visual2DRendering->RenderToTexture(
            optionsUi_->Canvas(),
            optionsCanvasTexture_,
            context);
        if (visual2DValidationTexture_ != nullptr)
        {
            context.visual2DRendering->RenderToTexture(
                optionsUi_->Canvas(),
                visual2DValidationTexture_,
                context);
        }
        curvedOptionsSurface_.Submit(context, camera_);
    }

    if (audioOptionsUi_ != nullptr &&
        audioPanelX_ > -AudioPanelSize.width)
    {
        context.visual2DRendering->SubmitScreen(
            *audioOptionsUi_,
            context,
            AudioPanelScreenOrigin(),
            AudioCanvasZOrder);
    }
}

void ColoredCubeScene::OnResize(
    const std::uint32_t width,
    const std::uint32_t height)
{
    width_ = width;
    height_ = height;
    if (camera_.ProjectionType() ==
        mrg::scene::CameraProjectionType::Perspective)
    {
        camera_.SetPerspectiveAspectRatio(
            height_ > 0
                ? static_cast<float>(width_) /
                    static_cast<float>(height_)
                : 1.0F);
    }
}

void ColoredCubeScene::Shutdown() noexcept
{
    // Engine has waited for submitted GPU work before SceneManager reaches
    // this callback, so releasing the mesh/material handles is safe.
    for (mrg::scene::MeshInstance& cube : cubes_)
    {
        cube.Reset();
    }
    curvedOptionsSurface_.Reset();
    visual2DValidationTexture_.reset();
    optionsCanvasTexture_.reset();
    optionsUi_.reset();
    audioOptionsUi_.reset();
    audioDevices_.clear();
    // AudioClip owns the FMOD sound. Release it before the engine-owned audio
    // system leaves scope.
    popSound_.reset();
    audioSystem_ = nullptr;
}

void ColoredCubeScene::InitializeOptionsUi(
    const mrg::EngineServices& services)
{
    XMStoreFloat4x4(
        &uiSurfaceWorld_,
        XMMatrixRotationY(XMConvertToRadians(14.0F)) *
            XMMatrixTranslation(0.0F, 0.0F, -0.65F));
    const mrg::geometry::CurvedRectangleShape curvedSurface(
        3.2F,
        2.1F,
        XMConvertToRadians(58.0F),
        40);
    optionsUi_ = std::make_unique<mrg::visual2d::WorldSpaceVisual2DCanvas>(
        mrg::visual2d::Size{320.0F, 210.0F},
        std::make_unique<mrg::visual2d::MeshUvVisual2DSurface>(
            curvedSurface,
            uiSurfaceWorld_));

    optionsCanvasTexture_ =
        services.visual2DRendering.CreateCanvasRenderTarget(960, 630);
    if (validateMultipleVisual2DPasses_)
    {
        visual2DValidationTexture_ =
            services.visual2DRendering.CreateCanvasRenderTarget(480, 315);
    }
    const mrg::graphics::GpuMeshHandle surfaceMesh =
        services.meshRendering.CreateMesh<
            mrg::geometry::VertexPositionUvColor>(curvedSurface);
    const mrg::graphics::MaterialInstanceHandle surfaceMaterial =
        services.meshRendering.CreateMaterial(
            mrg::graphics::BuiltInMaterial::
                UnlitVertexColorTextureArray);
    surfaceMaterial->SetTextureSet(optionsCanvasTexture_->Textures());
    curvedOptionsSurface_.SetMesh(surfaceMesh);
    curvedOptionsSurface_.SetMaterial(surfaceMaterial);
    curvedOptionsSurface_.SetTextureIndex(0);
    curvedOptionsSurface_.Transform().SetRotationRollPitchYaw(
        0.0F,
        XMConvertToRadians(14.0F),
        0.0F);
    curvedOptionsSurface_.Transform().SetPosition(0.0F, 0.0F, -0.65F);

    auto& panel = optionsUi_->Canvas().CreateNode(
        mrg::visual2d::Anchor::TopLeft,
        "Options.Panel");
    panel.SetBounds({0.0F, 0.0F, 320.0F, 210.0F});
    panel.AddComponent<mrg::visual2d::SpriteVisualComponent>().SetStyle({
        {0.055F, 0.075F, 0.11F, 0.94F},
        {0.065F, 0.085F, 0.12F, 0.94F},
        {0.055F, 0.075F, 0.11F, 0.94F},
        {0.055F, 0.075F, 0.11F, 0.60F}});

    auto& title = mrg::visual2d::CreateLabel(
        panel,
        TopLeftBounds({16.0F, 8.0F, 288.0F, 28.0F}, 210.0F),
        L"OPTIONS  [F2: SPACE]");
    auto& titleText = RequireComponent<mrg::visual2d::TextVisualComponent>(title);
    titleText.SetFontSize(19.0F);
    titleText.SetTextColor({0.62F, 0.84F, 1.0F, 1.0F});

    auto& rotation = mrg::visual2d::CreateToggle(
        panel,
        TopLeftBounds({16.0F, 44.0F, 288.0F, 36.0F}, 210.0F),
        L"CUBE ROTATION",
        true);
    RequireComponent<mrg::visual2d::SpriteVisualComponent>(rotation).SetStyle(
        MakeWidgetStyle(
        {0.075F, 0.145F, 0.285F, 0.98F},
        {0.13F, 0.285F, 0.52F, 1.0F},
        {0.035F, 0.085F, 0.19F, 1.0F},
        {0.035F, 0.065F, 0.12F, 0.58F}));
    rotationToggleId_ = rotation.Id();

    auto& speedLabel = mrg::visual2d::CreateLabel(
        panel,
        TopLeftBounds({16.0F, 90.0F, 124.0F, 28.0F}, 210.0F),
        L"ROTATION SPEED");
    RequireComponent<mrg::visual2d::TextVisualComponent>(speedLabel).
        SetFontSize(14.0F);

    auto& speed = mrg::visual2d::CreateSlider(
        panel,
        TopLeftBounds({148.0F, 90.0F, 156.0F, 28.0F}, 210.0F),
        0.2727F);
    RequireComponent<mrg::visual2d::SpriteVisualComponent>(speed).SetStyle(
        MakeWidgetStyle(
        {0.075F, 0.145F, 0.285F, 0.98F},
        {0.13F, 0.285F, 0.52F, 1.0F},
        {0.035F, 0.085F, 0.19F, 1.0F},
        {0.035F, 0.065F, 0.12F, 0.58F}));
    speedSliderId_ = speed.Id();

    auto& presentation = mrg::visual2d::CreateCycleSelector(
        panel,
        TopLeftBounds({16.0F, 128.0F, 288.0F, 40.0F}, 210.0F),
        {L"SCREEN SPACE", L"WORLD CURVED"});
    RequireComponent<mrg::visual2d::CycleSelectorBehaviorComponent>(
        presentation).SetSelectedIndex(worldSpaceUi_ ? 1 : 0);
    RequireComponent<mrg::visual2d::SpriteVisualComponent>(presentation).
        SetStyle(MakeWidgetStyle(
        {0.075F, 0.145F, 0.285F, 0.98F},
        {0.13F, 0.285F, 0.52F, 1.0F},
        {0.035F, 0.085F, 0.19F, 1.0F},
        {0.035F, 0.065F, 0.12F, 0.58F}));
    presentationComboId_ = presentation.Id();

    auto& examples = mrg::visual2d::CreateLabel(
        panel,
        TopLeftBounds({16.0F, 184.0F, 288.0F, 18.0F}, 210.0F),
        L"1:MESH   2:COLLISION   3:WIDGETS");
    auto& examplesText =
        RequireComponent<mrg::visual2d::TextVisualComponent>(examples);
    examplesText.SetFontSize(11.0F);
    examplesText.SetTextColor({0.68F, 0.78F, 0.92F, 1.0F});
}

void ColoredCubeScene::UpdateOptionsUi(
    const mrg::UpdateContext& context,
    const bool allowPointerInput)
{
    if (optionsUi_ == nullptr)
    {
        return;
    }
    optionsUi_->Canvas().Update(context.deltaSeconds);
    if (context.input.WasKeyPressed(VK_F2))
    {
        worldSpaceUi_ = !worldSpaceUi_;
        if (mrg::visual2d::Visual2DNode* node =
                optionsUi_->Canvas().FindNode(presentationComboId_))
        {
            if (auto* combo = node->GetComponent<
                    mrg::visual2d::CycleSelectorBehaviorComponent>())
            {
                combo->SetSelectedIndex(worldSpaceUi_ ? 1 : 0);
            }
        }
        uiInput_.Reset(optionsUi_->Canvas());
    }

    const mrg::visual2d::Point screenPointer{
        static_cast<float>(context.input.MousePositionX()),
        static_cast<float>(context.input.MousePositionY())};
    std::optional<mrg::visual2d::Point> canvasPointer;
    if (allowPointerInput && context.input.IsMouseInsideWindow())
    {
        if (!worldSpaceUi_)
        {
            canvasPointer = mrg::visual2d::MapScreenPointer(
                screenPointer,
                {static_cast<float>(width_), static_cast<float>(height_)},
                optionsUi_->Canvas(),
                {20.0F, 20.0F});
        }
        else
        {
            XMFLOAT4X4 viewProjection{};
            XMStoreFloat4x4(&viewProjection, camera_.ViewProjectionMatrix());
            const auto ray = mrg::visual2d::CreateWorldPointerRay(
                screenPointer,
                {static_cast<float>(width_), static_cast<float>(height_)},
                viewProjection);
            if (ray.has_value())
            {
                canvasPointer = optionsUi_->MapPointer(*ray);
            }
        }
    }

    mrg::visual2d::PointerInput pointer{};
    pointer.available = canvasPointer.has_value();
    pointer.position = canvasPointer.value_or(mrg::visual2d::Point{});
    pointer.leftButtonDown = context.input.IsMouseButtonDown(
        mrg::platform::MouseButton::Left);
    pointer.leftButtonPressed = context.input.WasMouseButtonPressed(
        mrg::platform::MouseButton::Left);
    pointer.leftButtonReleased = context.input.WasMouseButtonReleased(
        mrg::platform::MouseButton::Left);
    pointer.wheelDelta = context.input.MouseWheelDelta();
    pointer.timestampTicks = LatestPointerTimestamp(context.input);
    uiInput_.Process(optionsUi_->Canvas(), pointer);
    ApplyUiActions();
}

void ColoredCubeScene::ApplyUiActions()
{
    for (const mrg::visual2d::Action& action : optionsUi_->Canvas().TakeActions())
    {
        if (action.source == rotationToggleId_ &&
            action.type == mrg::visual2d::ActionType::ValueChanged)
        {
            rotationEnabled_ = action.value > 0.5F;
        }
        else if (action.source == speedSliderId_ &&
            action.type == mrg::visual2d::ActionType::ValueChanged)
        {
            rotationSpeedScale_ = 0.25F + action.value * 2.75F;
        }
        else if (action.source == presentationComboId_ &&
            action.type == mrg::visual2d::ActionType::SelectionChanged)
        {
            worldSpaceUi_ = action.selectedIndex == 1;
            uiInput_.Reset(optionsUi_->Canvas());
        }
    }
}

void ColoredCubeScene::InitializeAudioOptionsUi(
    const mrg::EngineServices& services)
{
    // The backend already enumerated the current output after FMOD finished
    // initializing. The Scene reads that snapshot without creating probes or
    // refreshing unrelated output APIs.
    audioSystem_ = &services.audio;
    const auto& initialDrivers = services.audio.OutputDrivers();
    audioDevices_.assign(initialDrivers.begin(), initialDrivers.end());
    // This Canvas owns only the panel's logical area. Screen placement and
    // slide animation are supplied as the Canvas origin during rendering and
    // pointer mapping, keeping its internal tree independent of the viewport.
    audioOptionsUi_ = std::make_unique<mrg::visual2d::Visual2DCanvas>(
        AudioPanelSize,
        mrg::visual2d::CanvasScaleMode::Fixed);

    // Widget2 is the readable dark base. Widget1 is a light, decorative
    // highlight layer so both user-provided frames can share one panel without
    // intercepting its controls.
    const mrg::visual2d::ImageHandle audioPanelBase =
        services.visual2DRendering.LoadImage(
            mrg_client::asset_paths::skin::widget::Second());
    const mrg::visual2d::ImageHandle audioPanelHighlight =
        services.visual2DRendering.LoadImage(
            mrg_client::asset_paths::skin::widget::First());
    const mrg::visual2d::VisualStyle titleStyle = MakeStaticStyle(
        {0.035F, 0.105F, 0.235F, 0.92F});
    const mrg::visual2d::VisualStyle sectionLabelStyle = MakeStaticStyle(
        {0.020F, 0.045F, 0.105F, 0.66F});
    const mrg::visual2d::VisualStyle controlStyle = MakeWidgetStyle(
        {0.055F, 0.120F, 0.265F, 0.96F},
        {0.105F, 0.245F, 0.470F, 0.98F},
        {0.035F, 0.080F, 0.195F, 0.98F},
        {0.030F, 0.065F, 0.135F, 0.62F});
    const mrg::visual2d::VisualStyle statusStyle = MakeStaticStyle(
        {0.025F, 0.145F, 0.110F, 0.92F});
    const mrg::visual2d::VisualStyle hintStyle = MakeStaticStyle(
        {0.020F, 0.035F, 0.090F, 0.84F});

    auto& panel = audioOptionsUi_->CreateNode(
        mrg::visual2d::Anchor::TopLeft,
        "AudioOptions.Panel");
    panel.SetBounds(
        {0.0F, 0.0F, AudioPanelSize.width, AudioPanelSize.height});
    mrg::visual2d::VisualStyle panelStyle{};
    panelStyle.normal = {1.0F, 1.0F, 1.0F, 1.0F};
    panelStyle.hovered = panelStyle.normal;
    panelStyle.pressed = panelStyle.normal;
    panelStyle.disabled = {1.0F, 1.0F, 1.0F, 0.65F};
    panelStyle.normalImage = audioPanelBase;
    panelStyle.hoveredImage = audioPanelBase;
    panelStyle.pressedImage = audioPanelBase;
    panelStyle.disabledImage = audioPanelBase;
    panel.AddComponent<mrg::visual2d::SpriteVisualComponent>().SetStyle(
        panelStyle);
    // The complete visible panel is one front-most input surface. Controls
    // remain preferred because child nodes are hit-tested before the parent.
    panel.AddComponent<mrg::visual2d::RectangleCollider2DComponent>();

    auto& panelHighlight = mrg::visual2d::CreateSprite(
        panel,
        TopLeftBounds(
            {0.0F, 0.0F, AudioPanelSize.width, AudioPanelSize.height},
            AudioPanelSize.height),
        audioPanelHighlight,
        "AudioOptions.Highlight");
    RequireComponent<mrg::visual2d::SpriteVisualComponent>(panelHighlight).
        SetTint({1.0F, 1.0F, 1.0F, 0.14F});

    auto& title = mrg::visual2d::CreateLabel(
        panel,
        TopLeftBounds(
            {16.0F, 10.0F, AudioPanelContentWidth, 30.0F},
            AudioPanelSize.height),
        L"AUDIO OUTPUT  [TAB: CLOSE]");
    auto& titleText = RequireComponent<mrg::visual2d::TextVisualComponent>(title);
    titleText.SetFontSize(18.0F);
    titleText.SetTextColor({0.58F, 0.86F, 1.0F, 1.0F});
    RequireComponent<mrg::visual2d::SpriteVisualComponent>(title).SetStyle(
        titleStyle);

    // The first ComboBox selects an output API. DirectSound is not offered:
    // FMOD 2.x no longer exposes a DirectSound output backend. Its closest
    // supported baseline is the explicit FMOD automatic/default path.
    auto& backendLabel = mrg::visual2d::CreateLabel(
        panel,
        TopLeftBounds(
            {16.0F, 48.0F, AudioPanelContentWidth, 16.0F},
            AudioPanelSize.height),
        L"OUTPUT API");
    auto& backendLabelText =
        RequireComponent<mrg::visual2d::TextVisualComponent>(backendLabel);
    backendLabelText.SetFontSize(12.0F);
    backendLabelText.SetTextColor({0.72F, 0.76F, 0.88F, 1.0F});
    RequireComponent<mrg::visual2d::SpriteVisualComponent>(backendLabel).
        SetStyle(sectionLabelStyle);

    std::vector<std::wstring> backendNames;
    backendNames.reserve(AudioBackendChoices.size());
    for (const mrg::audio::AudioOutputBackend backend : AudioBackendChoices)
    {
        backendNames.push_back(AudioBackendSelectorName(backend));
    }
    auto& backendCombo = mrg::visual2d::CreateCycleSelector(
        panel,
        TopLeftBounds(
            {16.0F, 68.0F, AudioPanelContentWidth, 36.0F},
            AudioPanelSize.height),
        std::move(backendNames));
    auto& backendBehavior = RequireComponent<
        mrg::visual2d::CycleSelectorBehaviorComponent>(backendCombo);
    selectedAudioBackend_ = services.audio.RequestedOutput();
    const std::optional<std::size_t> activeBackendChoice =
        FindAudioBackendChoice(selectedAudioBackend_);
    if (activeBackendChoice.has_value())
    {
        backendBehavior.SetSelectedIndex(*activeBackendChoice);
    }
    else
    {
        // No-sound/unknown output has no matching selectable API. Keep the
        // UI on the default path so the player can still choose a device.
        selectedAudioBackend_ = mrg::audio::AudioOutputBackend::Automatic;
    }
    RequireComponent<mrg::visual2d::TextVisualComponent>(backendCombo).
        SetFontSize(14.0F);
    RequireComponent<mrg::visual2d::SpriteVisualComponent>(backendCombo).
        SetStyle(controlStyle);
    audioBackendComboId_ = backendCombo.Id();

    auto& deviceLabel = mrg::visual2d::CreateLabel(
        panel,
        TopLeftBounds(
            {16.0F, 112.0F, AudioPanelContentWidth, 16.0F},
            AudioPanelSize.height),
        L"DEVICE");
    auto& deviceLabelText =
        RequireComponent<mrg::visual2d::TextVisualComponent>(deviceLabel);
    deviceLabelText.SetFontSize(12.0F);
    deviceLabelText.SetTextColor({0.72F, 0.76F, 0.88F, 1.0F});
    RequireComponent<mrg::visual2d::SpriteVisualComponent>(deviceLabel).
        SetStyle(sectionLabelStyle);

    auto& deviceCombo = mrg::visual2d::CreateComboBox(
        panel,
        TopLeftBounds(
            {16.0F, 132.0F, AudioPanelContentWidth, 38.0F},
            AudioPanelSize.height),
        {});
    auto& deviceBehavior = RequireComponent<
        mrg::visual2d::ComboBoxBehaviorComponent>(deviceCombo);
    deviceBehavior.SetFontSize(14.0F);
    deviceBehavior.SetMaxVisibleItems(4);
    deviceBehavior.SetItemHeight(36.0F);
    RequireComponent<mrg::visual2d::SpriteVisualComponent>(deviceCombo).
        SetStyle(controlStyle);
    // The popup is part of this element's subtree. Raising the element among
    // its panel siblings makes both drawing and hit testing follow one rule.
    deviceCombo.SetZIndex(ComboBoxPopupZIndex);
    audioDeviceComboId_ = deviceCombo.Id();

    auto& bufferLengthLabel = mrg::visual2d::CreateLabel(
        panel,
        TopLeftBounds(
            {16.0F, 178.0F, AudioPanelContentWidth, 16.0F},
            AudioPanelSize.height),
        L"DSP BUFFER LENGTH");
    auto& bufferLabelText = RequireComponent<
        mrg::visual2d::TextVisualComponent>(bufferLengthLabel);
    bufferLabelText.SetFontSize(12.0F);
    bufferLabelText.SetTextColor({0.72F, 0.76F, 0.88F, 1.0F});
    RequireComponent<mrg::visual2d::SpriteVisualComponent>(bufferLengthLabel).
        SetStyle(sectionLabelStyle);

    std::vector<std::wstring> bufferLengthNames;
    bufferLengthNames.reserve(AudioBufferLengthChoices.size());
    for (const std::uint32_t bufferLength : AudioBufferLengthChoices)
    {
        bufferLengthNames.push_back(
            AudioBufferLengthSelectorName(bufferLength));
    }
    auto& bufferLengthCombo = mrg::visual2d::CreateCycleSelector(
        panel,
        TopLeftBounds(
            {16.0F, 198.0F, AudioPanelContentWidth, 36.0F},
            AudioPanelSize.height),
        std::move(bufferLengthNames));
    RequireComponent<mrg::visual2d::TextVisualComponent>(bufferLengthCombo).
        SetFontSize(14.0F);
    RequireComponent<mrg::visual2d::SpriteVisualComponent>(bufferLengthCombo).
        SetStyle(controlStyle);
    audioBufferLengthComboId_ = bufferLengthCombo.Id();

    auto& status = mrg::visual2d::CreateLabel(
        panel,
        TopLeftBounds(
            {16.0F, 242.0F, AudioPanelContentWidth, 34.0F},
            AudioPanelSize.height),
        L"ACTIVE: " + AudioBackendName(services.audio.ActiveOutput()));
    auto& statusText = RequireComponent<mrg::visual2d::TextVisualComponent>(status);
    statusText.SetFontSize(15.0F);
    statusText.SetTextColor({0.62F, 1.0F, 0.72F, 1.0F});
    RequireComponent<mrg::visual2d::SpriteVisualComponent>(status).SetStyle(
        statusStyle);
    audioStatusLabelId_ = status.Id();
    auto& hint = mrg::visual2d::CreateLabel(
        panel,
        TopLeftBounds(
            {16.0F, 286.0F, AudioPanelContentWidth, 38.0F},
            AudioPanelSize.height),
        L"API: CLICK / DEVICE: DROPDOWN / DSP: CLICK / Z: PLAY pop.wav");
    auto& hintText = RequireComponent<mrg::visual2d::TextVisualComponent>(hint);
    hintText.SetFontSize(12.0F);
    hintText.SetTextColor({0.76F, 0.78F, 0.86F, 1.0F});
    RequireComponent<mrg::visual2d::SpriteVisualComponent>(hint).SetStyle(
        hintStyle);

    // Synchronize controls with the already running mixer without triggering
    // a device or DSP-buffer change during Scene initialization.
    RefreshAudioDeviceChoices();
    RefreshAudioBufferLengthChoice();

    std::string errorMessage;
    if (!ReloadPopSound(errorMessage))
    {
        throw std::runtime_error(
            "Failed to load the skin pop.wav: " + errorMessage);
    }
}

bool ColoredCubeScene::UpdateAudioOptionsUi(
    const mrg::UpdateContext& context)
{
    if (audioOptionsUi_ == nullptr)
    {
        return false;
    }
    audioOptionsUi_->Update(context.deltaSeconds);

    UpdateAudioPanelMotion(context);

    if (context.input.WasKeyPressed(static_cast<std::uint16_t>('Z')))
    {
        TryPlayPopSound(context.audio);
    }

    const std::optional<mrg::visual2d::Point> canvasPointer =
        MapAudioPanelPointer(context.input);
    const bool hadPointerCapture =
        audioUiInput_.CapturedNode() != 0;

    mrg::visual2d::PointerInput pointer{};
    pointer.available = canvasPointer.has_value();
    pointer.position = canvasPointer.value_or(mrg::visual2d::Point{});
    pointer.leftButtonDown = context.input.IsMouseButtonDown(
        mrg::platform::MouseButton::Left);
    pointer.leftButtonPressed = context.input.WasMouseButtonPressed(
        mrg::platform::MouseButton::Left);
    pointer.leftButtonReleased = context.input.WasMouseButtonReleased(
        mrg::platform::MouseButton::Left);
    pointer.wheelDelta = context.input.MouseWheelDelta();
    pointer.timestampTicks = LatestPointerTimestamp(context.input);
    audioUiInput_.Process(*audioOptionsUi_, pointer);
    ApplyAudioUiActions();

    // Only an actual node hit/capture in this panel-sized Canvas may block the
    // options Canvas and camera rendered behind it.
    return hadPointerCapture ||
        audioUiInput_.HoveredNode() != 0 ||
        audioUiInput_.CapturedNode() != 0;
}

void ColoredCubeScene::UpdateAudioPanelMotion(
    const mrg::UpdateContext& context)
{
    if (context.input.WasKeyPressed(VK_TAB))
    {
        audioPanelOpen_ = !audioPanelOpen_;
        audioUiInput_.Reset(*audioOptionsUi_);
    }

    const float targetX = audioPanelOpen_
        ? AudioPanelVisibleX
        : -AudioPanelSize.width - 2.0F;
    const float maximumStep =
        1150.0F * static_cast<float>(context.deltaSeconds);
    const float previousPanelX = audioPanelX_;
    audioPanelX_ += std::clamp(
        targetX - audioPanelX_,
        -maximumStep,
        maximumStep);
    if (audioPanelX_ == previousPanelX)
    {
        return;
    }

    // The Canvas origin moved under a potentially stationary pointer, so the
    // cached hover must be refreshed even though no node transform changed.
    audioUiInput_.InvalidateHitTest();
}

void ColoredCubeScene::TryPlayPopSound(mrg::audio::AudioSystem& audio)
{
    std::string errorMessage;
    if (popSound_ == nullptr)
    {
        SetAudioStatus(L"pop.wav IS NOT LOADED");
    }
    else if (!popSound_->Play(errorMessage))
    {
        SetAudioStatus(L"PLAY FAILED: " + Utf8ToWide(errorMessage));
    }
    else
    {
        SetAudioStatus(
            L"PLAYED VIA " + AudioBackendName(audio.ActiveOutput()));
    }
}

std::optional<mrg::visual2d::Point> ColoredCubeScene::MapAudioPanelPointer(
    const mrg::platform::InputState& input) const noexcept
{
    if (!input.IsMouseInsideWindow())
    {
        return std::nullopt;
    }
    const std::optional<mrg::visual2d::Point> canvasPointer =
        mrg::visual2d::MapScreenPointer(
        {
            static_cast<float>(input.MousePositionX()),
            static_cast<float>(input.MousePositionY())},
        {static_cast<float>(width_), static_cast<float>(height_)},
        *audioOptionsUi_,
        AudioPanelScreenOrigin());
    if (!canvasPointer.has_value() || audioUiInput_.CapturedNode() != 0)
    {
        return canvasPointer;
    }

    // Preserve out-of-panel positions only while a control owns pointer
    // capture; otherwise this minimal Canvas must not intercept the scene.
    if (canvasPointer->x < -AudioPanelSize.width * 0.5F ||
        canvasPointer->y < -AudioPanelSize.height * 0.5F ||
        canvasPointer->x > AudioPanelSize.width * 0.5F ||
        canvasPointer->y > AudioPanelSize.height * 0.5F)
    {
        return std::nullopt;
    }
    return canvasPointer;
}

mrg::visual2d::Point ColoredCubeScene::AudioPanelScreenOrigin()
    const noexcept
{
    return {
        audioPanelX_,
        (static_cast<float>(height_) - AudioPanelSize.height) * 0.5F};
}

void ColoredCubeScene::RefreshAudioDeviceChoices()
{
    if (audioOptionsUi_ == nullptr)
    {
        return;
    }
    mrg::visual2d::Visual2DNode* comboNode =
        audioOptionsUi_->FindNode(audioDeviceComboId_);
    auto* combo = comboNode != nullptr
        ? comboNode->GetComponent<mrg::visual2d::ComboBoxBehaviorComponent>()
        : nullptr;
    if (combo == nullptr || comboNode == nullptr)
    {
        return;
    }

    // The engine snapshot contains only the currently selected output API, so
    // the lower ComboBox no longer filters a process-wide multi-API list.
    std::vector<std::wstring> deviceNames;
    std::size_t activeSelection = 0;
    for (std::size_t index = 0; index < audioDevices_.size(); ++index)
    {
        const mrg::audio::AudioDeviceInfo& device = audioDevices_[index];
        if (audioSystem_ != nullptr &&
            device.driverIndex == audioSystem_->ActiveDriverIndex())
        {
            activeSelection = index;
        }
        deviceNames.push_back(AudioDeviceSelectorName(device));
    }

    if (deviceNames.empty())
    {
        combo->SetItems({L"NO DEVICE FOUND FOR THIS OUTPUT API"});
        comboNode->SetEnabled(false);
        return;
    }

    comboNode->SetEnabled(true);
    combo->SetItems(std::move(deviceNames));
    combo->SetSelectedIndex(activeSelection);
}

void ColoredCubeScene::RefreshAudioBufferLengthChoice()
{
    if (audioOptionsUi_ == nullptr || audioSystem_ == nullptr)
    {
        return;
    }
    mrg::visual2d::Visual2DNode* comboNode =
        audioOptionsUi_->FindNode(audioBufferLengthComboId_);
    auto* combo = comboNode != nullptr
        ? comboNode->GetComponent<
            mrg::visual2d::CycleSelectorBehaviorComponent>()
        : nullptr;
    if (combo == nullptr)
    {
        return;
    }

    const std::optional<std::size_t> selectedIndex =
        FindAudioBufferLengthChoice(audioSystem_->DspBufferLength());
    if (selectedIndex.has_value())
    {
        combo->SetSelectedIndex(*selectedIndex);
    }
}

void ColoredCubeScene::SelectAudioBackend(const std::size_t backendIndex)
{
    if (backendIndex >= AudioBackendChoices.size())
    {
        return;
    }

    const mrg::audio::AudioOutputBackend requested =
        AudioBackendChoices[backendIndex];
    std::string errorMessage;
    if (!audioSystem_->SetOutputBackend(requested, errorMessage))
    {
        SetAudioStatus(L"SWITCH FAILED: " + Utf8ToWide(errorMessage));
        selectedAudioBackend_ = audioSystem_->RequestedOutput();
        if (const auto activeChoice =
                FindAudioBackendChoice(selectedAudioBackend_);
            activeChoice.has_value())
        {
            if (mrg::visual2d::Visual2DNode* comboNode =
                    audioOptionsUi_->FindNode(audioBackendComboId_))
            {
                if (auto* combo = comboNode->GetComponent<
                        mrg::visual2d::CycleSelectorBehaviorComponent>())
                {
                    combo->SetSelectedIndex(*activeChoice);
                }
            }
        }
        return;
    }

    selectedAudioBackend_ = requested;
    const auto& drivers = audioSystem_->OutputDrivers();
    audioDevices_.assign(drivers.begin(), drivers.end());
    RefreshAudioDeviceChoices();
    RefreshAudioBufferLengthChoice();
    SetAudioStatus(
        L"ACTIVE: " + AudioBackendName(audioSystem_->ActiveOutput()) +
        L"  /  " + std::to_wstring(audioSystem_->DriverCount()) +
        L" DRIVER(S)");
}

void ColoredCubeScene::ApplyAudioDeviceSelection(
    const std::size_t audioDeviceIndex)
{
    if (audioSystem_ == nullptr || audioDeviceIndex >= audioDevices_.size())
    {
        return;
    }

    const mrg::audio::AudioDeviceInfo& selected =
        audioDevices_[audioDeviceIndex];
    std::string errorMessage;
    if (!audioSystem_->SetOutputDriver(selected.driverIndex, errorMessage))
    {
        SetAudioStatus(L"SWITCH FAILED: " + Utf8ToWide(errorMessage));
        RefreshAudioDeviceChoices();
        return;
    }

    SetAudioStatus(
        L"ACTIVE: " + AudioBackendName(audioSystem_->ActiveOutput()));
    RefreshAudioBufferLengthChoice();
}

void ColoredCubeScene::ApplyAudioBufferLengthSelection(
    const std::size_t bufferLengthIndex)
{
    if (audioSystem_ == nullptr ||
        bufferLengthIndex >= AudioBufferLengthChoices.size())
    {
        return;
    }

    const std::uint32_t requestedLength =
        AudioBufferLengthChoices[bufferLengthIndex];
    if (requestedLength == audioSystem_->DspBufferLength())
    {
        return;
    }

    // AudioSystem deliberately rejects mixer restarts while a Client-owned
    // clip exists. Release pop.wav before applying the new buffer length.
    popSound_.reset();
    std::string bufferError;
    const bool bufferChanged = audioSystem_->SetDspBufferSize(
        requestedLength,
        audioSystem_->DspBufferCount(),
        bufferError);

    std::string reloadError;
    const bool popReloaded = ReloadPopSound(reloadError);
    RefreshAudioBufferLengthChoice();

    if (!bufferChanged)
    {
        std::wstring status = L"BUFFER CHANGE FAILED: " +
            Utf8ToWide(bufferError);
        if (!popReloaded)
        {
            status += L" / pop.wav RELOAD FAILED: " + Utf8ToWide(reloadError);
        }
        SetAudioStatus(std::move(status));
        return;
    }
    if (!popReloaded)
    {
        SetAudioStatus(
            L"BUFFER CHANGED, pop.wav RELOAD FAILED: " +
            Utf8ToWide(reloadError));
        return;
    }

    SetAudioStatus(
        L"DSP BUFFER: " +
        std::to_wstring(audioSystem_->DspBufferLength()) +
        L" SAMPLES  /  " +
        std::to_wstring(audioSystem_->DspBufferCount()) + L" BLOCKS");
}

bool ColoredCubeScene::ReloadPopSound(std::string& errorMessage)
{
    popSound_.reset();
    if (audioSystem_ == nullptr)
    {
        errorMessage = "The audio system is unavailable.";
        return false;
    }

    popSound_ = audioSystem_->LoadSound(
        mrg_client::asset_paths::skin::TaikoHitSound(L"pop.wav"),
        errorMessage);
    return popSound_ != nullptr;
}

void ColoredCubeScene::ApplyAudioUiActions()
{
    for (const mrg::visual2d::Action& action : audioOptionsUi_->TakeActions())
    {
        if (action.type != mrg::visual2d::ActionType::SelectionChanged)
        {
            continue;
        }
        if (action.source == audioBackendComboId_)
        {
            SelectAudioBackend(action.selectedIndex);
            continue;
        }
        if (action.source == audioDeviceComboId_ &&
            action.selectedIndex < audioDevices_.size())
        {
            ApplyAudioDeviceSelection(action.selectedIndex);
            continue;
        }
        if (action.source == audioBufferLengthComboId_)
        {
            ApplyAudioBufferLengthSelection(action.selectedIndex);
        }
    }
}

void ColoredCubeScene::SetAudioStatus(std::wstring text)
{
    if (audioOptionsUi_ == nullptr)
    {
        return;
    }
    if (mrg::visual2d::Visual2DNode* status =
            audioOptionsUi_->FindNode(audioStatusLabelId_))
    {
        if (auto* label = status->GetComponent<
                mrg::visual2d::TextVisualComponent>())
        {
            label->SetText(std::move(text));
        }
    }
}

std::int64_t ColoredCubeScene::LatestPointerTimestamp(
    const mrg::platform::InputState& input) const noexcept
{
    std::int64_t result{};
    for (const mrg::platform::InputEvent& event : input.Events())
    {
        if (event.type == mrg::platform::InputEventType::MouseButtonPressed ||
            event.type == mrg::platform::InputEventType::MouseButtonReleased ||
            event.type == mrg::platform::InputEventType::MouseMoved)
        {
            result = event.performanceCounterTicks;
        }
    }
    return result;
}

void ColoredCubeScene::UpdateCamera(
    const mrg::UpdateContext& context,
    const bool suppressMouseWheel)
{
    constexpr float mouseSensitivity = 0.0025F;
    if (context.input.IsMouseButtonDown(
        mrg::platform::MouseButton::Right))
    {
        camera_.AddYawPitchRadians(
            static_cast<float>(context.input.MouseDeltaX()) *
                mouseSensitivity,
            -static_cast<float>(context.input.MouseDeltaY()) *
                mouseSensitivity);
    }

    const XMFLOAT3 forwardFloat = camera_.Forward();
    const XMFLOAT3 rightFloat = camera_.Right();
    const XMFLOAT3 upFloat = camera_.Up();
    const XMVECTOR forward = XMLoadFloat3(&forwardFloat);
    const XMVECTOR right = XMLoadFloat3(&rightFloat);
    const XMVECTOR up = XMLoadFloat3(&upFloat);

    const bool fast =
        context.input.IsKeyDown(VK_LSHIFT) ||
        context.input.IsKeyDown(VK_RSHIFT);
    const float movementSpeed = fast ? 10.0F : 4.0F;
    const float movement =
        movementSpeed * static_cast<float>(context.deltaSeconds);

    XMVECTOR position = XMLoadFloat3(&camera_.Position());
    if (context.input.IsKeyDown(
        static_cast<std::uint16_t>('W')))
    {
        position = XMVectorMultiplyAdd(
            forward,
            XMVectorReplicate(movement),
            position);
    }
    if (context.input.IsKeyDown(
        static_cast<std::uint16_t>('S')))
    {
        position = XMVectorNegativeMultiplySubtract(
            forward,
            XMVectorReplicate(movement),
            position);
    }
    if (context.input.IsKeyDown(
        static_cast<std::uint16_t>('D')))
    {
        position = XMVectorMultiplyAdd(
            right,
            XMVectorReplicate(movement),
            position);
    }
    if (context.input.IsKeyDown(
        static_cast<std::uint16_t>('A')))
    {
        position = XMVectorNegativeMultiplySubtract(
            right,
            XMVectorReplicate(movement),
            position);
    }
    if (context.input.IsKeyDown(
        static_cast<std::uint16_t>('E')))
    {
        position = XMVectorMultiplyAdd(
            up,
            XMVectorReplicate(movement),
            position);
    }
    if (context.input.IsKeyDown(
        static_cast<std::uint16_t>('Q')))
    {
        position = XMVectorNegativeMultiplySubtract(
            up,
            XMVectorReplicate(movement),
            position);
    }

    const float wheel = context.input.MouseWheelDelta();
    if (!suppressMouseWheel && wheel != 0.0F)
    {
        position = XMVectorMultiplyAdd(
            forward,
            XMVectorReplicate(wheel),
            position);
    }

    XMFLOAT3 updatedPosition{};
    XMStoreFloat3(&updatedPosition, position);
    camera_.SetPosition(updatedPosition);
}
