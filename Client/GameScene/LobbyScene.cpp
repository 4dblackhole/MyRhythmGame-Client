#include "LobbyScene.h"

#include "GameFlow/FingerDrumSceneIds.h"
#include "Presentation/MarqueeTextComponent.h"

#include <Windows.h>

#include <algorithm>
#include <filesystem>
#include <format>
#include <optional>
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

    [[nodiscard]] std::filesystem::path RuntimeSongsPath()
    {
        return mrg::platform::ResolveExecutableRelativePath(L"assets\\songs");
    }

    [[nodiscard]] std::filesystem::path ProfileImagePath()
    {
        return mrg::platform::ResolveExecutableRelativePath(
            L"assets\\images\\profile\\TemporaryPilot.png");
    }

    [[nodiscard]] std::wstring DecodeDisplayText(const std::string_view value)
    {
        if (value.empty())
        {
            return {};
        }

        // Current chart files contain both UTF-8 and legacy Korean-code-page
        // text. Prefer strict UTF-8 and fall back to the user's ANSI page so
        // old YMM/YMP metadata remains visible during migration.
        UINT codePage = CP_UTF8;
        DWORD flags = MB_ERR_INVALID_CHARS;
        int length = MultiByteToWideChar(
            codePage,
            flags,
            value.data(),
            static_cast<int>(value.size()),
            nullptr,
            0);
        if (length <= 0)
        {
            codePage = CP_ACP;
            flags = 0;
            length = MultiByteToWideChar(
                codePage,
                flags,
                value.data(),
                static_cast<int>(value.size()),
                nullptr,
                0);
        }
        if (length <= 0)
        {
            return L"?";
        }
        std::wstring result(static_cast<std::size_t>(length), L'\0');
        MultiByteToWideChar(
            codePage,
            flags,
            value.data(),
            static_cast<int>(value.size()),
            result.data(),
            length);
        return result;
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

    void ApplySelectableStyle(
        mrg::visual2d::Visual2DNode& node,
        const bool selected)
    {
        mrg::visual2d::VisualStyle style{};
        style.normal = selected
            ? mrg::visual2d::Color{0.310F, 0.624F, 0.878F, 1.0F}
            : mrg::visual2d::Color{0.900F, 0.956F, 0.992F, 0.96F};
        style.hovered = selected
            ? mrg::visual2d::Color{0.365F, 0.690F, 0.930F, 1.0F}
            : mrg::visual2d::Color{0.820F, 0.920F, 0.988F, 1.0F};
        style.pressed = {0.190F, 0.490F, 0.790F, 1.0F};
        style.disabled = {0.70F, 0.78F, 0.84F, 0.55F};
        RequireComponent<mrg::visual2d::SpriteVisualComponent>(node).
            SetStyle(style);
        RequireComponent<mrg::visual2d::TextVisualComponent>(node).
            SetTextColor(selected ? PureWhite : DeepBlue);
    }

    [[nodiscard]] std::wstring SongTitle(
        const finger_drum::chart::SongCatalogEntry& song)
    {
        return song.music.names.empty()
            ? song.metadataPath.stem().wstring()
            : DecodeDisplayText(song.music.names.front());
    }

    [[nodiscard]] std::wstring SongArtist(
        const finger_drum::chart::SongCatalogEntry& song)
    {
        return song.music.artists.empty()
            ? L"UNKNOWN ARTIST"
            : DecodeDisplayText(song.music.artists.front());
    }

    [[nodiscard]] std::int64_t LatestPointerTimestamp(
        const mrg::platform::InputState& input) noexcept
    {
        std::int64_t result{};
        for (const mrg::platform::InputEvent& event : input.Events())
        {
            if (event.type == mrg::platform::InputEventType::MouseMoved ||
                event.type == mrg::platform::InputEventType::MouseButtonPressed ||
                event.type == mrg::platform::InputEventType::MouseButtonReleased)
            {
                result = event.performanceCounterTicks;
            }
        }
        return result;
    }
}

