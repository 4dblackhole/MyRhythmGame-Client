#include "LobbyScene.h"

#include "GameFlow/FingerDrumSceneIds.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <optional>
#include <stdexcept>
#include <string_view>

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
    constexpr mrg::visual2d::Color Yellow{1.0F, 0.941F, 0.651F, 1.0F};

    constexpr std::array<std::wstring_view, 8> SongTitles{
        L"01   Aerial Route                 BPM 174",
        L"02   Afterglow Signal             BPM 158",
        L"03   Blue Horizon                 BPM 182",
        L"04   Cloudline                    BPM 196",
        L"05   First Light                  BPM 150",
        L"06   Glass Satellite              BPM 205",
        L"07   Northern Echo                BPM 128",
        L"08   Summer Trace                 BPM 168"};

    [[nodiscard]] mrg::visual2d::Rect Scale(
        const mrg::visual2d::Rect bounds) noexcept
    {
        return {
            bounds.x * PenpotScale,
            bounds.y * PenpotScale,
            bounds.width * PenpotScale,
            bounds.height * PenpotScale};
    }

    template <typename ComponentType>
    [[nodiscard]] ComponentType& RequireComponent(
        mrg::visual2d::Visual2DNode& node)
    {
        ComponentType* const component = node.GetComponent<ComponentType>();
        if (component == nullptr)
        {
            throw std::logic_error("A song-select node is missing a component.");
        }
        return *component;
    }

    void ApplyFlatStyle(
        mrg::visual2d::Visual2DNode& node,
        const mrg::visual2d::Color color)
    {
        mrg::visual2d::VisualStyle style{};
        style.normal = color;
        style.hovered = {
            std::min(color.red + 0.06F, 1.0F),
            std::min(color.green + 0.06F, 1.0F),
            std::min(color.blue + 0.06F, 1.0F),
            color.alpha};
        style.pressed = AccentBlue;
        style.disabled = {color.red, color.green, color.blue, 0.45F};
        RequireComponent<mrg::visual2d::SpriteVisualComponent>(node).
            SetStyle(style);
    }

    [[nodiscard]] std::int64_t LatestPointerTimestamp(
        const mrg::platform::InputState& input) noexcept
    {
        std::int64_t timestamp{};
        for (const mrg::platform::InputEvent& event : input.Events())
        {
            if (event.type == mrg::platform::InputEventType::MouseMoved ||
                event.type == mrg::platform::InputEventType::MouseButtonPressed ||
                event.type == mrg::platform::InputEventType::MouseButtonReleased ||
                event.type == mrg::platform::InputEventType::MouseWheel)
            {
                timestamp = event.performanceCounterTicks;
            }
        }
        return timestamp;
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

    // Penpot 원본 보드 전체를 하나의 중앙 정렬 트리로 만든다. 자식의
    // Z-Order는 원본의 패널 계층을 따르며 팝업만 최상위 형제로 둔다.
    board_ = &canvas_->CreateNode(
        mrg::visual2d::Anchor::Center,
        "FingerDrum.SongSelect.Board");
    board_->SetPivot({0.5F, 0.5F});
    board_->SetSize({1280.0F, 720.0F});
    board_->SetPosition({0.0F, 0.0F});

    AddPanel(*board_, {0.0F, 0.0F, 1920.0F, 1080.0F}, CanvasBlue, "Background");
    CreateHeader();
    CreateCategoryBar();
    CreateRecordPanel();
    CreateSongInformationPanel();
    CreatePatternPanel();
    CreateSongList();
    CreateBottomBar();
    CreateModifierPopup();
    SelectSong(selectedSongIndex_);
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
    ProcessPointer(context.input);
    ProcessActions(scenes);
    ProcessKeyboard(context.input, scenes);
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
        inputRouter_.InvalidateHitTest();
    }
}

void LobbyScene::Shutdown() noexcept
{
    if (canvas_ != nullptr)
    {
        inputRouter_.Reset(*canvas_);
    }
    songRows_ = {};
    songRowIds_ = {};
    selectedSongTitle_ = nullptr;
    modifierPopup_ = nullptr;
    board_ = nullptr;
    canvas_.reset();
}

