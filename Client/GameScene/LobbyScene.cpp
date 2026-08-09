#include "LobbyScene.h"

#include "GameFlow/FingerDrumSceneIds.h"
#include "Presentation/MarqueeTextComponent.h"

#include <Windows.h>

#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <utility>

namespace
{
    constexpr float PenpotScale = 2.0F / 3.0F;
    constexpr mrg::visual2d::Color CanvasBlue{0.918F, 0.965F, 1.0F, 1.0F};
    constexpr mrg::visual2d::Color PanelWhite{0.976F, 0.992F, 1.0F, 0.96F};
    constexpr mrg::visual2d::Color PureWhite{1.0F, 1.0F, 1.0F, 1.0F};
    constexpr mrg::visual2d::Color AccentBlue{0.310F, 0.624F, 0.878F, 1.0F};
    constexpr mrg::visual2d::Color LightBlue{0.780F, 0.918F, 0.980F, 1.0F};
    constexpr mrg::visual2d::Color DeepBlue{0.133F, 0.306F, 0.459F, 1.0F};
    constexpr mrg::visual2d::Color MutedBlue{0.412F, 0.537F, 0.635F, 1.0F};

    [[nodiscard]] mrg::visual2d::Rect Scale(
        const mrg::visual2d::Rect bounds) noexcept
    {
        return {
            bounds.x * PenpotScale,
            bounds.y * PenpotScale,
            bounds.width * PenpotScale,
            bounds.height * PenpotScale};
    }

    [[nodiscard]] std::filesystem::path ProfileImagePath()
    {
        return mrg::platform::ResolveExecutableRelativePath(
            L"assets\\images\\profile\\TemporaryPilot.png");
    }

    template <typename ComponentType>
    [[nodiscard]] ComponentType& RequireComponent(
        mrg::visual2d::Visual2DNode& node)
    {
        ComponentType* const component = node.GetComponent<ComponentType>();
        if (component == nullptr)
        {
            throw std::logic_error(
                "A song-select node is missing a required component.");
        }
        return *component;
    }

    void ApplyFlatStyle(
        mrg::visual2d::Visual2DNode& node,
        const mrg::visual2d::Color color)
    {
        mrg::visual2d::VisualStyle style{};
        style.normal = color;
        style.hovered = color;
        style.pressed = color;
        style.disabled = {color.red, color.green, color.blue, 0.45F};
        RequireComponent<mrg::visual2d::SpriteVisualComponent>(node).
            SetStyle(style);
    }
}

void LobbyScene::Initialize(const mrg::EngineServices& services)
{
    width_ = services.windowWidth;
    height_ = services.windowHeight;
    canvas_ = std::make_unique<mrg::visual2d::Visual2DCanvas>(
        mrg::visual2d::Size{1280.0F, 720.0F},
        mrg::visual2d::CanvasScaleMode::FixedHeight);
    canvas_->SetViewportSize({
        static_cast<float>(width_),
        static_cast<float>(height_)});

    // Penpot의 1920x1080 보드를 하나의 중앙 정렬 트리로 유지한다.
    // 실제 데이터가 생기기 전까지 모든 내용 패널은 empty state만 갖는다.
    board_ = &canvas_->CreateNode(
        mrg::visual2d::Anchor::Center,
        "FingerDrum.SongSelect.Board");
    board_->SetPivot({0.5F, 0.5F});
    board_->SetSize({1280.0F, 720.0F});
    board_->SetPosition({0.0F, 0.0F});

    AddPanel(
        *board_,
        {0.0F, 0.0F, 1920.0F, 1080.0F},
        CanvasBlue,
        "Background");
    CreateHeader(services);
    CreateCategoryBar();
    CreateRecordPanel();
    CreateSongInformationPanel();
    CreatePatternPanel();
    CreateSongList();
    CreateFooter();
}

