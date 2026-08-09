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
    constexpr float DesignToCanvasScale = 2.0F / 3.0F;
    constexpr mrg::visual2d::Color CanvasBlue{0.918F, 0.965F, 1.0F, 1.0F};
    constexpr mrg::visual2d::Color PanelWhite{0.976F, 0.992F, 1.0F, 0.96F};
    constexpr mrg::visual2d::Color PureWhite{1.0F, 1.0F, 1.0F, 1.0F};
    constexpr mrg::visual2d::Color AccentBlue{0.310F, 0.624F, 0.878F, 1.0F};
    constexpr mrg::visual2d::Color LightBlue{0.780F, 0.918F, 0.980F, 1.0F};
    constexpr mrg::visual2d::Color DeepBlue{0.133F, 0.306F, 0.459F, 1.0F};
    constexpr mrg::visual2d::Color MutedBlue{0.412F, 0.537F, 0.635F, 1.0F};
    constexpr mrg::visual2d::Color SelectedArtistBlue{
        0.847F, 0.933F, 1.0F, 1.0F};
    constexpr mrg::visual2d::Color HeaderWhite{
        0.973F, 0.988F, 1.0F, 0.96F};
    constexpr mrg::visual2d::Color CategoryWhite{
        1.0F, 1.0F, 1.0F, 0.90F};
    constexpr mrg::visual2d::Color HeaderTextBlue{
        0.243F, 0.514F, 0.757F, 1.0F};
    constexpr mrg::visual2d::Color SelectedHoverBlue{
        0.365F, 0.690F, 0.930F, 1.0F};
    constexpr mrg::visual2d::Color UnselectedBlue{
        0.900F, 0.956F, 0.992F, 0.96F};
    constexpr mrg::visual2d::Color UnselectedHoverBlue{
        0.820F, 0.920F, 0.988F, 1.0F};
    constexpr mrg::visual2d::Color PressedBlue{
        0.190F, 0.490F, 0.790F, 1.0F};
    constexpr mrg::visual2d::Color DisabledBlue{
        0.70F, 0.78F, 0.84F, 0.55F};

    namespace layout
    {
        constexpr mrg::visual2d::Size CanvasSize{1280.0F, 720.0F};
        constexpr mrg::visual2d::Rect DesignBoard{
            0.0F, 0.0F, 1920.0F, 1080.0F};
        constexpr mrg::visual2d::Rect Header{
            0.0F, 0.0F, 1920.0F, 96.0F};
        constexpr mrg::visual2d::Rect BackButton{
            24.0F, 17.0F, 126.0F, 62.0F};
        constexpr mrg::visual2d::Rect HeaderSeparator{
            174.0F, 25.0F, 2.0F, 47.0F};
        constexpr mrg::visual2d::Rect HeaderTitle{
            198.0F, 29.0F, 340.0F, 38.0F};
        constexpr mrg::visual2d::Rect ProfileFrame{
            1770.0F, 8.0F, 80.0F, 80.0F};
        constexpr mrg::visual2d::Rect ProfileImage{
            4.0F, 4.0F, 72.0F, 72.0F};
        constexpr mrg::visual2d::Rect CategoryBar{
            54.0F, 112.0F, 1812.0F, 56.0F};
        constexpr mrg::visual2d::Rect AllCategory{
            16.0F, 9.0F, 86.0F, 38.0F};
        constexpr mrg::visual2d::Rect SongCount{
            1630.0F, 9.0F, 150.0F, 38.0F};
        constexpr mrg::visual2d::Rect RecordPanel{
            54.0F, 186.0F, 418.0F, 686.0F};
        constexpr mrg::visual2d::Rect RecordHeading{
            26.0F, 26.0F, 260.0F, 28.0F};
        constexpr mrg::visual2d::Rect EmptyRecordMessage{
            26.0F, 300.0F, 366.0F, 54.0F};
        constexpr mrg::visual2d::Rect SongInformationPanel{
            492.0F, 186.0F, 854.0F, 262.0F};
        constexpr mrg::visual2d::Rect SongInformationHeading{
            26.0F, 21.0F, 380.0F, 24.0F};
        constexpr mrg::visual2d::Rect SelectedSong{
            32.0F, 76.0F, 790.0F, 64.0F};
        constexpr mrg::visual2d::Rect SelectedArtist{
            32.0F, 152.0F, 790.0F, 42.0F};
        constexpr mrg::visual2d::Rect PatternPanel{
            492.0F, 468.0F, 854.0F, 404.0F};
        constexpr mrg::visual2d::Rect PatternHeading{
            26.0F, 22.0F, 300.0F, 30.0F};
        constexpr mrg::visual2d::Rect SelectedPattern{
            344.0F, 22.0F, 480.0F, 30.0F};
        constexpr mrg::visual2d::Rect PlayButton{
            620.0F, 322.0F, 204.0F, 56.0F};
        constexpr mrg::visual2d::Rect SongListPanel{
            1366.0F, 186.0F, 500.0F, 686.0F};
        constexpr mrg::visual2d::Rect SongListHeading{
            22.0F, 20.0F, 300.0F, 28.0F};
        constexpr mrg::visual2d::Rect EmptySongListMessage{
            20.0F, 300.0F, 460.0F, 58.0F};
        constexpr mrg::visual2d::Rect SongRowTitle{
            18.0F, 12.0F, 424.0F, 34.0F};
        constexpr mrg::visual2d::Rect SongRowArtist{
            18.0F, 53.0F, 424.0F, 26.0F};
        constexpr mrg::visual2d::Rect FooterLeft{
            54.0F, 1040.0F, 880.0F, 24.0F};
        constexpr mrg::visual2d::Rect FooterRight{
            986.0F, 1040.0F, 880.0F, 24.0F};
        constexpr float SongRowTop = 72.0F;
        constexpr float SongRowHeight = 96.0F;
        constexpr float SongRowStep = 112.0F;
        constexpr float SongRowWidth = 460.0F;
        constexpr float PatternRowTop = 74.0F;
        constexpr float PatternRowHeight = 58.0F;
        constexpr float PatternRowStep = 72.0F;
        constexpr float PatternRowWidth = 796.0F;
        constexpr float HeaderButtonFontSize = 15.0F;
        constexpr float HeaderTitleFontSize = 16.0F;
        constexpr float PanelHeadingFontSize = 14.0F;
        constexpr float CategoryFontSize = 13.0F;
        constexpr float SongCountFontSize = 11.0F;
        constexpr float InformationHeadingFontSize = 12.0F;
        constexpr float EmptyRecordFontSize = 22.0F;
        constexpr float EmptySongListFontSize = 21.0F;
        constexpr float SelectedSongFontSize = 28.0F;
        constexpr float SelectedArtistFontSize = 16.0F;
        constexpr float SelectedPatternFontSize = 13.0F;
        constexpr float PlayButtonFontSize = 17.0F;
        constexpr float SongRowTitleFontSize = 17.0F;
        constexpr float SongRowArtistFontSize = 13.0F;
        constexpr float PatternButtonFontSize = 15.0F;
        constexpr float FooterFontSize = 11.0F;
        constexpr std::size_t SongMarqueeThreshold = 38;
        constexpr std::size_t ArtistMarqueeThreshold = 52;
        constexpr std::size_t SongRowMarqueeThreshold = 34;
        constexpr double SongMarqueeSpeed = 0.28;
        constexpr double ArtistMarqueeSpeed = 0.32;
        constexpr double SongRowMarqueeSpeed = 0.24;

        [[nodiscard]] constexpr mrg::visual2d::Rect SongRow(
            const std::size_t index) noexcept
        {
            return {
                20.0F,
                SongRowTop + SongRowStep * static_cast<float>(index),
                SongRowWidth,
                SongRowHeight};
        }

        [[nodiscard]] constexpr mrg::visual2d::Rect PatternRow(
            const std::size_t index) noexcept
        {
            return {
                28.0F,
                PatternRowTop + PatternRowStep * static_cast<float>(index),
                PatternRowWidth,
                PatternRowHeight};
        }
    }

    [[nodiscard]] mrg::visual2d::Rect Scale(
        const mrg::visual2d::Rect bounds) noexcept
    {
        return {
            bounds.x * DesignToCanvasScale,
            bounds.y * DesignToCanvasScale,
            bounds.width * DesignToCanvasScale,
            bounds.height * DesignToCanvasScale};
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
            ? AccentBlue
            : UnselectedBlue;
        style.hovered = selected
            ? SelectedHoverBlue
            : UnselectedHoverBlue;
        style.pressed = PressedBlue;
        style.disabled = DisabledBlue;
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
        layout::CanvasSize,
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
    board_->SetSize(layout::CanvasSize);
    board_->SetPosition({0.0F, 0.0F});

    AddPanel(
        *board_,
        layout::DesignBoard,
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
    songRows_.clear();
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
        layout::Header,
        HeaderWhite,
        "Header");
    auto& back = mrg::visual2d::CreateButton(
        header, Scale(layout::BackButton), L"< BACK", "Back");
    backButtonId_ = back.Id();
    ApplySelectableStyle(back, false);
    RequireComponent<mrg::visual2d::TextVisualComponent>(back).
        SetFontSize(layout::HeaderButtonFontSize);
    AddPanel(header, layout::HeaderSeparator, LightBlue, "Separator");
    AddLabel(
        header,
        layout::HeaderTitle,
        L"FINGERDRUM / MUSIC SELECT",
        layout::HeaderTitleFontSize,
        HeaderTextBlue,
        "Title");

    auto& frame = AddPanel(
        header,
        layout::ProfileFrame,
        AccentBlue,
        "TemporaryProfileFrame");
    auto& profile = mrg::visual2d::CreateSprite(
        frame,
        Scale(layout::ProfileImage),
        services.visual2DRendering.LoadImage(ProfileImagePath()),
        "TemporaryProfileImage");
    profile.SetZIndex(1);
}