void LobbyScene::CreateHeader()
{
    auto& header = AddPanel(
        *board_, {0.0F, 0.0F, 1920.0F, 96.0F},
        {0.973F, 0.988F, 1.0F, 0.96F}, "Header");
    AddLabel(header, {54.0F, 15.0F, 108.0F, 58.0F}, L"Logo", 48.0F,
        {0.325F, 0.612F, 0.871F, 1.0F}, "Logo");
    AddPanel(header, {188.0F, 25.0F, 2.0F, 47.0F}, LightBlue, "Separator");
    AddLabel(header, {208.0F, 29.0F, 260.0F, 38.0F}, L"MUSIC SELECT", 16.0F,
        {0.243F, 0.514F, 0.757F, 1.0F}, "Title");
    AddPanel(header, {1468.0F, 29.0F, 94.0F, 38.0F},
        {0.471F, 0.784F, 0.933F, 1.0F}, "OnlineBadge");
    AddLabel(header, {1468.0F, 29.0F, 94.0F, 38.0F}, L"ONLINE", 13.0F,
        PureWhite, "OnlineText", mrg::visual2d::TextAlignment::Center);
    AddLabel(header, {1586.0F, 17.0F, 140.0F, 20.0F}, L"PILOT 07", 12.0F,
        MutedBlue, "Pilot");
    AddLabel(header, {1586.0F, 40.0F, 150.0F, 30.0F}, L"RATING 12.48", 16.0F,
        DeepBlue, "Rating");
    AddPanel(header, {1742.0F, 29.0F, 124.0F, 38.0F}, Yellow, "CreditBadge");
    AddLabel(header, {1742.0F, 29.0F, 124.0F, 38.0F}, L"CREDIT  2", 13.0F,
        DeepBlue, "Credit", mrg::visual2d::TextAlignment::Center);
}

void LobbyScene::CreateCategoryBar()
{
    auto& bar = AddPanel(
        *board_, {54.0F, 112.0F, 1812.0F, 56.0F},
        {1.0F, 1.0F, 1.0F, 0.90F}, "Categories");
    struct Category final
    {
        float x;
        float width;
        std::wstring_view text;
        bool selected;
    };
    constexpr std::array<Category, 8> categories{{
        {16.0F, 86.0F, L"ALL", false},
        {112.0F, 90.0F, L"NEW", false},
        {212.0F, 118.0F, L"POPULAR", false},
        {340.0F, 152.0F, L"ELECTRONIC", true},
        {502.0F, 90.0F, L"ROCK", false},
        {602.0F, 98.0F, L"VOCAL", false},
        {710.0F, 124.0F, L"ORIGINAL", false},
        {844.0F, 132.0F, L"FAVORITE", false}}};
    for (const Category& category : categories)
    {
        AddButton(
            bar,
            {category.x, 9.0F, category.width, 38.0F},
            std::wstring(category.text),
            13.0F,
            category.selected ? AccentBlue : PureWhite,
            category.selected ? PureWhite : DeepBlue,
            "Category");
    }
    AddLabel(bar, {1580.0F, 12.0F, 200.0F, 32.0F}, L"128 TRACKS", 13.0F,
        MutedBlue, "TrackCount", mrg::visual2d::TextAlignment::Trailing);
}

void LobbyScene::CreateRecordPanel()
{
    auto& panel = AddPanel(
        *board_, {54.0F, 186.0F, 418.0F, 686.0F}, PanelWhite, "RecordPanel");
    AddLabel(panel, {26.0F, 26.0F, 220.0F, 28.0F}, L"LOCAL RECORD", 14.0F,
        MutedBlue, "Heading");
    AddLabel(panel, {26.0F, 66.0F, 300.0F, 46.0F}, L"987,420", 30.0F,
        DeepBlue, "Score");
    AddLabel(panel, {28.0F, 114.0F, 250.0F, 26.0F}, L"EXPERT · CLEAR", 13.0F,
        AccentBlue, "ClearState");
    AddPanel(panel, {26.0F, 170.0F, 366.0F, 2.0F}, LightBlue, "Divider");

    constexpr std::array<std::wstring_view, 5> stats{
        L"MAX                00482",
        L"PERFECT            00137",
        L"GREAT              00018",
        L"GOOD               00003",
        L"MISS               00001"};
    for (std::size_t index = 0; index < stats.size(); ++index)
    {
        AddLabel(panel,
            {28.0F, 202.0F + static_cast<float>(index) * 46.0F, 360.0F, 30.0F},
            std::wstring(stats[index]), 15.0F,
            index == 0 ? AccentBlue : DeepBlue, "RecordStat");
    }
    AddPanel(panel, {26.0F, 456.0F, 366.0F, 2.0F}, LightBlue, "Divider2");
    AddLabel(panel, {28.0F, 486.0F, 350.0F, 26.0F}, L"ACCURACY     98.72%", 16.0F,
        DeepBlue, "Accuracy");
    AddLabel(panel, {28.0F, 530.0F, 350.0F, 26.0F}, L"MAX COMBO      641", 16.0F,
        DeepBlue, "Combo");
    AddPanel(panel, {26.0F, 596.0F, 366.0F, 58.0F}, Yellow, "RankPanel");
    AddLabel(panel, {42.0F, 606.0F, 330.0F, 38.0F}, L"RANK    S+", 22.0F,
        DeepBlue, "Rank", mrg::visual2d::TextAlignment::Center);
}