LobbyScene::LobbyScene(
    std::shared_ptr<finger_drum::GameplayLaunchRequest> launchRequest)
    : launchRequest_(std::move(launchRequest))
{
}

void LobbyScene::Initialize(const mrg::EngineServices& services)
{
    if (launchRequest_ == nullptr)
    {
        throw std::invalid_argument("Lobby requires a gameplay launch request.");
    }

    width_ = services.windowWidth;
    height_ = services.windowHeight;
    catalog_ = finger_drum::chart::SongCatalog{}.Load(RuntimeSongsPath());
    canvas_ = std::make_unique<mrg::visual2d::Visual2DCanvas>(
        mrg::visual2d::Size{1280.0F, 720.0F},
        mrg::visual2d::CanvasScaleMode::FixedHeight);
    canvas_->SetViewportSize({
        static_cast<float>(width_),
        static_cast<float>(height_)});

    // The 1920x1080 Penpot board is uniformly mapped into the Canvas's
    // 1280x720 reference space. Every widget remains in one deterministic
    // Visual2D tree, including the mouse-accessible Back and Play controls.
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
    RefreshSelectionPresentation();
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
    if (ProcessActions(scenes) || ProcessKeyboard(context.input, scenes))
    {
        return;
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
        inputRouter_.InvalidateHitTest();
    }
}

void LobbyScene::Shutdown() noexcept
{
    if (canvas_ != nullptr)
    {
        inputRouter_.Reset(*canvas_);
    }
    patternButtonIds_.clear();
    patternButtons_.clear();
    songButtonIds_.clear();
    songButtons_.clear();
    patternList_ = nullptr;
    selectedPatternLabel_ = nullptr;
    selectedArtistLabel_ = nullptr;
    selectedSongLabel_ = nullptr;
    board_ = nullptr;
    canvas_.reset();
    catalog_ = {};
}

void LobbyScene::CreateHeader(const mrg::EngineServices& services)
{
    auto& header = AddPanel(
        *board_,
        {0.0F, 0.0F, 1920.0F, 96.0F},
        {0.973F, 0.988F, 1.0F, 0.96F},
        "Header");
    auto& back = mrg::visual2d::CreateButton(
        header, Scale({24.0F, 17.0F, 126.0F, 62.0F}), L"< BACK", "Back");
    backButtonId_ = back.Id();
    ApplySelectableStyle(back, false);
    RequireComponent<mrg::visual2d::TextVisualComponent>(back).
        SetFontSize(15.0F);
    AddPanel(header, {174.0F, 25.0F, 2.0F, 47.0F}, LightBlue, "Separator");
    AddLabel(
        header,
        {198.0F, 29.0F, 340.0F, 38.0F},
        L"FINGERDRUM / MUSIC SELECT",
        16.0F,
        {0.243F, 0.514F, 0.757F, 1.0F},
        "Title");

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
    AddLabel(panel, {26.0F, 26.0F, 260.0F, 28.0F}, L"LOCAL RECORD",
        14.0F, MutedBlue, "Heading");
    AddLabel(panel, {26.0F, 300.0F, 366.0F, 54.0F}, L"NO RECORDS",
        22.0F, DeepBlue, "NoRecords", mrg::visual2d::TextAlignment::Center);
}

void LobbyScene::CreateSongInformationPanel()
{
    auto& panel = AddPanel(
        *board_,
        {492.0F, 186.0F, 854.0F, 262.0F},
        PanelWhite,
        "SongInformation");
    AddLabel(panel, {26.0F, 21.0F, 380.0F, 24.0F}, L"NOW SELECTING",
        12.0F, MutedBlue, "Heading");
    selectedSongLabel_ = &AddLabel(
        panel, {32.0F, 76.0F, 790.0F, 64.0F}, L"NO SONG SELECTED",
        28.0F, DeepBlue, "SelectedSong", mrg::visual2d::TextAlignment::Center);
    selectedSongLabel_->AddComponent<
        finger_drum::presentation::MarqueeTextComponent>(
            L"NO SONG SELECTED", 38, 0.28);
    selectedArtistLabel_ = &AddLabel(
        panel, {32.0F, 152.0F, 790.0F, 42.0F}, L"",
        16.0F, MutedBlue, "SelectedArtist", mrg::visual2d::TextAlignment::Center);
    selectedArtistLabel_->AddComponent<
        finger_drum::presentation::MarqueeTextComponent>(L"", 52, 0.32);
}

