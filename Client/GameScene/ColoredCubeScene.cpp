#include "ColoredCubeScene.h"

#include "GameFlow/SceneIds.h"

#include <Windows.h>

#include <DirectXMath.h>

#include <algorithm>
#include <array>
#include <filesystem>
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
    [[nodiscard]] std::filesystem::path RuntimeAssetPath(
        const wchar_t* fileName)
    {
        return mrg::platform::ResolveExecutableRelativePath(
            std::filesystem::path(L"assets") / fileName);
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

    constexpr mrg::ui::UiSize AudioPanelSize{380.0F, 278.0F};
    constexpr float AudioPanelVisibleX = 20.0F;
    constexpr float AudioPanelY = 250.0F;
}

ColoredCubeScene::ColoredCubeScene(
    const bool startWithWorldSpaceUi) noexcept
    : worldSpaceUi_(startWithWorldSpaceUi)
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
        RuntimeAssetPath(L"awhc.png"),
        RuntimeAssetPath(L"bwhc.png")};
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
        selectedSceneId = game::scene_ids::BlueGradient;
    }
    else if (context.input.WasKeyPressed(
        static_cast<std::uint16_t>('2')))
    {
        selectedSceneId = game::scene_ids::RedGradient;
    }
    else if (context.input.WasKeyPressed(
        static_cast<std::uint16_t>('3')))
    {
        selectedSceneId = game::scene_ids::GreenGradient;
    }

    if (!selectedSceneId.empty())
    {
        if (!scenes.ChangeScene(selectedSceneId))
        {
            throw std::runtime_error(
                "Failed to queue a gradient cube Scene route.");
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

    UpdateCamera(context);
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
        uiRenderer_.SubmitScreen(optionsUi_->Canvas(), context, {20.0F, 20.0F});
    }
    else if (optionsUi_ != nullptr && optionsCanvasTexture_ != nullptr)
    {
        // The complete Canvas, including DirectWrite glyphs, is first drawn
        // into a texture. Sampling that texture on the segmented mesh bends
        // both rectangles and text with the exact same UV mapping used by
        // MeshUvUiSurface for pointer input.
        uiRenderer_.RenderToTexture(
            optionsUi_->Canvas(),
            optionsCanvasTexture_,
            context);
        curvedOptionsSurface_.Submit(context, camera_);
    }

    if (audioOptionsUi_ != nullptr &&
        audioPanelX_ > -AudioPanelSize.width)
    {
        uiRenderer_.SubmitScreen(
            *audioOptionsUi_,
            context,
            {audioPanelX_, AudioPanelY});
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
    optionsCanvasTexture_.reset();
    optionsUi_.reset();
    audioOptionsUi_.reset();
    audioDevices_.clear();
    if (audioSystem_ != nullptr &&
        popSound_ != mrg::audio::InvalidAudioSoundHandle)
    {
        audioSystem_->UnloadSound(popSound_);
    }
    popSound_ = mrg::audio::InvalidAudioSoundHandle;
    audioSystem_ = nullptr;
    uiRenderer_.Shutdown();
}

void ColoredCubeScene::InitializeOptionsUi(
    const mrg::EngineServices& services)
{
    uiRenderer_.Initialize(services.meshRendering, services.textRendering);

    XMStoreFloat4x4(
        &uiSurfaceWorld_,
        XMMatrixRotationY(XMConvertToRadians(14.0F)) *
            XMMatrixTranslation(0.0F, 0.0F, -0.65F));
    const mrg::geometry::CurvedRectangleShape curvedSurface(
        3.2F,
        2.1F,
        XMConvertToRadians(58.0F),
        40);
    optionsUi_ = std::make_unique<mrg::ui::WorldSpaceCanvas>(
        mrg::ui::UiSize{320.0F, 210.0F},
        std::make_unique<mrg::ui::MeshUvUiSurface>(
            curvedSurface,
            uiSurfaceWorld_));

    optionsCanvasTexture_ = uiRenderer_.CreateCanvasRenderTarget(960, 630);
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

    auto& panel = optionsUi_->Canvas().Root().EmplaceChild<mrg::ui::UiPanel>();
    panel.SetBounds({0.0F, 0.0F, 320.0F, 210.0F});
    panel.SetStyle({
        {0.055F, 0.075F, 0.11F, 0.94F},
        {0.065F, 0.085F, 0.12F, 0.94F},
        {0.055F, 0.075F, 0.11F, 0.94F},
        {0.055F, 0.075F, 0.11F, 0.60F}});

    auto& title = panel.EmplaceChild<mrg::ui::UiLabel>(L"OPTIONS  [F2: SPACE]");
    title.SetBounds({16.0F, 10.0F, 288.0F, 32.0F});
    title.SetFontSize(19.0F);
    title.SetTextColor({0.62F, 0.84F, 1.0F, 1.0F});

    auto& rotation = panel.EmplaceChild<mrg::ui::UiToggle>(
        L"CUBE ROTATION",
        true);
    rotation.SetBounds({16.0F, 50.0F, 288.0F, 40.0F});
    rotationToggleId_ = rotation.Id();

    auto& speedLabel = panel.EmplaceChild<mrg::ui::UiLabel>(L"ROTATION SPEED");
    speedLabel.SetBounds({16.0F, 94.0F, 132.0F, 34.0F});
    speedLabel.SetFontSize(15.0F);

    auto& speed = panel.EmplaceChild<mrg::ui::UiSlider>(0.2727F);
    speed.SetBounds({150.0F, 94.0F, 154.0F, 34.0F});
    speedSliderId_ = speed.Id();

    auto& presentation = panel.EmplaceChild<mrg::ui::UiComboBox>();
    presentation.SetItems({L"SCREEN SPACE", L"WORLD CURVED"});
    presentation.SetSelectedIndex(worldSpaceUi_ ? 1 : 0);
    presentation.SetBounds({16.0F, 142.0F, 288.0F, 48.0F});
    presentationComboId_ = presentation.Id();
}

void ColoredCubeScene::UpdateOptionsUi(
    const mrg::UpdateContext& context,
    const bool allowPointerInput)
{
    if (optionsUi_ == nullptr)
    {
        return;
    }
    if (context.input.WasKeyPressed(VK_F2))
    {
        worldSpaceUi_ = !worldSpaceUi_;
        if (auto* combo = dynamic_cast<mrg::ui::UiComboBox*>(
                optionsUi_->Canvas().FindElement(presentationComboId_)))
        {
            combo->SetSelectedIndex(worldSpaceUi_ ? 1 : 0);
        }
        uiInput_.Reset(optionsUi_->Canvas());
    }

    const mrg::ui::UiPoint screenPointer{
        static_cast<float>(context.input.MousePositionX()),
        static_cast<float>(context.input.MousePositionY())};
    std::optional<mrg::ui::UiPoint> canvasPointer;
    if (allowPointerInput && context.input.IsMouseInsideWindow())
    {
        if (!worldSpaceUi_)
        {
            canvasPointer = mrg::ui::MapScreenPointer(
                screenPointer,
                {static_cast<float>(width_), static_cast<float>(height_)},
                optionsUi_->Canvas().LogicalSize(),
                {20.0F, 20.0F});
        }
        else
        {
            XMFLOAT4X4 viewProjection{};
            XMStoreFloat4x4(&viewProjection, camera_.ViewProjectionMatrix());
            const auto ray = mrg::ui::CreateWorldPointerRay(
                screenPointer,
                {static_cast<float>(width_), static_cast<float>(height_)},
                viewProjection);
            if (ray.has_value())
            {
                canvasPointer = optionsUi_->MapPointer(*ray);
            }
        }
    }

    mrg::ui::UiPointerInput pointer{};
    pointer.available = canvasPointer.has_value();
    pointer.position = canvasPointer.value_or(mrg::ui::UiPoint{});
    pointer.leftButtonDown = context.input.IsMouseButtonDown(
        mrg::platform::MouseButton::Left);
    pointer.leftButtonPressed = context.input.WasMouseButtonPressed(
        mrg::platform::MouseButton::Left);
    pointer.leftButtonReleased = context.input.WasMouseButtonReleased(
        mrg::platform::MouseButton::Left);
    pointer.timestampTicks = LatestPointerTimestamp(context.input);
    uiInput_.Process(optionsUi_->Canvas(), pointer);
    ApplyUiActions();
}

void ColoredCubeScene::ApplyUiActions()
{
    for (const mrg::ui::UiAction& action : optionsUi_->Canvas().TakeActions())
    {
        if (action.source == rotationToggleId_ &&
            action.type == mrg::ui::UiActionType::ValueChanged)
        {
            rotationEnabled_ = action.value > 0.5F;
        }
        else if (action.source == speedSliderId_ &&
            action.type == mrg::ui::UiActionType::ValueChanged)
        {
            rotationSpeedScale_ = 0.25F + action.value * 2.75F;
        }
        else if (action.source == presentationComboId_ &&
            action.type == mrg::ui::UiActionType::SelectionChanged)
        {
            worldSpaceUi_ = action.selectedIndex == 1;
            uiInput_.Reset(optionsUi_->Canvas());
        }
    }
}

void ColoredCubeScene::InitializeAudioOptionsUi(
    const mrg::EngineServices& services)
{
    // Preserve the engine-owned service reference and take a snapshot of the
    // backend/device choices that this scene can present to the player.
    audioSystem_ = &services.audio;
    audioDevices_.assign(
        services.audio.OutputDevices().begin(),
        services.audio.OutputDevices().end());
    audioOptionsUi_ = std::make_unique<mrg::ui::UiCanvas>(AudioPanelSize);

    auto& panel = audioOptionsUi_->Root().EmplaceChild<mrg::ui::UiPanel>();
    panel.SetBounds({0.0F, 0.0F, AudioPanelSize.width, AudioPanelSize.height});
    panel.SetStyle({
        {0.045F, 0.055F, 0.085F, 0.97F},
        {0.055F, 0.070F, 0.105F, 0.97F},
        {0.035F, 0.045F, 0.070F, 0.97F},
        {0.045F, 0.055F, 0.085F, 0.70F}});

    auto& title = panel.EmplaceChild<mrg::ui::UiLabel>(
        L"AUDIO OUTPUT  [TAB: CLOSE]");
    title.SetBounds({16.0F, 10.0F, 348.0F, 32.0F});
    title.SetFontSize(18.0F);
    title.SetTextColor({0.58F, 0.86F, 1.0F, 1.0F});

    // The first ComboBox selects an output API. DirectSound is not offered:
    // FMOD 2.x no longer exposes a DirectSound output backend. Its closest
    // supported baseline is the explicit FMOD automatic/default path.
    auto& backendLabel = panel.EmplaceChild<mrg::ui::UiLabel>(L"OUTPUT API");
    backendLabel.SetBounds({16.0F, 42.0F, 348.0F, 18.0F});
    backendLabel.SetFontSize(12.0F);
    backendLabel.SetTextColor({0.72F, 0.76F, 0.88F, 1.0F});

    auto& backendCombo = panel.EmplaceChild<mrg::ui::UiComboBox>();
    std::vector<std::wstring> backendNames;
    backendNames.reserve(AudioBackendChoices.size());
    for (const mrg::audio::AudioOutputBackend backend : AudioBackendChoices)
    {
        backendNames.push_back(AudioBackendSelectorName(backend));
    }
    backendCombo.SetItems(std::move(backendNames));
    selectedAudioBackend_ = services.audio.ActiveOutput();
    const std::optional<std::size_t> activeBackendChoice =
        FindAudioBackendChoice(selectedAudioBackend_);
    if (activeBackendChoice.has_value())
    {
        backendCombo.SetSelectedIndex(*activeBackendChoice);
    }
    else
    {
        // No-sound/unknown output has no matching selectable API. Keep the
        // UI on the default path so the player can still choose a device.
        selectedAudioBackend_ = mrg::audio::AudioOutputBackend::Automatic;
    }
    backendCombo.SetBounds({16.0F, 60.0F, 348.0F, 38.0F});
    backendCombo.SetFontSize(14.0F);
    audioBackendComboId_ = backendCombo.Id();

    auto& deviceLabel = panel.EmplaceChild<mrg::ui::UiLabel>(L"DEVICE");
    deviceLabel.SetBounds({16.0F, 106.0F, 348.0F, 18.0F});
    deviceLabel.SetFontSize(12.0F);
    deviceLabel.SetTextColor({0.72F, 0.76F, 0.88F, 1.0F});

    auto& deviceCombo = panel.EmplaceChild<mrg::ui::UiComboBox>();
    deviceCombo.SetBounds({16.0F, 124.0F, 348.0F, 42.0F});
    deviceCombo.SetFontSize(14.0F);
    audioDeviceComboId_ = deviceCombo.Id();

    auto& status = panel.EmplaceChild<mrg::ui::UiLabel>(
        L"ACTIVE: " + AudioBackendName(services.audio.ActiveOutput()));
    status.SetBounds({16.0F, 176.0F, 348.0F, 34.0F});
    status.SetFontSize(15.0F);
    status.SetTextColor({0.62F, 1.0F, 0.72F, 1.0F});
    audioStatusLabelId_ = status.Id();

    auto& hint = panel.EmplaceChild<mrg::ui::UiLabel>(
        L"TOP: OUTPUT API   /   BOTTOM: DEVICE   /   Z: PLAY pop.wav");
    hint.SetBounds({16.0F, 220.0F, 348.0F, 38.0F});
    hint.SetFontSize(12.0F);
    hint.SetTextColor({0.76F, 0.78F, 0.86F, 1.0F});

    // Filter the lower ComboBox after all its controls exist. The active
    // driver is selected without changing the already running audio output.
    RefreshAudioDeviceChoices();

    std::string errorMessage;
    popSound_ = services.audio.LoadSound(
        RuntimeAssetPath(L"sounds\\pop.wav"),
        errorMessage);
    if (popSound_ == mrg::audio::InvalidAudioSoundHandle)
    {
        throw std::runtime_error(
            "Failed to load assets/sounds/pop.wav: " + errorMessage);
    }
}

bool ColoredCubeScene::UpdateAudioOptionsUi(
    const mrg::UpdateContext& context)
{
    if (audioOptionsUi_ == nullptr)
    {
        return false;
    }

    UpdateAudioPanelMotion(context);

    if (context.input.WasKeyPressed(static_cast<std::uint16_t>('Z')))
    {
        TryPlayPopSound(context.audio);
    }

    const std::optional<mrg::ui::UiPoint> canvasPointer =
        MapAudioPanelPointer(context.input);

    mrg::ui::UiPointerInput pointer{};
    pointer.available = canvasPointer.has_value();
    pointer.position = canvasPointer.value_or(mrg::ui::UiPoint{});
    pointer.leftButtonDown = context.input.IsMouseButtonDown(
        mrg::platform::MouseButton::Left);
    pointer.leftButtonPressed = context.input.WasMouseButtonPressed(
        mrg::platform::MouseButton::Left);
    pointer.leftButtonReleased = context.input.WasMouseButtonReleased(
        mrg::platform::MouseButton::Left);
    pointer.timestampTicks = LatestPointerTimestamp(context.input);
    audioUiInput_.Process(*audioOptionsUi_, pointer);
    ApplyAudioUiActions();
    return canvasPointer.has_value();
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
    audioPanelX_ += std::clamp(
        targetX - audioPanelX_,
        -maximumStep,
        maximumStep);
}

void ColoredCubeScene::TryPlayPopSound(mrg::audio::AudioSystem& audio)
{
    std::string errorMessage;
    if (popSound_ == mrg::audio::InvalidAudioSoundHandle)
    {
        SetAudioStatus(L"pop.wav IS NOT LOADED");
    }
    else if (!audio.PlaySound(popSound_, errorMessage))
    {
        SetAudioStatus(L"PLAY FAILED: " + Utf8ToWide(errorMessage));
    }
    else
    {
        SetAudioStatus(
            L"PLAYED VIA " + AudioBackendName(audio.ActiveOutput()));
    }
}

std::optional<mrg::ui::UiPoint> ColoredCubeScene::MapAudioPanelPointer(
    const mrg::platform::InputState& input) const noexcept
{
    if (!input.IsMouseInsideWindow())
    {
        return std::nullopt;
    }
    return mrg::ui::MapScreenPointer(
        {
            static_cast<float>(input.MousePositionX()),
            static_cast<float>(input.MousePositionY())},
        {static_cast<float>(width_), static_cast<float>(height_)},
        AudioPanelSize,
        {audioPanelX_, AudioPanelY});
}

void ColoredCubeScene::RefreshAudioDeviceChoices()
{
    if (audioOptionsUi_ == nullptr)
    {
        return;
    }
    auto* combo = dynamic_cast<mrg::ui::UiComboBox*>(
        audioOptionsUi_->FindElement(audioDeviceComboId_));
    if (combo == nullptr)
    {
        return;
    }

    // Rebuild the lower ComboBox from only the devices belonging to the
    // backend currently selected in the upper ComboBox.
    filteredAudioDeviceIndices_.clear();
    std::vector<std::wstring> deviceNames;
    std::size_t activeSelection = 0;
    for (std::size_t index = 0; index < audioDevices_.size(); ++index)
    {
        const mrg::audio::AudioDeviceInfo& device = audioDevices_[index];
        if (device.backend != selectedAudioBackend_)
        {
            continue;
        }

        if (audioSystem_ != nullptr &&
            device.backend == audioSystem_->ActiveOutput() &&
            device.driverIndex == audioSystem_->ActiveDriverIndex())
        {
            activeSelection = filteredAudioDeviceIndices_.size();
        }
        filteredAudioDeviceIndices_.push_back(index);
        deviceNames.push_back(AudioDeviceSelectorName(device));
    }

    if (deviceNames.empty())
    {
        combo->SetItems({L"NO DEVICE FOUND FOR THIS OUTPUT API"});
        combo->SetEnabled(false);
        return;
    }

    combo->SetEnabled(true);
    combo->SetItems(std::move(deviceNames));
    combo->SetSelectedIndex(activeSelection);
}

void ColoredCubeScene::SelectAudioBackend(const std::size_t backendIndex)
{
    if (backendIndex >= AudioBackendChoices.size())
    {
        return;
    }

    selectedAudioBackend_ = AudioBackendChoices[backendIndex];
    RefreshAudioDeviceChoices();
    if (filteredAudioDeviceIndices_.empty())
    {
        SetAudioStatus(L"NO DEVICE FOUND: " +
            AudioBackendName(selectedAudioBackend_));
        return;
    }

    // Selecting an API immediately applies its first device. This makes an
    // ASIO backend with only one driver selectable despite the current simple
    // ComboBox widget using click-to-cycle instead of a popup list.
    ApplyAudioDeviceSelection(filteredAudioDeviceIndices_.front());
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
    if (!audioSystem_->SelectOutputDevice(selected, errorMessage))
    {
        SetAudioStatus(L"SWITCH FAILED: " + Utf8ToWide(errorMessage));

        // The engine leaves the previous backend intact on failure. Restore
        // both ComboBoxes to that active backend/driver instead of displaying
        // a request that did not become effective.
        selectedAudioBackend_ = audioSystem_->ActiveOutput();
        if (const auto activeBackend =
                FindAudioBackendChoice(selectedAudioBackend_);
            activeBackend.has_value())
        {
            if (auto* backendCombo = dynamic_cast<mrg::ui::UiComboBox*>(
                    audioOptionsUi_->FindElement(audioBackendComboId_)))
            {
                backendCombo->SetSelectedIndex(*activeBackend);
            }
        }
        else
        {
            selectedAudioBackend_ = mrg::audio::AudioOutputBackend::Automatic;
        }
        RefreshAudioDeviceChoices();
        return;
    }

    SetAudioStatus(
        L"ACTIVE: " + AudioBackendName(audioSystem_->ActiveOutput()));
}

void ColoredCubeScene::ApplyAudioUiActions()
{
    for (const mrg::ui::UiAction& action : audioOptionsUi_->TakeActions())
    {
        if (action.type != mrg::ui::UiActionType::SelectionChanged)
        {
            continue;
        }
        if (action.source == audioBackendComboId_)
        {
            SelectAudioBackend(action.selectedIndex);
            continue;
        }
        if (action.source == audioDeviceComboId_ &&
            action.selectedIndex < filteredAudioDeviceIndices_.size())
        {
            ApplyAudioDeviceSelection(
                filteredAudioDeviceIndices_[action.selectedIndex]);
        }
    }
}

void ColoredCubeScene::SetAudioStatus(std::wstring text)
{
    if (audioOptionsUi_ == nullptr)
    {
        return;
    }
    if (auto* status = dynamic_cast<mrg::ui::UiLabel*>(
            audioOptionsUi_->FindElement(audioStatusLabelId_)))
    {
        status->SetText(std::move(text));
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
    const mrg::UpdateContext& context)
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
    if (wheel != 0.0F)
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
