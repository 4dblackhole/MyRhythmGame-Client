#include "GradientCubeScene.h"

#include "GameFlow/SceneIds.h"

#include <Windows.h>

#include <DirectXMath.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace DirectX;

namespace
{
    [[nodiscard]] XMFLOAT3 ThemeColor(
        const GradientCubeTheme theme) noexcept
    {
        switch (theme)
        {
        case GradientCubeTheme::Blue:
            return {0.08F, 0.24F, 1.0F};
        case GradientCubeTheme::Red:
            return {1.0F, 0.08F, 0.08F};
        case GradientCubeTheme::Green:
            return {0.08F, 1.0F, 0.22F};
        }
        return {1.0F, 1.0F, 1.0F};
    }

    [[nodiscard]] XMFLOAT4 GradientColor(
        const GradientCubeTheme theme,
        const XMFLOAT3& position,
        const float halfSideLength) noexcept
    {
        const float vertical = std::clamp(
            (position.y + halfSideLength) / (halfSideLength * 2.0F),
            0.0F,
            1.0F);
        const float depth = std::clamp(
            (position.z + halfSideLength) / (halfSideLength * 2.0F),
            0.0F,
            1.0F);
        const float intensity = 0.20F + vertical * 0.65F + depth * 0.15F;
        const XMFLOAT3 base = ThemeColor(theme);
        return {
            base.x * intensity,
            base.y * intensity,
            base.z * intensity,
            1.0F};
    }

    void AppendFace(
        std::vector<mrg::geometry::VertexAttributes>& vertices,
        std::vector<std::uint32_t>& indices,
        const std::array<XMFLOAT3, 4>& positions,
        const XMFLOAT3& normal,
        const GradientCubeTheme theme,
        const float halfSideLength)
    {
        constexpr std::array uvs{
            XMFLOAT2{0.0F, 1.0F},
            XMFLOAT2{0.0F, 0.0F},
            XMFLOAT2{1.0F, 0.0F},
            XMFLOAT2{1.0F, 1.0F}};
        const auto baseIndex = static_cast<std::uint32_t>(vertices.size());

        for (std::size_t index = 0; index < positions.size(); ++index)
        {
            vertices.push_back(mrg::geometry::VertexAttributes{
                positions[index],
                uvs[index],
                normal,
                GradientColor(theme, positions[index], halfSideLength)});
        }

        indices.insert(
            indices.end(),
            {
                baseIndex,
                baseIndex + 1,
                baseIndex + 2,
                baseIndex,
                baseIndex + 2,
                baseIndex + 3});
    }

    class GradientCubeShape final : public mrg::geometry::Shape
    {
    public:
        explicit GradientCubeShape(
            const GradientCubeTheme theme,
            const float sideLength = 1.5F)
        {
            if (sideLength <= 0.0F)
            {
                throw std::invalid_argument(
                    "Gradient cube side length must be positive.");
            }

            const float half = sideLength * 0.5F;
            std::vector<mrg::geometry::VertexAttributes> vertices;
            std::vector<std::uint32_t> indices;
            vertices.reserve(24);
            indices.reserve(36);

            AppendFace(
                vertices,
                indices,
                {{{-half, -half, -half},
                  {-half, half, -half},
                  {half, half, -half},
                  {half, -half, -half}}},
                {0.0F, 0.0F, -1.0F},
                theme,
                half);
            AppendFace(
                vertices,
                indices,
                {{{-half, -half, half},
                  {half, -half, half},
                  {half, half, half},
                  {-half, half, half}}},
                {0.0F, 0.0F, 1.0F},
                theme,
                half);
            AppendFace(
                vertices,
                indices,
                {{{-half, -half, half},
                  {-half, half, half},
                  {-half, half, -half},
                  {-half, -half, -half}}},
                {-1.0F, 0.0F, 0.0F},
                theme,
                half);
            AppendFace(
                vertices,
                indices,
                {{{half, -half, -half},
                  {half, half, -half},
                  {half, half, half},
                  {half, -half, half}}},
                {1.0F, 0.0F, 0.0F},
                theme,
                half);
            AppendFace(
                vertices,
                indices,
                {{{-half, half, -half},
                  {-half, half, half},
                  {half, half, half},
                  {half, half, -half}}},
                {0.0F, 1.0F, 0.0F},
                theme,
                half);
            AppendFace(
                vertices,
                indices,
                {{{-half, -half, half},
                  {-half, -half, -half},
                  {half, -half, -half},
                  {half, -half, half}}},
                {0.0F, -1.0F, 0.0F},
                theme,
                half);

            SetMeshData(std::move(vertices), std::move(indices));
        }
    };
}

GradientCubeScene::GradientCubeScene(
    const GradientCubeTheme theme,
    const std::size_t cubeCount)
    : theme_(theme),
      cubeCount_(cubeCount)
{
    if (cubeCount_ == 0 || cubeCount_ > cubes_.size())
    {
        throw std::invalid_argument(
            "A gradient cube scene requires between one and three cubes.");
    }
}

void GradientCubeScene::Initialize(const mrg::EngineServices& services)
{
    width_ = services.windowWidth;
    height_ = services.windowHeight;

    camera_.SetPosition(0.0F, 0.0F, -7.0F);
    camera_.SetPerspective(
        XMConvertToRadians(60.0F),
        height_ > 0
            ? static_cast<float>(width_) / static_cast<float>(height_)
            : 1.0F,
        0.1F,
        1000.0F);

    const GradientCubeShape shape(theme_);
    const mrg::graphics::GpuMeshHandle mesh =
        services.meshRendering.CreateMesh<
            mrg::geometry::VertexPositionColor>(shape);
    const mrg::graphics::MaterialInstanceHandle material =
        services.meshRendering.CreateMaterial(
            mrg::graphics::BuiltInMaterial::UnlitVertexColor);

    constexpr float spacing = 2.1F;
    const float left = -spacing *
        static_cast<float>(cubeCount_ - 1) * 0.5F;
    for (std::size_t index = 0; index < cubeCount_; ++index)
    {
        mrg::scene::MeshInstance& cube = cubes_[index];
        cube.SetMesh(mesh);
        cube.SetMaterial(material);
        cube.Transform().SetPosition(
            left + spacing * static_cast<float>(index),
            0.0F,
            0.0F);
    }
}

void GradientCubeScene::Update(
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
                "Failed to return from the gradient cube Scene.");
        }
        return;
    }

    const float seconds = static_cast<float>(context.totalSeconds);
    for (std::size_t index = 0; index < cubeCount_; ++index)
    {
        const float phase = static_cast<float>(index) * 0.65F;
        cubes_[index].Transform().SetRotationRollPitchYaw(
            seconds * 0.31F + phase,
            seconds * 0.57F + phase,
            seconds * -0.19F);
    }
}

void GradientCubeScene::Render(
    const mrg::graphics::RenderContext& context)
{
    for (std::size_t index = 0; index < cubeCount_; ++index)
    {
        cubes_[index].Submit(context, camera_);
    }
}

void GradientCubeScene::OnResize(
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

void GradientCubeScene::Shutdown() noexcept
{
    // MeshRenderSystem's in-flight frame resources retain any submitted GPU
    // handles, so this DestroyOnExit Scene can release its ownership now.
    for (mrg::scene::MeshInstance& cube : cubes_)
    {
        cube.Reset();
    }
}