void LobbyScene::CreateSongInformationPanel()
{
    auto& panel = AddPanel(
        *board_, {492.0F, 186.0F, 854.0F, 262.0F}, PanelWhite, "SongInformation");
    AddLabel(panel, {26.0F, 21.0F, 380.0F, 24.0F},
        L"NOW SELECTING  /  003 OF 128", 12.0F, MutedBlue, "SelectionIndex");
    AddPanel(panel, {700.0F, 18.0F, 126.0F, 36.0F}, AccentBlue, "GenreBadge");
    AddLabel(panel, {700.0F, 18.0F, 126.0F, 36.0F}, L"ELECTRONIC", 12.0F,
        PureWhite, "Genre", mrg::visual2d::TextAlignment::Center);
    selectedSongTitle_ = &AddLabel(panel, {26.0F, 49.0F, 616.0F, 72.0F},
        L"Blue Horizon — Where the Morning Sky Begins", 32.0F,
        DeepBlue, "SongTitle");
    AddLabel(panel, {28.0F, 130.0F, 600.0F, 32.0F},
        L"Luminous Field  feat. Airi", 18.0F, AccentBlue, "Artist");
    AddLabel(panel, {28.0F, 178.0F, 600.0F, 24.0F},
        L"BPM 182   ·   02:14   ·   4/4", 13.0F, MutedBlue, "Metadata");
    AddButton(panel, {26.0F, 206.0F, 148.0F, 36.0F}, L"♡  FAVORITE", 12.0F,
        LightBlue, DeepBlue, "Favorite");
    AddButton(panel, {184.0F, 206.0F, 112.0F, 36.0F}, L"PREVIEW", 12.0F,
        PureWhite, DeepBlue, "Preview");
    AddLabel(panel, {546.0F, 190.0F, 86.0F, 54.0F}, L"03", 36.0F,
        {0.650F, 0.820F, 0.930F, 1.0F}, "LargeIndex");
    AddPanel(panel, {662.0F, 36.0F, 166.0F, 204.0F},
        {0.390F, 0.720F, 0.930F, 1.0F}, "Jacket");
    AddLabel(panel, {676.0F, 102.0F, 138.0F, 62.0F}, L"BLUE\nHORIZON", 17.0F,
        PureWhite, "JacketTitle", mrg::visual2d::TextAlignment::Center);
}