void LobbyScene::Update(
    const mrg::UpdateContext& context,
    mrg::scene::SceneManager& scenes)
{
    if (canvas_ == nullptr)
    {
        return;
    }

    canvas_->Update(context.deltaSeconds);
    if (context.input.WasKeyPressed(VK_ESCAPE) &&
        !scenes.ChangeScene(finger_drum::scene_ids::Logo))
    {
        throw std::runtime_error("Failed to return to the FingerDrum Logo.");
    }
}

void LobbyScene::Render(const mrg::graphics::RenderContext& context)
{
    if (canvas_ != nullptr && context.visual2DRendering != nullptr)
    {
        context.visual2DRendering->SubmitScreen(*canvas_, context);
    }
}

void LobbyScene::OnResize(
    const std::uint32_t width,
    const std::uint32_t height)
{
    width_ = width;
    height_ = height;
    if (canvas_ != nullptr)
    {
        canvas_->SetViewportSize({
            static_cast<float>(width_),
            static_cast<float>(height_)});
    }
}

void LobbyScene::Shutdown() noexcept
{
    board_ = nullptr;
    canvas_.reset();
}

void LobbyScene::CreateHeader(const mrg::EngineServices& services)
{
    auto& header = AddPanel(
        *board_,
        {0.0F, 0.0F, 1920.0F, 96.0F},
        {0.973F, 0.988F, 1.0F, 0.96F},
        "Header");
    AddLabel(
        header,
        {54.0F, 15.0F, 108.0F, 58.0F},
        L"Logo",
        48.0F,
        {0.325F, 0.612F, 0.871F, 1.0F},
        "Logo");
    AddPanel(
        header,
        {188.0F, 25.0F, 2.0F, 47.0F},
        LightBlue,
        "Separator");
    AddLabel(
        header,
        {208.0F, 29.0F, 260.0F, 38.0F},
        L"MUSIC SELECT",
        16.0F,
        {0.243F, 0.514F, 0.757F, 1.0F},
        "Title");

    // 프로필 데이터가 없으므로 이름, 레이팅, 크레딧을 꾸며내지 않는다.
    // 오른쪽에는 실제 profile 시스템이 생길 때 교체할 임시 이미지만 둔다.
    auto& frame = AddPanel(
        header,
        {1770.0F, 8.0F, 80.0F, 80.0F},
        AccentBlue,
        "TemporaryProfileFrame");
    auto& profile = mrg::visual2d::CreateSprite(
        frame,
        Scale({4.0F, 4.0F, 72.0F, 72.0F}),
        services.visual2DRendering.LoadImage(ProfileImagePath()),
        "TemporaryProfileImage");
    profile.SetZIndex(1);
}

void LobbyScene::CreateCategoryBar()
{
    auto& bar = AddPanel(
        *board_,
        {54.0F, 112.0F, 1812.0F, 56.0F},
        {1.0F, 1.0F, 1.0F, 0.90F},
        "Categories");
    AddPanel(bar, {16.0F, 9.0F, 86.0F, 38.0F}, AccentBlue, "AllCategory");
    AddLabel(
        bar,
        {16.0F, 9.0F, 86.0F, 38.0F},
        L"ALL",
        13.0F,
        PureWhite,
        "AllCategoryText",
        mrg::visual2d::TextAlignment::Center);
}

void LobbyScene::CreateRecordPanel()
{
    auto& panel = AddPanel(
        *board_,
        {54.0F, 186.0F, 418.0F, 686.0F},
        PanelWhite,
        "RecordPanel");
    AddLabel(
        panel,
        {26.0F, 26.0F, 260.0F, 28.0F},
        L"LOCAL RECORD",
        14.0F,
        MutedBlue,
        "Heading");
    AddLabel(
        panel,
        {26.0F, 300.0F, 366.0F, 54.0F},
        L"NO RECORDS",
        22.0F,
        DeepBlue,
        "NoRecords",
        mrg::visual2d::TextAlignment::Center);
}