void LobbyScene::CreateCategoryBar()
{
    auto& bar = AddPanel(
        *board_,
        layout::CategoryBar,
        CategoryWhite,
        "Categories");
    AddPanel(bar, layout::AllCategory, AccentBlue, "AllCategory");
    AddLabel(
        bar,
        layout::AllCategory,
        L"ALL",
        layout::CategoryFontSize,
        PureWhite,
        "AllCategoryText",
        mrg::visual2d::TextAlignment::Center);
    AddLabel(
        bar,
        layout::SongCount,
        std::format(L"{} SONGS", catalog_.songs.size()),
        layout::SongCountFontSize,
        MutedBlue,
        "SongCount",
        mrg::visual2d::TextAlignment::Trailing);
}

void LobbyScene::CreateRecordPanel()
{
    auto& panel = AddPanel(
        *board_,
        layout::RecordPanel,
        PanelWhite,
        "RecordPanel");
    AddLabel(panel, layout::RecordHeading, L"LOCAL RECORD",
        layout::PanelHeadingFontSize, MutedBlue, "Heading");
    AddLabel(panel, layout::EmptyRecordMessage, L"NO RECORDS",
        layout::EmptyRecordFontSize, DeepBlue, "NoRecords",
        mrg::visual2d::TextAlignment::Center);
}