void LobbyScene::CreatePatternPanel()
{
    auto& panel = AddPanel(
        *board_, {492.0F, 468.0F, 854.0F, 404.0F}, PanelWhite, "PatternPanel");
    AddLabel(panel, {26.0F, 22.0F, 300.0F, 30.0F}, L"PATTERN SELECT", 14.0F,
        MutedBlue, "Heading");
    struct Pattern final
    {
        std::wstring_view name;
        int level;
        mrg::visual2d::Color color;
    };
    constexpr std::array<Pattern, 4> patterns{
        Pattern{L"BASIC", 4, {0.448F, 0.765F, 0.918F, 1.0F}},
        Pattern{L"ADVANCED", 9, {0.310F, 0.624F, 0.878F, 1.0F}},
        Pattern{L"EXPERT", 14, {0.145F, 0.388F, 0.651F, 1.0F}},
        Pattern{L"MASTER", 17, {0.333F, 0.286F, 0.620F, 1.0F}}};
    for (std::size_t index = 0; index < patterns.size(); ++index)
    {
        const float y = 66.0F + static_cast<float>(index) * 66.0F;
        AddPanel(panel, {26.0F, y, 544.0F, 54.0F},
            index == 2 ? LightBlue : PureWhite, "PatternRow");
        AddPanel(panel, {26.0F, y, 8.0F, 54.0F}, patterns[index].color,
            "PatternAccent");
        AddLabel(panel, {48.0F, y + 8.0F, 280.0F, 36.0F},
            std::wstring(patterns[index].name), 18.0F,
            index == 2 ? DeepBlue : MutedBlue, "PatternName");
        AddLabel(panel, {450.0F, y + 6.0F, 96.0F, 38.0F},
            L"LV " + std::to_wstring(patterns[index].level), 20.0F,
            patterns[index].color, "PatternLevel",
            mrg::visual2d::TextAlignment::Trailing);
    }
    AddPanel(panel, {594.0F, 66.0F, 232.0F, 252.0F},
        {0.945F, 0.977F, 1.0F, 1.0F}, "PatternStats");
    AddLabel(panel, {618.0F, 84.0F, 184.0F, 28.0F}, L"CHART INFO", 13.0F,
        MutedBlue, "ChartInfo", mrg::visual2d::TextAlignment::Center);
    AddLabel(panel, {618.0F, 132.0F, 184.0F, 138.0F},
        L"NOTES       641\n\nMAX BPM     182\n\nJUDGE LV     50", 14.0F,
        DeepBlue, "ChartStats");
    AddLabel(panel, {26.0F, 350.0F, 800.0F, 28.0F},
        L"↑↓ SELECT PATTERN     ENTER PLAY     M MODIFIERS", 12.0F,
        MutedBlue, "Hint");
}

void LobbyScene::CreateSongList()
{
    auto& panel = AddPanel(
        *board_, {1366.0F, 186.0F, 500.0F, 686.0F}, PanelWhite, "SongList");
    AddLabel(panel, {22.0F, 20.0F, 300.0F, 28.0F}, L"SONG LIST", 14.0F,
        MutedBlue, "Heading");
    AddLabel(panel, {330.0F, 20.0F, 144.0F, 28.0F}, L"SORT: TITLE", 12.0F,
        AccentBlue, "Sort", mrg::visual2d::TextAlignment::Trailing);
    for (std::size_t index = 0; index < songRows_.size(); ++index)
    {
        const float y = 64.0F + static_cast<float>(index) * 70.0F;
        auto& row = AddButton(
            panel,
            {20.0F, y, 460.0F, 58.0F},
            std::wstring(SongTitles[index]),
            14.0F,
            PureWhite,
            DeepBlue,
            "SongRow");
        RequireComponent<mrg::visual2d::TextVisualComponent>(row).
            SetHorizontalAlignment(mrg::visual2d::TextAlignment::Leading);
        songRows_[index] = &row;
        songRowIds_[index] = row.Id();
    }
    AddLabel(panel, {20.0F, 636.0F, 460.0F, 26.0F},
        L"MOUSE WHEEL / ↑↓  BROWSE", 12.0F, MutedBlue, "ListHint",
        mrg::visual2d::TextAlignment::Center);
}

void LobbyScene::CreateBottomBar()
{
    auto& bar = AddPanel(
        *board_, {54.0F, 894.0F, 1812.0F, 126.0F},
        {0.969F, 0.990F, 1.0F, 1.0F}, "BottomBar");
    AddLabel(bar, {28.0F, 20.0F, 210.0F, 26.0F}, L"PLAY SETUP", 13.0F,
        MutedBlue, "Heading");
    auto& modifiers = AddButton(bar, {28.0F, 58.0F, 250.0F, 46.0F},
        L"MODIFIERS  [M]", 14.0F, PureWhite, DeepBlue, "Modifiers");
    modifierButtonId_ = modifiers.Id();
    AddLabel(bar, {312.0F, 61.0F, 500.0F, 40.0F},
        L"SPEED 1.00x     MIRROR OFF     RANDOM OFF", 14.0F,
        DeepBlue, "SetupSummary");
    auto& play = AddButton(bar, {1432.0F, 20.0F, 352.0F, 84.0F},
        L"PLAY   EXPERT 14", 22.0F, AccentBlue, PureWhite, "Play");
    playButtonId_ = play.Id();
    AddLabel(*board_, {54.0F, 1040.0F, 880.0F, 24.0F},
        L"FINGERDRUM  ·  MUSIC SELECT PROTOTYPE", 11.0F,
        MutedBlue, "FooterLeft");
    AddLabel(*board_, {986.0F, 1040.0F, 880.0F, 24.0F},
        L"ESC BACK   ·   ENTER CONFIRM", 11.0F,
        MutedBlue, "FooterRight", mrg::visual2d::TextAlignment::Trailing);
}