void LobbyScene::CreatePatternPanel()
{
    auto& panel = AddPanel(
        *board_,
        {492.0F, 468.0F, 854.0F, 404.0F},
        PanelWhite,
        "PatternPanel");
    AddLabel(panel, {26.0F, 22.0F, 300.0F, 30.0F}, L"PATTERN SELECT",
        14.0F, MutedBlue, "Heading");
    selectedPatternLabel_ = &AddLabel(
        panel, {344.0F, 22.0F, 480.0F, 30.0F}, L"NO PATTERN",
        13.0F, DeepBlue, "SelectedPattern",
        mrg::visual2d::TextAlignment::Trailing);
    patternList_ = &panel.CreateChild("PatternList");
    patternList_->SetSize({854.0F * PenpotScale, 404.0F * PenpotScale});
    auto& play = mrg::visual2d::CreateButton(
        panel, Scale({620.0F, 322.0F, 204.0F, 56.0F}), L"PLAY", "Play");
    playButtonId_ = play.Id();
    ApplySelectableStyle(play, true);
    RequireComponent<mrg::visual2d::TextVisualComponent>(play).
        SetFontSize(17.0F);
}

void LobbyScene::CreateSongList()
{
    auto& panel = AddPanel(
        *board_,
        {1366.0F, 186.0F, 500.0F, 686.0F},
        PanelWhite,
        "SongList");
    AddLabel(panel, {22.0F, 20.0F, 300.0F, 28.0F}, L"SONG LIST",
        14.0F, MutedBlue, "Heading");

    if (catalog_.songs.empty())
    {
        AddLabel(panel, {20.0F, 300.0F, 460.0F, 58.0F},
            L"NO SONGS AVAILABLE", 21.0F, DeepBlue, "NoSongs",
            mrg::visual2d::TextAlignment::Center);
        return;
    }

    constexpr float rowTop = 72.0F;
    constexpr float rowHeight = 96.0F;
    constexpr float rowStep = 112.0F;
    for (std::size_t index = 0; index < catalog_.songs.size(); ++index)
    {
        const finger_drum::chart::SongCatalogEntry& song = catalog_.songs[index];
        const std::wstring text = SongTitle(song) + L"\n" + SongArtist(song);
        auto& button = mrg::visual2d::CreateButton(
            panel,
            Scale({20.0F, rowTop + rowStep * static_cast<float>(index),
                460.0F, rowHeight}),
            text,
            std::format("Song.{}", index));
        RequireComponent<mrg::visual2d::TextVisualComponent>(button).
            SetFontSize(14.0F);
        songButtons_.push_back(&button);
        songButtonIds_.push_back(button.Id());
    }
}

void LobbyScene::CreateFooter()
{
    AddLabel(*board_, {54.0F, 1040.0F, 880.0F, 24.0F},
        L"UP / DOWN : SONG    LEFT / RIGHT : PATTERN    ENTER : PLAY",
        11.0F, MutedBlue, "FooterLeft");
    AddLabel(*board_, {986.0F, 1040.0F, 880.0F, 24.0F},
        L"ESC : BACK", 11.0F, MutedBlue, "FooterRight",
        mrg::visual2d::TextAlignment::Trailing);
}

