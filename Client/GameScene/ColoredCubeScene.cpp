#include "ColoredCubeScene.h"

#include "GameFlow/SceneIds.h"

#include <Windows.h>

#include <DirectXMath.h>

#include <array>
#include <filesystem>
#include <random>
#include <stdexcept>
#include <string_view>

using namespace DirectX;

namespace
{
    [[nodiscard]] std::filesystem::path RuntimeAssetPath(
        const wchar_t* fileName)
    {
        return mrg::platform::ResolveExecutableRelativePath(
            std::filesystem::path(L"assets") / fileName);
    }
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
    const float totalSeconds = static_cast<float>(context.totalSeconds);
    for (std::size_t index = 0; index < cubes_.size(); ++index)
    {
        const XMFLOAT3& initial = initialRotations_[index];
        const XMFLOAT3& velocity = angularVelocities_[index];
        cubes_[index].Transform().SetRotationRollPitchYaw(
            initial.x + velocity.x * totalSeconds,
            initial.y + velocity.y * totalSeconds,
            initial.z + velocity.z * totalSeconds);
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
