#include "CollisionExampleScene.h"

#include "Examples/ColoredCube/GameFlow/SceneIds.h"

#include <Windows.h>

#include <DirectXMath.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace DirectX;

namespace
{
    constexpr XMFLOAT3 FixedBoxCenter{1.0F, 0.0F, 0.0F};
    constexpr XMFLOAT3 FixedBoxHalfExtents{1.1F, 1.1F, 1.1F};
    constexpr float MovingSphereRadius = 0.82F;
    constexpr float FixedBoxYaw = 0.48F;

    // CubeShape intentionally has demonstration corner colors. This wrapper
    // reuses its topology while replacing the canonical vertex colors so the
    // collision result can tint the complete rendered box uniformly.
    class SolidBoxShape final : public mrg::geometry::Shape
    {
    public:
        SolidBoxShape()
        {
            const mrg::geometry::CubeShape source(2.2F);
            std::vector<mrg::geometry::VertexAttributes> vertices(
                source.Vertices().begin(),
                source.Vertices().end());
            for (mrg::geometry::VertexAttributes& vertex : vertices)
            {
                vertex.color = {1.0F, 1.0F, 1.0F, 1.0F};
            }
            std::vector<std::uint32_t> indices(
                source.Indices().begin(),
                source.Indices().end());
            SetMeshData(std::move(vertices), std::move(indices));
        }
    };
}

void CollisionExampleScene::Initialize(
    const mrg::EngineServices& services)
{
    width_ = services.windowWidth;
    height_ = services.windowHeight;
    camera_.SetPosition(0.0F, 0.0F, -8.0F);
    camera_.SetPerspective(
        XMConvertToRadians(55.0F),
        height_ > 0
            ? static_cast<float>(width_) / static_cast<float>(height_)
            : 1.0F,
        0.1F,
        1000.0F);

    // Render data and collision data intentionally remain separate. The
    // Scene owns the mapping from each MeshInstance Transform to its collider.
    const mrg::geometry::SphereShape sphere(MovingSphereRadius, 32, 20);
    const SolidBoxShape box;
    const mrg::graphics::GpuMeshHandle sphereMesh =
        services.meshRendering.CreateMesh<
            mrg::geometry::VertexPositionColor>(sphere);
    const mrg::graphics::GpuMeshHandle boxMesh =
        services.meshRendering.CreateMesh<
            mrg::geometry::VertexPositionColor>(box);
    const mrg::graphics::MaterialInstanceHandle material =
        services.meshRendering.CreateMaterial(
            mrg::graphics::BuiltInMaterial::UnlitVertexColor);

    movingSphere_.SetMesh(sphereMesh);
    movingSphere_.SetMaterial(material);
    fixedBox_.SetMesh(boxMesh);
    fixedBox_.SetMaterial(material);
    fixedBox_.Transform().SetPosition(
        FixedBoxCenter.x,
        FixedBoxCenter.y,
        FixedBoxCenter.z);

    XMStoreFloat4(
        &fixedBoxOrientation_,
        XMQuaternionRotationRollPitchYaw(0.0F, FixedBoxYaw, 0.0F));
    fixedBox_.Transform().SetRotationQuaternion(
        fixedBoxOrientation_.x,
        fixedBoxOrientation_.y,
        fixedBoxOrientation_.z,
        fixedBoxOrientation_.w);

    textRendering_ = &services.textRendering;
    helpFont_ = services.textRendering.LoadSystemFont(L"Segoe UI");
    UpdateCollision(0.0);
}

void CollisionExampleScene::Update(
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
                "Failed to return from the collision example Scene.");
        }
        return;
    }

    // The sine motion runs by itself; Left/Right adds a persistent offset so
    // the same collision query can also be inspected interactively.
    constexpr float manualSpeed = 2.5F;
    const float movement =
        manualSpeed * static_cast<float>(context.deltaSeconds);
    if (context.input.IsKeyDown(VK_LEFT))
    {
        manualOffset_ -= movement;
    }
    if (context.input.IsKeyDown(VK_RIGHT))
    {
        manualOffset_ += movement;
    }
    if (context.input.WasKeyPressed(static_cast<std::uint16_t>('R')))
    {
        manualOffset_ = 0.0F;
    }
    manualOffset_ = std::clamp(manualOffset_, -3.0F, 3.0F);
    UpdateCollision(context.totalSeconds);
}

void CollisionExampleScene::UpdateCollision(const double totalSeconds)
{
    const float movingX =
        std::sin(static_cast<float>(totalSeconds) * 0.9F) * 3.2F +
        manualOffset_;
    constexpr XMFLOAT3 movingBase{-1.0F, 0.0F, 0.0F};
    const XMFLOAT3 movingCenter{
        movingBase.x + movingX,
        movingBase.y,
        movingBase.z};

    movingSphere_.Transform().SetPosition(
        movingCenter.x,
        movingCenter.y,
        movingCenter.z);

    const mrg::collision::Sphere3D sphereCollider{
        movingCenter,
        MovingSphereRadius};
    const mrg::collision::Obb3D boxCollider{
        FixedBoxCenter,
        FixedBoxHalfExtents,
        fixedBoxOrientation_};
    colliding_ = mrg::collision::Intersects(sphereCollider, boxCollider);

    const XMFLOAT4 hitColor{1.0F, 0.12F, 0.12F, 1.0F};
    movingSphere_.SetColor(
        colliding_
            ? hitColor
            : XMFLOAT4{0.10F, 0.46F, 1.0F, 1.0F});
    fixedBox_.SetColor(
        colliding_
            ? hitColor
            : XMFLOAT4{0.12F, 0.82F, 0.34F, 1.0F});
}

void CollisionExampleScene::Render(
    const mrg::graphics::RenderContext& context)
{
    movingSphere_.Submit(context, camera_);
    fixedBox_.Submit(context, camera_);
    SubmitHelpText();
}

void CollisionExampleScene::OnResize(
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

void CollisionExampleScene::Shutdown() noexcept
{
    movingSphere_.Reset();
    fixedBox_.Reset();
    helpFont_.reset();
    textRendering_ = nullptr;
}

void CollisionExampleScene::SubmitHelpText() const
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
    title.style.color = colliding_
        ? XMFLOAT4{0.88F, 0.06F, 0.06F, 1.0F}
        : XMFLOAT4{0.06F, 0.40F, 0.20F, 1.0F};
    textRendering_->Submit(
        colliding_
            ? L"2  SPHERE vs OBB: COLLISION"
            : L"2  SPHERE vs OBB: SEPARATED",
        title);

    mrg::graphics::TextDrawCommand hint = title;
    hint.positionPixels = {34.0F, 66.0F};
    hint.layoutSizePixels = {900.0F, 32.0F};
    hint.style.fontSizePixels = 18.0F;
    hint.style.color = {0.14F, 0.30F, 0.48F, 1.0F};
    textRendering_->Submit(
        L"LEFT / RIGHT: OFFSET   |   R: RESET   |   SPACE: BACK",
        hint);
}