void LobbyScene::CreateModifierPopup()
{
    modifierPopup_ = &AddPanel(*board_, {530.0F, 250.0F, 860.0F, 560.0F},
        {0.925F, 0.969F, 1.0F, 0.99F}, "ModifierPopup");
    modifierPopup_->SetZIndex(100);
    // Empty popup space also participates in hit testing, preventing a click
    // from leaking through to the song list behind the modal panel.
    modifierPopup_->AddComponent<
        mrg::visual2d::RectangleCollider2DComponent>();
    modifierPopup_->AddComponent<mrg::visual2d::PointerReceiverComponent>();
    AddLabel(*modifierPopup_, {36.0F, 28.0F, 560.0F, 52.0F},
        L"PLAY MODIFIERS", 26.0F, DeepBlue, "Title");
    AddLabel(*modifierPopup_, {38.0F, 92.0F, 760.0F, 42.0F},
        L"Adjust gameplay presentation without changing the chart.", 14.0F,
        MutedBlue, "Description");
    constexpr std::array<std::wstring_view, 4> options{
        L"NOTE SPEED     1.00x",
        L"MIRROR         OFF",
        L"RANDOM         OFF",
        L"HIT SOUND      100%"};
    for (std::size_t index = 0; index < options.size(); ++index)
    {
        AddButton(*modifierPopup_,
            {38.0F, 158.0F + static_cast<float>(index) * 72.0F, 784.0F, 54.0F},
            std::wstring(options[index]), 16.0F, PureWhite, DeepBlue,
            "ModifierOption");
    }
    auto& close = AddButton(*modifierPopup_, {606.0F, 484.0F, 216.0F, 50.0F},
        L"CLOSE", 15.0F, AccentBlue, PureWhite, "CloseModifier");
    closeModifierButtonId_ = close.Id();
    modifierPopup_->SetVisible(false);
    modifierPopup_->SetEnabled(false);
}

void LobbyScene::ProcessPointer(const mrg::platform::InputState& input)
{
    const std::optional<mrg::visual2d::Point> mapped =
        input.IsMouseInsideWindow()
        ? mrg::visual2d::MapScreenPointer(
            {static_cast<float>(input.MousePositionX()),
             static_cast<float>(input.MousePositionY())},
            {static_cast<float>(width_), static_cast<float>(height_)},
            *canvas_)
        : std::nullopt;
    mrg::visual2d::PointerInput pointer{};
    pointer.available = mapped.has_value();
    pointer.position = mapped.value_or(mrg::visual2d::Point{});
    pointer.leftButtonDown = input.IsMouseButtonDown(
        mrg::platform::MouseButton::Left);
    pointer.leftButtonPressed = input.WasMouseButtonPressed(
        mrg::platform::MouseButton::Left);
    pointer.leftButtonReleased = input.WasMouseButtonReleased(
        mrg::platform::MouseButton::Left);
    pointer.wheelDelta = input.MouseWheelDelta();
    pointer.timestampTicks = LatestPointerTimestamp(input);
    inputRouter_.Process(*canvas_, pointer);
}

void LobbyScene::ProcessActions(mrg::scene::SceneManager& scenes)
{
    for (const mrg::visual2d::Action& action : canvas_->TakeActions())
    {
        if (action.type != mrg::visual2d::ActionType::Clicked)
        {
            continue;
        }
        if (action.source == playButtonId_)
        {
            EnterSelectedPattern(scenes);
            return;
        }
        if (action.source == modifierButtonId_ ||
            action.source == closeModifierButtonId_)
        {
            ToggleModifierPopup();
            continue;
        }
        for (std::size_t index = 0; index < songRowIds_.size(); ++index)
        {
            if (action.source == songRowIds_[index])
            {
                SelectSong(index);
                break;
            }
        }
    }
}