void LobbyScene::RebuildPatternButtons()
{
    if (patternList_ == nullptr || canvas_ == nullptr)
    {
        return;
    }
    inputRouter_.Reset(*canvas_);
    for (const mrg::visual2d::NodeId id : patternButtonIds_)
    {
        static_cast<void>(patternList_->RemoveChild(id));
    }
    patternButtons_.clear();
    patternButtonIds_.clear();

    if (catalog_.songs.empty())
    {
        return;
    }
    const auto& patterns = catalog_.songs[selectedSongIndex_].patterns;
    for (std::size_t index = 0; index < patterns.size(); ++index)
    {
        const std::wstring name = patterns[index].pattern.name.empty()
            ? patterns[index].patternPath.stem().wstring()
            : DecodeDisplayText(patterns[index].pattern.name);
        auto& button = mrg::visual2d::CreateButton(
            *patternList_,
            Scale({28.0F, 74.0F + 72.0F * static_cast<float>(index),
                796.0F, 58.0F}),
            name,
            std::format("Pattern.{}", index));
        RequireComponent<mrg::visual2d::TextVisualComponent>(button).
            SetFontSize(15.0F);
        patternButtons_.push_back(&button);
        patternButtonIds_.push_back(button.Id());
    }
    inputRouter_.InvalidateHitTest();
}

void LobbyScene::RefreshSelectionPresentation()
{
    if (selectedSongLabel_ == nullptr || selectedArtistLabel_ == nullptr ||
        selectedPatternLabel_ == nullptr)
    {
        return;
    }

    if (catalog_.songs.empty())
    {
        RequireComponent<finger_drum::presentation::MarqueeTextComponent>(
            *selectedSongLabel_).SetText(L"NO SONG SELECTED");
        RequireComponent<finger_drum::presentation::MarqueeTextComponent>(
            *selectedArtistLabel_).SetText(L"");
        RequireComponent<mrg::visual2d::TextVisualComponent>(
            *selectedPatternLabel_).SetText(L"NO PATTERN");
        return;
    }

    selectedSongIndex_ = std::min(selectedSongIndex_, catalog_.songs.size() - 1);
    const auto& song = catalog_.songs[selectedSongIndex_];
    RequireComponent<finger_drum::presentation::MarqueeTextComponent>(
        *selectedSongLabel_).SetText(SongTitle(song));
    RequireComponent<finger_drum::presentation::MarqueeTextComponent>(
        *selectedArtistLabel_).SetText(SongArtist(song));
    for (std::size_t index = 0; index < songButtons_.size(); ++index)
    {
        ApplySelectableStyle(*songButtons_[index], index == selectedSongIndex_);
    }

    RebuildPatternButtons();
    if (song.patterns.empty())
    {
        RequireComponent<mrg::visual2d::TextVisualComponent>(
            *selectedPatternLabel_).SetText(L"NO PATTERNS AVAILABLE");
        return;
    }
    selectedPatternIndex_ = std::min(
        selectedPatternIndex_, song.patterns.size() - 1);
    SelectPattern(selectedPatternIndex_);
}

void LobbyScene::ProcessPointer(const mrg::platform::InputState& input)
{
    const std::optional<mrg::visual2d::Point> canvasPointer =
        input.IsMouseInsideWindow()
            ? mrg::visual2d::MapScreenPointer(
                  {static_cast<float>(input.MousePositionX()),
                   static_cast<float>(input.MousePositionY())},
                  {static_cast<float>(width_), static_cast<float>(height_)},
                  *canvas_)
            : std::nullopt;
    mrg::visual2d::PointerInput pointer{};
    pointer.available = canvasPointer.has_value();
    pointer.position = canvasPointer.value_or(mrg::visual2d::Point{});
    pointer.leftButtonDown = input.IsMouseButtonDown(
        mrg::platform::MouseButton::Left);
    pointer.leftButtonPressed = input.WasMouseButtonPressed(
        mrg::platform::MouseButton::Left);
    pointer.leftButtonReleased = input.WasMouseButtonReleased(
        mrg::platform::MouseButton::Left);
    pointer.timestampTicks = LatestPointerTimestamp(input);
    inputRouter_.Process(*canvas_, pointer);
}