void LobbyScene::CreateSongInformationPanel()
{
    auto& panel = AddPanel(
        *board_,
        layout::SongInformationPanel,
        PanelWhite,
        "SongInformation");
    AddLabel(panel, layout::SongInformationHeading, L"NOW SELECTING",
        layout::InformationHeadingFontSize, MutedBlue, "Heading");
    selectedSongLabel_ = &AddLabel(
        panel, layout::SelectedSong, L"NO SONG SELECTED",
        layout::SelectedSongFontSize, DeepBlue, "SelectedSong",
        mrg::visual2d::TextAlignment::Center);
    selectedSongLabel_->AddComponent<
        finger_drum::presentation::MarqueeTextComponent>(
            L"NO SONG SELECTED",
            layout::SongMarqueeThreshold,
            layout::SongMarqueeSpeed);
    selectedArtistLabel_ = &AddLabel(
        panel, layout::SelectedArtist, L"",
        layout::SelectedArtistFontSize, MutedBlue, "SelectedArtist",
        mrg::visual2d::TextAlignment::Center);
    selectedArtistLabel_->AddComponent<
        finger_drum::presentation::MarqueeTextComponent>(
            L"",
            layout::ArtistMarqueeThreshold,
            layout::ArtistMarqueeSpeed);
}

void LobbyScene::CreatePatternPanel()
{
    auto& panel = AddPanel(
        *board_,
        layout::PatternPanel,
        PanelWhite,
        "PatternPanel");
    AddLabel(panel, layout::PatternHeading, L"PATTERN SELECT",
        layout::PanelHeadingFontSize, MutedBlue, "Heading");
    selectedPatternLabel_ = &AddLabel(
        panel, layout::SelectedPattern, L"NO PATTERN",
        layout::SelectedPatternFontSize, DeepBlue, "SelectedPattern",
        mrg::visual2d::TextAlignment::Trailing);
    patternList_ = &panel.CreateChild("PatternList");
    patternList_->SetSize({
        layout::PatternPanel.width * DesignToCanvasScale,
        layout::PatternPanel.height * DesignToCanvasScale});
    auto& play = mrg::visual2d::CreateButton(
        panel, Scale(layout::PlayButton), L"PLAY", "Play");
    playButtonId_ = play.Id();
    ApplySelectableStyle(play, true);
    RequireComponent<mrg::visual2d::TextVisualComponent>(play).
        SetFontSize(layout::PlayButtonFontSize);
}