void LobbyScene::ProcessKeyboard(
    const mrg::platform::InputState& input,
    mrg::scene::SceneManager& scenes)
{
    if (input.WasKeyPressed(static_cast<std::uint16_t>('M')))
    {
        ToggleModifierPopup();
        return;
    }
    if (input.WasKeyPressed(VK_ESCAPE))
    {
        if (modifierPopup_ != nullptr && modifierPopup_->IsVisible())
        {
            ToggleModifierPopup();
        }
        else if (!scenes.ChangeScene(finger_drum::scene_ids::Logo))
        {
            throw std::runtime_error("Failed to return to FingerDrum Logo.");
        }
        return;
    }
    if (modifierPopup_ != nullptr && modifierPopup_->IsVisible())
    {
        return;
    }
    if (input.WasKeyPressed(VK_UP))
    {
        SelectSong((selectedSongIndex_ + songRows_.size() - 1) % songRows_.size());
    }
    else if (input.WasKeyPressed(VK_DOWN))
    {
        SelectSong((selectedSongIndex_ + 1) % songRows_.size());
    }
    if (input.WasKeyPressed(VK_RETURN))
    {
        EnterSelectedPattern(scenes);
    }
}

void LobbyScene::SelectSong(const std::size_t index)
{
    if (index >= songRows_.size())
    {
        return;
    }
    selectedSongIndex_ = index;
    for (std::size_t rowIndex = 0; rowIndex < songRows_.size(); ++rowIndex)
    {
        ApplyFlatStyle(
            *songRows_[rowIndex],
            rowIndex == selectedSongIndex_ ? AccentBlue : PureWhite);
        RequireComponent<mrg::visual2d::TextVisualComponent>(*songRows_[rowIndex]).
            SetTextColor(rowIndex == selectedSongIndex_ ? PureWhite : DeepBlue);
    }
    if (selectedSongTitle_ != nullptr)
    {
        const std::wstring title(SongTitles[selectedSongIndex_]);
        RequireComponent<mrg::visual2d::TextVisualComponent>(*selectedSongTitle_).
            SetText(title.substr(5, title.find(L"   BPM") - 5));
    }
    inputRouter_.InvalidateHitTest();
}

void LobbyScene::ToggleModifierPopup()
{
    if (modifierPopup_ == nullptr)
    {
        return;
    }
    const bool show = !modifierPopup_->IsVisible();
    modifierPopup_->SetVisible(show);
    modifierPopup_->SetEnabled(show);
    inputRouter_.InvalidateHitTest();
}

void LobbyScene::EnterSelectedPattern(mrg::scene::SceneManager& scenes)
{
    if (!scenes.ChangeScene(finger_drum::scene_ids::RhythmTest))
    {
        throw std::runtime_error("Failed to enter the rhythm test Scene.");
    }
}

mrg::visual2d::Visual2DNode& LobbyScene::AddPanel(
    mrg::visual2d::Visual2DNode& parent,
    const mrg::visual2d::Rect penpotBounds,
    const mrg::visual2d::Color color,
    std::string name)
{
    auto& panel = mrg::visual2d::CreatePanel(
        parent, Scale(penpotBounds), std::move(name));
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
        parent, Scale(penpotBounds), std::move(text), std::move(name));
    auto& textComponent = RequireComponent<mrg::visual2d::TextVisualComponent>(label);
    textComponent.SetFontSize(penpotFontSize * PenpotScale);
    textComponent.SetTextColor(color);
    textComponent.SetHorizontalAlignment(alignment);
    return label;
}

mrg::visual2d::Visual2DNode& LobbyScene::AddButton(
    mrg::visual2d::Visual2DNode& parent,
    const mrg::visual2d::Rect penpotBounds,
    std::wstring text,
    const float penpotFontSize,
    const mrg::visual2d::Color color,
    const mrg::visual2d::Color textColor,
    std::string name)
{
    auto& button = mrg::visual2d::CreateButton(
        parent, Scale(penpotBounds), std::move(text), std::move(name));
    ApplyFlatStyle(button, color);
    auto& textComponent = RequireComponent<mrg::visual2d::TextVisualComponent>(button);
    textComponent.SetFontSize(penpotFontSize * PenpotScale);
    textComponent.SetTextColor(textColor);
    return button;
}
