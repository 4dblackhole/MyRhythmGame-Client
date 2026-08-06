#include "MeshExampleScene.h"

#include "GameFlow/SceneIds.h"

#include <Windows.h>

#include <DirectXMath.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace DirectX;

namespace
{
    // A Client-defined Shape is enough to introduce a new mesh type. The
    // engine converts these canonical attributes to the vertex layout chosen
    // in CreateMesh at compile time.
    class PyramidShape final : public mrg::geometry::Shape
    {
    public:
        PyramidShape()
        {
            std::vector<mrg::geometry::VertexAttributes> vertices{
                {{0.0F, 1.25F, 0.0F}, {}, {}, {1.0F, 0.25F, 0.15F, 1.0F}},
                {{-1.0F, -0.9F, -0.8F}, {}, {}, {1.0F, 0.85F, 0.15F, 1.0F}},
                {{1.0F, -0.9F, -0.8F}, {}, {}, {0.20F, 1.0F, 0.35F, 1.0F}},
                {{1.0F, -0.9F, 0.8F}, {}, {}, {0.20F, 0.55F, 1.0F, 1.0F}},
                {{-1.0F, -0.9F, 0.8F}, {}, {}, {0.75F, 0.25F, 1.0F, 1.0F}}};
            std::vector<std::uint32_t> indices{
                1, 0, 2,
                2, 0, 3,
                3, 0, 4,
                4, 0, 1,
                1, 2, 3,
                1, 3, 4};
            SetMeshData(std::move(vertices), std::move(indices));
        }
    };

    void ConfigureCamera(
        mrg::scene::Camera& camera,
        const std::uint32_t width,
        const std::uint32_t height)
    {
        camera.SetPosition(0.0F, 0.0F, -9.0F);
        camera.SetPerspective(
            XMConvertToRadians(55.0F),
            height > 0
                ? static_cast<float>(width) / static_cast<float>(height)
                : 1.0F,
            0.1F,
            1000.0F);
    }
}

void MeshExampleScene::Initialize(const mrg::EngineServices& services)
{
    width_ = services.windowWidth;
    height_ = services.windowHeight;
    ConfigureCamera(camera_, width_, height_);

    // Phase 1: create CPU-side Shape data, then select PositionColor as the
    // GPU vertex representation when each immutable mesh is uploaded.
    const mrg::geometry::RectangleShape rectangle(2.4F, 2.0F);
    const mrg::geometry::SphereShape sphere(1.15F, 32, 20);
    const PyramidShape pyramid;
    const std::array gpuMeshes{
        services.meshRendering.CreateMesh<
            mrg::geometry::VertexPositionColor>(rectangle),
        services.meshRendering.CreateMesh<
            mrg::geometry::VertexPositionColor>(sphere),
        services.meshRendering.CreateMesh<
            mrg::geometry::VertexPositionColor>(pyramid)};

    // Phase 2: a Material can be shared even though the instances reference
    // different meshes. Per-instance color and Transform remain independent.
    const mrg::graphics::MaterialInstanceHandle material =
        services.meshRendering.CreateMaterial(
            mrg::graphics::BuiltInMaterial::UnlitVertexColor);
    constexpr std::array positions{
        XMFLOAT3{-3.0F, 0.0F, 0.0F},
        XMFLOAT3{0.0F, 0.0F, 0.0F},
        XMFLOAT3{3.0F, 0.0F, 0.0F}};
    constexpr std::array colors{
        XMFLOAT4{0.18F, 0.72F, 1.0F, 1.0F},
        XMFLOAT4{1.0F, 0.32F, 0.72F, 1.0F},
        XMFLOAT4{1.0F, 1.0F, 1.0F, 1.0F}};

    for (std::size_t index = 0; index < meshes_.size(); ++index)
    {
        meshes_[index].SetMesh(gpuMeshes[index]);
        meshes_[index].SetMaterial(material);
        meshes_[index].SetColor(colors[index]);
        meshes_[index].Transform().SetPosition(
            positions[index].x,
            positions[index].y,
            positions[index].z);
    }

    textRendering_ = &services.textRendering;
    helpFont_ = services.textRendering.LoadSystemFont(L"Segoe UI");
}

void MeshExampleScene::Update(
    const mrg::UpdateContext& context,
    mrg::scene::SceneManager& scenes)
{
    if (context.input.WasKeyPressed(VK_ESCAPE))
    {
        scenes.Quit();
        return;
    }
    if (context.input.WasKeyPressed(VK_SPACE))
    {
        if (!scenes.ChangeScene(game::scene_ids::ColoredCube))
        {
            throw std::runtime_error(
                "Failed to return from the mesh example Scene.");
        }
        return;
    }

    animationSeconds_ += static_cast<float>(context.deltaSeconds);
    meshes_[0].Transform().SetRotationRollPitchYaw(
        0.0F,
        std::sin(animationSeconds_ * 0.8F) * 0.35F,
        0.0F);
    meshes_[1].Transform().SetScale(
        1.0F,
        1.0F + std::sin(animationSeconds_ * 1.7F) * 0.12F,
        1.0F);
    meshes_[2].Transform().SetRotationRollPitchYaw(
        animationSeconds_ * 0.22F,
        animationSeconds_ * 0.65F,
        0.0F);
}

void MeshExampleScene::Render(
    const mrg::graphics::RenderContext& context)
{
    // Submit only queues instance data. The engine batches and records the
    // actual draw calls during EndFrame.
    for (mrg::scene::MeshInstance& mesh : meshes_)
    {
        mesh.Submit(context, camera_);
    }
    SubmitHelpText();
}

void MeshExampleScene::OnResize(
    const std::uint32_t width,
    const std::uint32_t height)
{
    width_ = width;
    height_ = height;
    camera_.SetPerspectiveAspectRatio(
        height_ > 0
            ? static_cast<float>(width_) / static_cast<float>(height_)
            : 1.0F);
}

void MeshExampleScene::Shutdown() noexcept
{
    for (mrg::scene::MeshInstance& mesh : meshes_)
    {
        mesh.Reset();
    }
    helpFont_.reset();
    textRendering_ = nullptr;
}

void MeshExampleScene::SubmitHelpText() const
{
    if (textRendering_ == nullptr || helpFont_ == nullptr)
    {
        return;
    }

    mrg::graphics::TextDrawCommand title;
    title.positionPixels = {32.0F, 26.0F};
    title.layoutSizePixels = {900.0F, 42.0F};
    title.style.font = helpFont_;
    title.style.fontSizePixels = 26.0F;
    title.style.color = {0.06F, 0.18F, 0.36F, 1.0F};
    textRendering_->Submit(
        L"1  SHAPE -> GPU MESH -> MESH INSTANCE",
        title);

    mrg::graphics::TextDrawCommand hint = title;
    hint.positionPixels = {34.0F, 66.0F};
    hint.layoutSizePixels = {900.0F, 32.0F};
    hint.style.fontSizePixels = 18.0F;
    hint.style.color = {0.14F, 0.30F, 0.48F, 1.0F};
    textRendering_->Submit(
        L"RectangleShape / SphereShape / Client-defined PyramidShape   |   SPACE: BACK",
        hint);
}