bool LobbyScene::ProcessActions(mrg::scene::SceneManager& scenes)
{
    for (const mrg::visual2d::Action& action : canvas_->TakeActions())
    {
        if (action.type != mrg::visual2d::ActionType::Clicked)
        {
            continue;
        }
        if (action.source == backButtonId_)
        {
            return ReturnToLogo(scenes);
        }
        if (action.source == playButtonId_)
        {
            return StartSelectedPattern(scenes);
        }
        for (std::size_t index = 0; index < songButtonIds_.size(); ++index)
        {
            if (action.source == songButtonIds_[index])
            {
                SelectSong(index);
                return false;
            }
        }
        for (std::size_t index = 0; index < patternButtonIds_.size(); ++index)
        {
            if (action.source == patternButtonIds_[index])
            {
                SelectPattern(index);
                return false;
            }
        }
    }
    return false;
}

bool LobbyScene::ProcessKeyboard(
    const mrg::platform::InputState& input,
    mrg::scene::SceneManager& scenes)
{
    if (input.WasKeyPressed(VK_ESCAPE))
    {
        return ReturnToLogo(scenes);
    }
    if (catalog_.songs.empty())
    {
        return false;
    }
    if (input.WasKeyPressed(VK_UP))
    {
        SelectSong((selectedSongIndex_ + catalog_.songs.size() - 1) %
            catalog_.songs.size());
    }
    else if (input.WasKeyPressed(VK_DOWN))
    {
        SelectSong((selectedSongIndex_ + 1) % catalog_.songs.size());
    }

    const auto& patterns = catalog_.songs[selectedSongIndex_].patterns;
    if (!patterns.empty() && input.WasKeyPressed(VK_LEFT))
    {
        SelectPattern((selectedPatternIndex_ + patterns.size() - 1) %
            patterns.size());
    }
    else if (!patterns.empty() && input.WasKeyPressed(VK_RIGHT))
    {
        SelectPattern((selectedPatternIndex_ + 1) % patterns.size());
    }
    if (input.WasKeyPressed(VK_RETURN) || input.WasKeyPressed(VK_SPACE))
    {
        return StartSelectedPattern(scenes);
    }
    return false;
}

void LobbyScene::SelectSong(const std::size_t index)
{
    if (index >= catalog_.songs.size())
    {
        return;
    }
    selectedSongIndex_ = index;
    selectedPatternIndex_ = 0;
    RefreshSelectionPresentation();
}

void LobbyScene::SelectPattern(const std::size_t index)
{
    if (catalog_.songs.empty())
    {
        return;
    }
    const auto& patterns = catalog_.songs[selectedSongIndex_].patterns;
    if (index >= patterns.size())
    {
        return;
    }
    selectedPatternIndex_ = index;
    const auto& pattern = patterns[selectedPatternIndex_];
    const std::wstring name = pattern.pattern.name.empty()
        ? pattern.patternPath.stem().wstring()
        : DecodeDisplayText(pattern.pattern.name);
    RequireComponent<mrg::visual2d::TextVisualComponent>(
        *selectedPatternLabel_).SetText(name);
    for (std::size_t buttonIndex = 0;
         buttonIndex < patternButtons_.size();
         ++buttonIndex)
    {
        ApplySelectableStyle(
            *patternButtons_[buttonIndex],
            buttonIndex == selectedPatternIndex_);
    }
}

bool LobbyScene::StartSelectedPattern(mrg::scene::SceneManager& scenes)
{
    if (catalog_.songs.empty())
    {
        return false;
    }
    const auto& song = catalog_.songs[selectedSongIndex_];
    if (song.patterns.empty())
    {
        return false;
    }
    const auto& pattern = song.patterns[selectedPatternIndex_];
    launchRequest_->patternPath = pattern.patternPath;
    launchRequest_->effectPath = pattern.effectPath;
    launchRequest_->musicPath = song.audioPath;
    launchRequest_->mode = pattern.pattern.mode;
    if (!scenes.ChangeScene(finger_drum::scene_ids::RhythmTest))
    {
        throw std::runtime_error("Failed to enter transient gameplay.");
    }
    return true;
}

bool LobbyScene::ReturnToLogo(mrg::scene::SceneManager& scenes) const
{
    if (!scenes.ChangeScene(finger_drum::scene_ids::Logo))
    {
        throw std::runtime_error("Failed to return to the FingerDrum Logo.");
    }
    return true;
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