void LobbyScene::CreateSongInformationPanel()
{
    auto& panel = AddPanel(
        *board_,
        {492.0F, 186.0F, 854.0F, 262.0F},
        PanelWhite,
        "SongInformation");
    AddLabel(
        panel,
        {26.0F, 21.0F, 380.0F, 24.0F},
        L"NOW SELECTING",
        12.0F,
        MutedBlue,
        "Heading");
    auto& selectedSong = AddLabel(
        panel,
        {26.0F, 102.0F, 802.0F, 58.0F},
        L"NO SONG SELECTED",
        26.0F,
        DeepBlue,
        "NoSongSelected",
        mrg::visual2d::TextAlignment::Center);
    // Short empty-state text remains still. Once a real selected title is
    // assigned, values longer than this box automatically scroll without
    // drawing characters outside the fixed text node.
    selectedSong.AddComponent<
        finger_drum::presentation::MarqueeTextComponent>(
            L"NO SONG SELECTED",
            38,
            0.28);
}

void LobbyScene::CreatePatternPanel()
{
    auto& panel = AddPanel(
        *board_,
        {492.0F, 468.0F, 854.0F, 404.0F},
        PanelWhite,
        "PatternPanel");
    AddLabel(
        panel,
        {26.0F, 22.0F, 300.0F, 30.0F},
        L"PATTERN SELECT",
        14.0F,
        MutedBlue,
        "Heading");
    AddLabel(
        panel,
        {26.0F, 176.0F, 802.0F, 58.0F},
        L"NO PATTERNS AVAILABLE",
        24.0F,
        DeepBlue,
        "NoPatterns",
        mrg::visual2d::TextAlignment::Center);
}

void LobbyScene::CreateSongList()
{
    auto& panel = AddPanel(
        *board_,
        {1366.0F, 186.0F, 500.0F, 686.0F},
        PanelWhite,
        "SongList");
    AddLabel(
        panel,
        {22.0F, 20.0F, 300.0F, 28.0F},
        L"SONG LIST",
        14.0F,
        MutedBlue,
        "Heading");
    // Future rows intentionally contain only song title and artist. Index,
    // BPM, difficulty and record data belong to their own views.
    AddLabel(
        panel,
        {20.0F, 300.0F, 460.0F, 58.0F},
        L"NO SONGS AVAILABLE",
        21.0F,
        DeepBlue,
        "NoSongs",
        mrg::visual2d::TextAlignment::Center);
}

void LobbyScene::CreateFooter()
{
    AddLabel(
        *board_,
        {54.0F, 1040.0F, 880.0F, 24.0F},
        L"FINGERDRUM  ·  MUSIC SELECT",
        11.0F,
        MutedBlue,
        "FooterLeft");
    AddLabel(
        *board_,
        {986.0F, 1040.0F, 880.0F, 24.0F},
        L"ESC  BACK",
        11.0F,
        MutedBlue,
        "FooterRight",
        mrg::visual2d::TextAlignment::Trailing);
}

mrg::visual2d::Visual2DNode& LobbyScene::AddPanel(
    mrg::visual2d::Visual2DNode& parent,
    const mrg::visual2d::Rect penpotBounds,
    const mrg::visual2d::Color color,
    std::string name)
{
    auto& panel = mrg::visual2d::CreatePanel(
        parent,
        Scale(penpotBounds),
        std::move(name));
    ApplyFlatStyle(panel, color);
    return panel;
}

mrg::visual2d::Visual2DNode& LobbyScene::AddLabel(
    mrg::visual2d::Visual2DNode& parent,
    const mrg::visual2d::Rect penpotBounds,
    std::wstring text,
    const float penpotFontSize,
    const mrg::visual2d::Color color,
    std::string name,
    const mrg::visual2d::TextAlignment alignment)
{
    auto& label = mrg::visual2d::CreateLabel(
        parent,
        Scale(penpotBounds),
        std::move(text),
        std::move(name));
    auto& textComponent =
        RequireComponent<mrg::visual2d::TextVisualComponent>(label);
    textComponent.SetFontSize(penpotFontSize * PenpotScale);
    textComponent.SetTextColor(color);
    textComponent.SetHorizontalAlignment(alignment);
    return label;
}