void LobbyScene::CreateSongList()
{
    auto& panel = AddPanel(
        *board_,
        layout::SongListPanel,
        PanelWhite,
        "SongList");
    AddLabel(panel, layout::SongListHeading, L"SONG LIST",
        layout::PanelHeadingFontSize, MutedBlue, "Heading");

    if (catalog_.songs.empty())
    {
        AddLabel(panel, layout::EmptySongListMessage,
            L"NO SONGS AVAILABLE", layout::EmptySongListFontSize,
            DeepBlue, "NoSongs",
            mrg::visual2d::TextAlignment::Center);
        return;
    }

    for (std::size_t index = 0; index < catalog_.songs.size(); ++index)
    {
        CreateSongListRow(panel, index, catalog_.songs[index]);
    }
}

void LobbyScene::CreateSongListRow(
    mrg::visual2d::Visual2DNode& parent,
    const std::size_t index,
    const finger_drum::chart::SongCatalogEntry& song)
{
    // Keep interaction on the row background while title and artist remain
    // separate presentation nodes. This avoids formatting metadata as one
    // hard-coded multiline button label and makes each text style independent.
    auto& button = mrg::visual2d::CreateButton(
        parent,
        Scale(layout::SongRow(index)),
        L"",
        std::format("Song.{}", index));
    const std::wstring titleText = SongTitle(song);
    const std::wstring artistText = SongArtist(song);
    auto& title = AddLabel(
        button,
        layout::SongRowTitle,
        titleText,
        layout::SongRowTitleFontSize,
        DeepBlue,
        "Title");
    auto& artist = AddLabel(
        button,
        layout::SongRowArtist,
        artistText,
        layout::SongRowArtistFontSize,
        MutedBlue,
        "Artist");
    title.AddComponent<finger_drum::presentation::MarqueeTextComponent>(
        titleText,
        layout::SongRowMarqueeThreshold,
        layout::SongRowMarqueeSpeed);
    songRows_.push_back({&button, &title, &artist});
}

void LobbyScene::ApplySongListRowStyle(
    SongListRow& row,
    const bool selected)
{
    if (row.button == nullptr || row.title == nullptr || row.artist == nullptr)
    {
        throw std::logic_error("A song-list row is incomplete.");
    }

    ApplySelectableStyle(*row.button, selected);
    RequireComponent<mrg::visual2d::TextVisualComponent>(*row.title).
        SetTextColor(selected ? PureWhite : DeepBlue);
    RequireComponent<mrg::visual2d::TextVisualComponent>(*row.artist).
        SetTextColor(selected ? SelectedArtistBlue : MutedBlue);
}

void LobbyScene::CreateFooter()
{
    AddLabel(*board_, layout::FooterLeft,
        L"UP / DOWN : SONG    LEFT / RIGHT : PATTERN    ENTER : PLAY",
        layout::FooterFontSize, MutedBlue, "FooterLeft");
    AddLabel(*board_, layout::FooterRight,
        L"ESC : BACK", layout::FooterFontSize, MutedBlue, "FooterRight",
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
            Scale(layout::PatternRow(index)),
            name,
            std::format("Pattern.{}", index));
        RequireComponent<mrg::visual2d::TextVisualComponent>(button).
            SetFontSize(layout::PatternButtonFontSize);
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
    for (std::size_t index = 0; index < songRows_.size(); ++index)
    {
        ApplySongListRowStyle(songRows_[index], index == selectedSongIndex_);
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
        for (std::size_t index = 0; index < songRows_.size(); ++index)
        {
            if (songRows_[index].button != nullptr &&
                action.source == songRows_[index].button->Id())
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
    textComponent.SetFontSize(penpotFontSize * DesignToCanvasScale);
    textComponent.SetTextColor(color);
    textComponent.SetHorizontalAlignment(alignment);
    return label;
}
