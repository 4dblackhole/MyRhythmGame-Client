#include "LobbyScene.h"

#include "GameFlow/FingerDrumSceneIds.h"
#include "Presentation/MarqueeTextComponent.h"
#include "Taiko/TaikoMode.h"

#include <Windows.h>

#include <algorithm>
#include <cmath>
#include <cwctype>
#include <filesystem>
#include <format>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <utility>

namespace
{
    constexpr float DesignToCanvasScale = 2.0F / 3.0F;
    constexpr double PreviewFadeSeconds = 0.2;

    [[nodiscard]] constexpr float ReadableFontScale(
        const float penpotFontSize) noexcept
    {
        if (penpotFontSize <= 10.0F)
        {
            return 1.45F;
        }
        if (penpotFontSize <= 13.0F)
        {
            return 1.30F;
        }
        if (penpotFontSize <= 18.0F)
        {
            return 1.15F;
        }
        if (penpotFontSize <= 20.0F)
        {
            return 1.08F;
        }
        return 1.0F;
    }

    [[nodiscard]] constexpr float CanvasFontSize(
        const float penpotFontSize) noexcept
    {
        return penpotFontSize * DesignToCanvasScale *
            ReadableFontScale(penpotFontSize);
    }
    constexpr mrg::visual2d::Color CanvasBlue{0.918F, 0.965F, 1.0F, 1.0F};
    constexpr mrg::visual2d::Color PanelWhite{0.976F, 0.992F, 1.0F, 0.98F};
    constexpr mrg::visual2d::Color PureWhite{1.0F, 1.0F, 1.0F, 1.0F};
    constexpr mrg::visual2d::Color AccentBlue{0.310F, 0.624F, 0.878F, 1.0F};
    constexpr mrg::visual2d::Color AccentHover{0.365F, 0.690F, 0.930F, 1.0F};
    constexpr mrg::visual2d::Color BorderBlue{0.310F, 0.624F, 0.878F, 1.0F};
    constexpr mrg::visual2d::Color SubtleBorder{0.569F, 0.769F, 0.890F, 1.0F};
    constexpr mrg::visual2d::Color PaleBorder{0.725F, 0.851F, 0.929F, 1.0F};
    constexpr mrg::visual2d::Color PaleBlue{0.937F, 0.973F, 0.996F, 1.0F};
    constexpr mrg::visual2d::Color ViewportBlue{0.933F, 0.973F, 0.992F, 0.62F};
    constexpr mrg::visual2d::Color FocusBlue{0.906F, 0.965F, 1.0F, 1.0F};
    constexpr mrg::visual2d::Color RowBlue{0.929F, 0.973F, 0.996F, 1.0F};
    constexpr mrg::visual2d::Color DeepBlue{0.157F, 0.325F, 0.455F, 1.0F};
    constexpr mrg::visual2d::Color MutedBlue{0.290F, 0.471F, 0.592F, 1.0F};
    constexpr mrg::visual2d::Color SoftTextBlue{0.455F, 0.659F, 0.784F, 1.0F};
    constexpr mrg::visual2d::Color DisabledBlue{0.70F, 0.78F, 0.84F, 0.55F};

    namespace layout
    {
        constexpr mrg::visual2d::Size CanvasSize{1280.0F, 720.0F};
        constexpr mrg::visual2d::Rect DesignBoard{0.0F, 0.0F, 1920.0F, 1080.0F};

        constexpr mrg::visual2d::Rect CategoryBar{54.0F, 20.0F, 1285.71F, 56.0F};
        constexpr mrg::visual2d::Rect AllCategory{16.0F, 9.0F, 86.0F, 38.0F};

        constexpr mrg::visual2d::Rect RecordPanel{54.0F, 94.0F, 506.29F, 852.0F};
        constexpr mrg::visual2d::Rect RecordSelector{22.0F, 22.0F, 462.29F, 44.0F};
        constexpr mrg::visual2d::Rect EmptyRecordMessage{22.0F, 400.0F, 462.29F, 54.0F};

        constexpr mrg::visual2d::Rect InformationPanel{580.29F, 94.0F, 759.43F, 852.0F};
        constexpr mrg::visual2d::Rect Preview{24.0F, 24.0F, 360.0F, 360.0F};
        constexpr mrg::visual2d::Rect PreviewTitle{54.0F, 190.0F, 300.0F, 28.0F};
        constexpr mrg::visual2d::Rect PreviewEmpty{54.0F, 224.0F, 300.0F, 20.0F};
        constexpr mrg::visual2d::Rect SelectedSong{404.0F, 38.0F, 331.43F, 38.0F};
        constexpr mrg::visual2d::Rect SelectedArtist{404.0F, 80.0F, 331.43F, 24.0F};
        constexpr mrg::visual2d::Rect DifficultyInformation{404.0F, 136.0F, 331.43F, 126.0F};
        constexpr mrg::visual2d::Rect DifficultyHeading{16.0F, 14.0F, 145.0F, 16.0F};
        constexpr mrg::visual2d::Rect CreatorHeading{169.71F, 14.0F, 145.0F, 16.0F};
        constexpr mrg::visual2d::Rect SelectedDifficulty{16.0F, 32.0F, 145.0F, 28.0F};
        constexpr mrg::visual2d::Rect SelectedCreator{169.71F, 32.0F, 145.0F, 28.0F};
        constexpr mrg::visual2d::Rect InformationDivider{16.0F, 70.0F, 299.43F, 1.0F};
        constexpr mrg::visual2d::Rect SelectedDetails{16.0F, 86.0F, 299.43F, 22.0F};

        constexpr mrg::visual2d::Rect BrowserPanel{1359.71F, 20.0F, 506.29F, 926.0F};
        constexpr mrg::visual2d::Rect SearchField{14.0F, 22.0F, 478.29F, 54.0F};
        constexpr mrg::visual2d::Rect SearchCount{365.0F, 0.0F, 96.0F, 54.0F};
        constexpr mrg::visual2d::Rect SortSelector{14.0F, 88.0F, 478.29F, 42.0F};
        constexpr mrg::visual2d::Rect SongViewport{14.0F, 142.0F, 478.29F, 734.0F};
        constexpr mrg::visual2d::Rect EmptySongMessage{28.0F, 320.0F, 422.29F, 46.0F};
        constexpr mrg::visual2d::Rect BrowserHint{18.0F, 884.0F, 470.29F, 22.0F};

        constexpr float ContentLeft = 8.0F;
        constexpr float ContentTop = 14.0F;
        constexpr float ContentBottom = 720.0F;
        constexpr float ViewportVisibleHeight = ContentBottom - ContentTop;
        constexpr float SongGap = 12.0F;
        constexpr float NormalSongHeight = 72.0F;
        constexpr float ExpandedBaseHeight = 116.0F;
        constexpr float DifficultyRowHeight = 34.0F;
        constexpr float DifficultyRowStep = 42.0F;
        constexpr float ScrollStep = 84.0F;
        constexpr mrg::visual2d::Rect ScrollbarTrack{469.29F, 14.0F, 5.0F, 706.0F};

        constexpr mrg::visual2d::Rect OptionButton{612.0F, 972.0F, 696.0F, 60.0F};
        constexpr mrg::visual2d::Rect BackButton{-24.0F, 1006.0F, 280.0F, 82.0F};
        constexpr mrg::visual2d::Rect BackLabel{34.0F, 10.0F, 210.0F, 44.0F};
        constexpr mrg::visual2d::Rect GoButton{1664.0F, 1006.0F, 280.0F, 82.0F};
        constexpr mrg::visual2d::Rect GoLabel{36.0F, 10.0F, 208.0F, 44.0F};
    }

    struct ResponsiveSongSelectLayout
    {
        mrg::visual2d::Rect background;
        mrg::visual2d::Rect category;
        mrg::visual2d::Rect record;
        mrg::visual2d::Rect information;
        mrg::visual2d::Rect browser;
        mrg::visual2d::Rect option;
        mrg::visual2d::Rect back;
        mrg::visual2d::Rect go;
    };

    [[nodiscard]] constexpr mrg::visual2d::Rect CanvasTopLeftBounds(
        const float x,
        const float top,
        const float width,
        const float height) noexcept
    {
        return {x, layout::CanvasSize.height - top - height, width, height};
    }

    [[nodiscard]] constexpr ResponsiveSongSelectLayout CalculateResponsiveLayout(
        const float logicalWidth) noexcept
    {
        constexpr float ReferenceOuterMargin = 54.0F * DesignToCanvasScale;
        constexpr float ReferenceColumnGap = 20.0F * DesignToCanvasScale;
        constexpr float ReferenceSideWidth =
            layout::RecordPanel.width * DesignToCanvasScale;
        constexpr float ReferenceInformationWidth =
            layout::InformationPanel.width * DesignToCanvasScale;
        constexpr float ReferenceColumnWidth =
            ReferenceSideWidth * 2.0F + ReferenceInformationWidth;

        // Keep the Penpot panel widths at 16:9 and wider. For narrower aspect
        // ratios, shrink the complete three-column composition uniformly so
        // its ownership and ordering remain unambiguous even in portrait.
        const float chromeScale = std::min(1.0F, logicalWidth / 960.0F);
        const float outerMargin = ReferenceOuterMargin * chromeScale;
        const float columnGap = ReferenceColumnGap * chromeScale;
        const float availableWidth = std::max(
            logicalWidth - outerMargin * 2.0F - columnGap * 2.0F,
            1.0F);
        const float columnScale = std::min(
            1.0F, availableWidth / ReferenceColumnWidth);
        const float sideWidth = ReferenceSideWidth * columnScale;
        const float informationWidth = std::max(
            availableWidth - sideWidth * 2.0F,
            1.0F);
        const float recordX = outerMargin;
        const float informationX = recordX + sideWidth + columnGap;
        const float browserX = logicalWidth - outerMargin - sideWidth;

        constexpr float MainTop = 94.0F * DesignToCanvasScale;
        constexpr float MainHeight = 852.0F * DesignToCanvasScale;
        constexpr float BrowserTop = 20.0F * DesignToCanvasScale;
        constexpr float BrowserHeight = 926.0F * DesignToCanvasScale;
        constexpr float CategoryHeight = 56.0F * DesignToCanvasScale;
        const float categoryWidth = std::max(
            browserX - columnGap - recordX,
            1.0F);

        const float footerScale = std::min(1.0F, logicalWidth / 640.0F);
        const float edgeWidth = layout::BackButton.width *
            DesignToCanvasScale * footerScale;
        const float edgeOverflow = -layout::BackButton.x *
            DesignToCanvasScale * footerScale;
        const float footerGap = ReferenceColumnGap * chromeScale;
        const float optionWidth = std::min(
            layout::OptionButton.width * DesignToCanvasScale,
            std::max(
                logicalWidth -
                    2.0F * (edgeWidth - edgeOverflow + footerGap),
                1.0F));
        const float optionX = (logicalWidth - optionWidth) * 0.5F;

        return {
            {0.0F, 0.0F, logicalWidth, layout::CanvasSize.height},
            CanvasTopLeftBounds(
                recordX, BrowserTop, categoryWidth, CategoryHeight),
            CanvasTopLeftBounds(
                recordX, MainTop, sideWidth, MainHeight),
            CanvasTopLeftBounds(
                informationX, MainTop, informationWidth, MainHeight),
            CanvasTopLeftBounds(
                browserX, BrowserTop, sideWidth, BrowserHeight),
            CanvasTopLeftBounds(
                optionX,
                layout::OptionButton.y * DesignToCanvasScale,
                optionWidth,
                layout::OptionButton.height * DesignToCanvasScale),
            CanvasTopLeftBounds(
                -edgeOverflow,
                layout::BackButton.y * DesignToCanvasScale,
                edgeWidth,
                layout::BackButton.height * DesignToCanvasScale),
            CanvasTopLeftBounds(
                logicalWidth - edgeWidth + edgeOverflow,
                layout::GoButton.y * DesignToCanvasScale,
                edgeWidth,
                layout::GoButton.height * DesignToCanvasScale)};
    }

    constexpr ResponsiveSongSelectLayout PortraitLayout =
        CalculateResponsiveLayout(405.0F);
    static_assert(PortraitLayout.record.x >= 0.0F);
    static_assert(
        PortraitLayout.record.x + PortraitLayout.record.width <
        PortraitLayout.information.x);
    static_assert(
        PortraitLayout.information.x + PortraitLayout.information.width <
        PortraitLayout.browser.x);
    static_assert(
        PortraitLayout.browser.x + PortraitLayout.browser.width <= 405.0F);
    static_assert(
        PortraitLayout.back.x + PortraitLayout.back.width <
        PortraitLayout.option.x);
    static_assert(
        PortraitLayout.option.x + PortraitLayout.option.width <
        PortraitLayout.go.x);

    constexpr ResponsiveSongSelectLayout UltrawideLayout =
        CalculateResponsiveLayout(1680.0F);
    static_assert(
        UltrawideLayout.information.width >
        layout::InformationPanel.width * DesignToCanvasScale);
    static_assert(
        UltrawideLayout.option.x + UltrawideLayout.option.width * 0.5F ==
        840.0F);

    [[nodiscard]] mrg::visual2d::Rect ScaleTopLeftBounds(
        const mrg::visual2d::Rect bounds,
        const float parentHeight) noexcept
    {
        const float scaledHeight = bounds.height * DesignToCanvasScale;
        return {
            bounds.x * DesignToCanvasScale,
            parentHeight - bounds.y * DesignToCanvasScale - scaledHeight,
            bounds.width * DesignToCanvasScale,
            scaledHeight};
    }

    [[nodiscard]] std::filesystem::path RuntimeSongsPath()
    {
        return mrg::platform::ResolveExecutableRelativePath(L"assets\\songs");
    }

    [[nodiscard]] std::wstring DecodeDisplayText(const std::string_view value)
    {
        if (value.empty())
        {
            return {};
        }
        UINT codePage = CP_UTF8;
        DWORD flags = MB_ERR_INVALID_CHARS;
        int length = MultiByteToWideChar(
            codePage, flags, value.data(), static_cast<int>(value.size()), nullptr, 0);
        if (length <= 0)
        {
            codePage = CP_ACP;
            flags = 0;
            length = MultiByteToWideChar(
                codePage, flags, value.data(), static_cast<int>(value.size()), nullptr, 0);
        }
        if (length <= 0)
        {
            return L"?";
        }
        std::wstring result(static_cast<std::size_t>(length), L'\0');
        MultiByteToWideChar(
            codePage, flags, value.data(), static_cast<int>(value.size()), result.data(), length);
        return result;
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
            ? L"—"
            : DecodeDisplayText(song.music.artists.front());
    }

    [[nodiscard]] std::wstring PatternName(
        const finger_drum::chart::SongCatalogPattern& pattern)
    {
        return pattern.pattern.name.empty()
            ? pattern.patternPath.stem().wstring()
            : DecodeDisplayText(pattern.pattern.name);
    }

    [[nodiscard]] std::wstring Lowercase(std::wstring value)
    {
        std::ranges::transform(
            value,
            value.begin(),
            [](const wchar_t character)
            {
                return static_cast<wchar_t>(std::towlower(character));
            });
        return value;
    }

    [[nodiscard]] bool MatchesSearch(
        const finger_drum::chart::SongCatalogEntry& song,
        const std::wstring& searchText)
    {
        if (searchText.empty())
        {
            return true;
        }
        const std::wstring needle = Lowercase(searchText);
        return Lowercase(SongTitle(song)).find(needle) != std::wstring::npos ||
            Lowercase(SongArtist(song)).find(needle) != std::wstring::npos;
    }

    [[nodiscard]] std::wstring BpmText(const double bpm)
    {
        std::wstring result = std::format(L"{:.2f}", bpm);
        while (result.ends_with(L'0'))
        {
            result.pop_back();
        }
        if (result.ends_with(L'.'))
        {
            result.pop_back();
        }
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
                "Song-select node '" + node.Name() +
                "' is missing a required component.");
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

    void ApplySpriteStyle(
        mrg::visual2d::Visual2DNode& node,
        const mrg::visual2d::Color normal,
        const mrg::visual2d::Color hovered)
    {
        mrg::visual2d::VisualStyle style{};
        style.normal = normal;
        style.hovered = hovered;
        style.pressed = AccentBlue;
        style.disabled = DisabledBlue;
        RequireComponent<mrg::visual2d::SpriteVisualComponent>(node).
            SetStyle(style);
    }

    void ApplyButtonStyle(
        mrg::visual2d::Visual2DNode& node,
        const mrg::visual2d::Color normal,
        const mrg::visual2d::Color hovered,
        const mrg::visual2d::Color textColor)
    {
        ApplySpriteStyle(node, normal, hovered);
        RequireComponent<mrg::visual2d::TextVisualComponent>(node).
            SetTextColor(textColor);
    }

    void SetCornerRadius(
        mrg::visual2d::Visual2DNode& node,
        const float penpotRadius)
    {
        RequireComponent<mrg::visual2d::SpriteVisualComponent>(node).
            SetCornerRadius(penpotRadius * DesignToCanvasScale);
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
    mrg::visual2d::ScreenVisual2DManager& screenVisuals,
    mrg::audio::AudioPlaybackManager& audioPlayback,
    std::shared_ptr<finger_drum::GameplayLaunchRequest> launchRequest)
    : launchRequest_(std::move(launchRequest)),
      screenVisuals_(screenVisuals),
      audioPlayback_(audioPlayback)
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
    audioSystem_ = &services.audio;
    catalog_ = finger_drum::chart::SongCatalog{}.Load(RuntimeSongsPath());
    if (catalog_.songs.empty() && !catalog_.diagnostics.empty())
    {
        launchError_ = L"CATALOG ERROR: " + DecodeDisplayText(
            catalog_.diagnostics.front().message);
    }
    canvasHandle_ = screenVisuals_.CreateOwnedCanvas({
        layout::CanvasSize,
        mrg::visual2d::CanvasScaleMode::FixedHeight});
    canvas_ = canvasHandle_.Get();
    if (canvas_ == nullptr)
    {
        throw std::runtime_error("Failed to create the Lobby screen Canvas.");
    }

    board_ = &canvas_->CreateNode(
        mrg::visual2d::Anchor::Center,
        "FingerDrum.SongSelect.Board");
    board_->SetPivot({0.5F, 0.5F});
    board_->SetSize(layout::CanvasSize);
    board_->SetPosition({0.0F, 0.0F});

    background_ = &AddPanel(
        *board_, layout::DesignBoard, CanvasBlue, "Background");
    CreateCategoryBar();
    CreateRecordPanel();
    CreateSongInformationPanel();
    CreateSongBrowser();
    CreateFooter();
    UpdateResponsiveLayout();
    RebuildVisibleSongs();
}

void LobbyScene::BeginScene()
{
    sceneActive_ = true;
    static_cast<void>(canvasHandle_.SetVisible(true));
    SyncPreviewToFocusedSong();
}

void LobbyScene::EndScene() noexcept
{
    sceneActive_ = false;
    StopPreviewAudio();
    static_cast<void>(canvasHandle_.SetVisible(false));
}

void LobbyScene::Update(
    const mrg::UpdateContext& context,
    mrg::scene::SceneManager& scenes)
{
    if (canvas_ == nullptr)
    {
        return;
    }
    UpdatePreviewAudio(context.deltaSeconds);
    ProcessPointer(context.input);
    if (ProcessActions(scenes) || ProcessKeyboard(context.input, scenes))
    {
        return;
    }
}

void LobbyScene::Render(const mrg::graphics::RenderContext&)
{
}

void LobbyScene::OnResize(
    const std::uint32_t width,
    const std::uint32_t height)
{
    width_ = width;
    height_ = height;
    if (canvas_ != nullptr)
    {
        UpdateResponsiveLayout();
        RebuildSongCards();
        EnsureFocusedCardVisible();
        inputRouter_.InvalidateHitTest();
    }
}

void LobbyScene::Shutdown() noexcept
{
    StopPreviewAudio();
    audioSystem_ = nullptr;
    if (canvas_ != nullptr)
    {
        inputRouter_.Reset(*canvas_);
    }
    difficultyButtonIds_.clear();
    songCards_.clear();
    visibleSongIndices_.clear();
    goLabel_ = nullptr;
    goFill_ = nullptr;
    goButton_ = nullptr;
    backLabel_ = nullptr;
    backFill_ = nullptr;
    backButton_ = nullptr;
    optionLabel_ = nullptr;
    optionButton_ = nullptr;
    selectedDetailsLabel_ = nullptr;
    selectedCreatorLabel_ = nullptr;
    selectedPatternLabel_ = nullptr;
    selectedArtistLabel_ = nullptr;
    selectedSongLabel_ = nullptr;
    browserHint_ = nullptr;
    sortSelector_ = nullptr;
    searchCountLabel_ = nullptr;
    searchField_ = nullptr;
    scrollbarHandle_ = nullptr;
    scrollbarTrack_ = nullptr;
    songContent_ = nullptr;
    songViewport_ = nullptr;
    browserPanel_ = nullptr;
    informationDivider_ = nullptr;
    creatorHeading_ = nullptr;
    difficultyHeading_ = nullptr;
    difficultyInformation_ = nullptr;
    previewEmpty_ = nullptr;
    previewTitle_ = nullptr;
    preview_ = nullptr;
    informationPanel_ = nullptr;
    emptyRecordMessage_ = nullptr;
    recordSelector_ = nullptr;
    recordPanel_ = nullptr;
    categoryBar_ = nullptr;
    background_ = nullptr;
    board_ = nullptr;
    canvas_ = nullptr;
    canvasHandle_.Reset();
    catalog_ = {};
}

void LobbyScene::CreateCategoryBar()
{
    auto& bar = AddBorderedPanel(
        *board_, layout::CategoryBar, PanelWhite, BorderBlue,
        3.0F, 20.0F, "Categories");
    categoryBar_ = &bar;
    auto& all = AddPanel(
        bar, layout::AllCategory, AccentBlue, "AllCategory", 19.0F);
    AddLabel(
        all,
        {0.0F, 0.0F, 86.0F, 38.0F},
        L"ALL", 13.0F, PureWhite, "AllCategoryText",
        mrg::visual2d::TextAlignment::Center);
}

void LobbyScene::CreateRecordPanel()
{
    auto& panel = AddBorderedPanel(
        *board_, layout::RecordPanel, PanelWhite, BorderBlue,
        3.0F, 24.0F, "RecordPanel");
    recordPanel_ = &panel;
    auto& selector = mrg::visual2d::CreateComboBox(
        panel,
        ScaleTopLeftBounds(layout::RecordSelector, panel.NodeSize().height),
        {L"PERSONAL RECORD"},
        "RecordSelector");
    recordSelector_ = &selector;
    recordSelectorId_ = selector.Id();
    ApplySpriteStyle(selector, PaleBlue, PaleBlue);
    SetCornerRadius(selector, 10.0F);
    auto& recordBehavior = RequireComponent<
        mrg::visual2d::ComboBoxBehaviorComponent>(selector);
    recordBehavior.SetFontSize(CanvasFontSize(10.0F));
    recordBehavior.SetTextColor(DeepBlue);
    recordBehavior.SetSelectedTextColor(PureWhite);
    recordBehavior.SetPopupBackgroundColor(PureWhite);
    emptyRecordMessage_ = &AddLabel(
        panel, layout::EmptyRecordMessage, L"NO RECORDS",
        28.0F, DeepBlue, "NoRecords",
        mrg::visual2d::TextAlignment::Center);
}

void LobbyScene::CreateSongInformationPanel()
{
    auto& panel = AddBorderedPanel(
        *board_, layout::InformationPanel, PanelWhite, BorderBlue,
        3.0F, 24.0F, "SongInformation");
    informationPanel_ = &panel;
    auto& preview = AddPanel(
        panel, layout::Preview,
        {0.765F, 0.906F, 0.980F, 1.0F},
        "BackgroundPreview", 12.0F);
    preview_ = &preview;
    previewTitle_ = &AddLabel(
        preview, layout::PreviewTitle, L"BACKGROUND PREVIEW",
        13.0F, MutedBlue, "PreviewTitle",
        mrg::visual2d::TextAlignment::Center);
    previewEmpty_ = &AddLabel(
        preview, layout::PreviewEmpty, L"NO IMAGE",
        10.0F, SoftTextBlue, "PreviewEmpty",
        mrg::visual2d::TextAlignment::Center);

    selectedSongLabel_ = &AddLabel(
        panel, layout::SelectedSong, L"NO SONG SELECTED",
        28.0F, DeepBlue, "SelectedSong");
    selectedSongLabel_->AddComponent<
        finger_drum::presentation::MarqueeTextComponent>(
            L"NO SONG SELECTED", 24, 0.28);
    selectedArtistLabel_ = &AddLabel(
        panel, layout::SelectedArtist, L"",
        16.0F, MutedBlue, "SelectedArtist");
    selectedArtistLabel_->AddComponent<
        finger_drum::presentation::MarqueeTextComponent>(L"", 34, 0.32);

    auto& information = AddBorderedPanel(
        panel, layout::DifficultyInformation, PaleBlue, PaleBorder,
        1.0F, 12.0F, "DifficultyInformation");
    difficultyInformation_ = &information;
    difficultyHeading_ = &AddLabel(
        information, layout::DifficultyHeading, L"DIFFICULTY",
        9.0F, SoftTextBlue, "DifficultyHeading");
    creatorHeading_ = &AddLabel(
        information, layout::CreatorHeading, L"CREATOR",
        9.0F, SoftTextBlue, "CreatorHeading");
    selectedPatternLabel_ = &AddLabel(
        information, layout::SelectedDifficulty, L"—",
        18.0F, AccentBlue, "SelectedDifficulty");
    selectedCreatorLabel_ = &AddLabel(
        information, layout::SelectedCreator, L"—",
        18.0F, DeepBlue, "SelectedCreator");
    informationDivider_ = &AddPanel(
        information, layout::InformationDivider,
        PaleBorder, "InformationDivider");
    selectedDetailsLabel_ = &AddLabel(
        information, layout::SelectedDetails,
        L"LEVEL —   BPM —   NOTES —   MODE —",
        9.0F, MutedBlue, "SelectedDetails",
        mrg::visual2d::TextAlignment::Center);
}

void LobbyScene::CreateSongBrowser()
{
    auto& panel = AddBorderedPanel(
        *board_, layout::BrowserPanel, PanelWhite, BorderBlue,
        3.0F, 24.0F, "SongBrowser");
    browserPanel_ = &panel;

    auto& search = mrg::visual2d::CreateButton(
        panel,
        ScaleTopLeftBounds(layout::SearchField, panel.NodeSize().height),
        L"SEARCH SONGS",
        "SearchField");
    searchField_ = &search;
    searchFieldId_ = search.Id();
    ApplyButtonStyle(search, PureWhite, PaleBlue, SoftTextBlue);
    SetCornerRadius(search, 12.0F);
    auto& searchText = RequireComponent<mrg::visual2d::TextVisualComponent>(search);
    searchText.SetFontSize(CanvasFontSize(11.0F));
    searchText.SetHorizontalAlignment(mrg::visual2d::TextAlignment::Leading);
    searchText.SetContentBounds(
        ScaleTopLeftBounds({14.0F, 0.0F, 340.0F, 54.0F}, search.NodeSize().height));
    searchCountLabel_ = &AddLabel(
        search, layout::SearchCount, L"0 SONGS", 9.0F,
        SoftTextBlue, "SearchCount",
        mrg::visual2d::TextAlignment::Trailing);

    auto& sort = mrg::visual2d::CreateComboBox(
        panel,
        ScaleTopLeftBounds(layout::SortSelector, panel.NodeSize().height),
        {L"난이도순", L"곡 이름순", L"아티스트 이름순"},
        "SortSelector");
    sortSelector_ = &sort;
    sortSelectorId_ = sort.Id();
    ApplySpriteStyle(sort, PaleBlue, PaleBlue);
    SetCornerRadius(sort, 10.0F);
    auto& sortBehavior =
        RequireComponent<mrg::visual2d::ComboBoxBehaviorComponent>(sort);
    sortBehavior.SetItemHeight(38.0F * DesignToCanvasScale);
    sortBehavior.SetMaxVisibleItems(3);
    sortBehavior.SetFontSize(CanvasFontSize(12.0F));
    sortBehavior.SetTextColor(DeepBlue);
    sortBehavior.SetSelectedTextColor(PureWhite);
    sortBehavior.SetPopupBackgroundColor(PureWhite);

    auto& viewport = AddBorderedPanel(
        panel, layout::SongViewport, ViewportBlue, SubtleBorder,
        1.5F, 14.0F, "SongViewport");
    songViewport_ = &viewport;
    viewport.SetClipRect({
        0.0F, 0.0F,
        viewport.NodeSize().width,
        viewport.NodeSize().height});
    songContent_ = &viewport.CreateChild("SongContent");
    songContent_->SetSize(viewport.NodeSize());
    songContent_->SetPosition({0.0F, 0.0F});
    songContent_->SetZIndex(1);
    scrollbarTrack_ = &AddPanel(
        viewport, layout::ScrollbarTrack,
        {0.835F, 0.910F, 0.957F, 1.0F},
        "ScrollbarTrack", 3.0F);
    scrollbarTrack_->SetZIndex(2);
    scrollbarHandle_ = &AddPanel(
        viewport, layout::ScrollbarTrack,
        AccentBlue, "ScrollbarHandle", 3.0F);
    scrollbarHandle_->SetZIndex(3);

    browserHint_ = &AddLabel(
        panel, layout::BrowserHint,
        L"← / →  MOVE SONG     ↑ / ↓  DIFFICULTY     ENTER  GO",
        9.0F, SoftTextBlue, "BrowserHint",
        mrg::visual2d::TextAlignment::Center);
}

void LobbyScene::CreateFooter()
{
    auto& option = AddBorderedPanel(
        *board_, layout::OptionButton, PureWhite, BorderBlue,
        3.0F, 12.0F, "OptionSelect");
    optionButton_ = &option;
    optionLabel_ = &AddLabel(
        option, {0.0F, 0.0F, 696.0F, 60.0F},
        L"OPTION SELECT", 14.0F, MutedBlue, "OptionLabel",
        mrg::visual2d::TextAlignment::Center);
    optionButtonId_ = option.Id();

    struct EdgeButtonNodes
    {
        mrg::visual2d::NodeId id{};
        mrg::visual2d::Visual2DNode* button{};
        mrg::visual2d::Visual2DNode* fill{};
        mrg::visual2d::Visual2DNode* label{};
    };
    auto createEdgeButton = [this](
        const mrg::visual2d::Rect bounds,
        const mrg::visual2d::Rect labelBounds,
        const std::wstring& text,
        const std::string& name) -> EdgeButtonNodes
    {
        auto& button = mrg::visual2d::CreateButton(
            *board_,
            ScaleTopLeftBounds(bounds, board_->NodeSize().height),
            L"",
            name);
        ApplyButtonStyle(button, PureWhite, PureWhite, PureWhite);
        SetCornerRadius(button, 28.0F);
        auto& fill = AddPanel(
            button,
            {4.0F, 4.0F, bounds.width - 8.0F, bounds.height - 8.0F},
            AccentBlue,
            name + ".Fill",
            24.0F);
        auto& label = AddLabel(
            button, labelBounds, text, 27.0F, PureWhite,
            name + ".Label", mrg::visual2d::TextAlignment::Center);
        return {button.Id(), &button, &fill, &label};
    };
    const EdgeButtonNodes back = createEdgeButton(
        layout::BackButton, layout::BackLabel, L"BACK", "Back");
    backButtonId_ = back.id;
    backButton_ = back.button;
    backFill_ = back.fill;
    backLabel_ = back.label;
    const EdgeButtonNodes go = createEdgeButton(
        layout::GoButton, layout::GoLabel, L"GO", "Go");
    goButtonId_ = go.id;
    goButton_ = go.button;
    goFill_ = go.fill;
    goLabel_ = go.label;
}

void LobbyScene::UpdateResponsiveLayout()
{
    if (canvas_ == nullptr || board_ == nullptr || background_ == nullptr ||
        categoryBar_ == nullptr || recordPanel_ == nullptr ||
        informationPanel_ == nullptr || browserPanel_ == nullptr ||
        optionButton_ == nullptr || backButton_ == nullptr ||
        goButton_ == nullptr)
    {
        return;
    }

    const mrg::visual2d::Size canvasSize = canvas_->LogicalSize();
    const ResponsiveSongSelectLayout responsive =
        CalculateResponsiveLayout(canvasSize.width);
    board_->SetSize(canvasSize);
    board_->SetPosition({0.0F, 0.0F});
    background_->SetBounds(responsive.background);

    ResizeBorderedPanel(*categoryBar_, responsive.category, 3.0F);
    ResizeBorderedPanel(*recordPanel_, responsive.record, 3.0F);
    ResizeBorderedPanel(*informationPanel_, responsive.information, 3.0F);
    ResizeBorderedPanel(*browserPanel_, responsive.browser, 3.0F);
    categoryBar_->SetClipRect({
        0.0F, 0.0F,
        categoryBar_->NodeSize().width,
        categoryBar_->NodeSize().height});
    recordPanel_->SetClipRect({
        0.0F, 0.0F,
        recordPanel_->NodeSize().width,
        recordPanel_->NodeSize().height});
    informationPanel_->SetClipRect({
        0.0F, 0.0F,
        informationPanel_->NodeSize().width,
        informationPanel_->NodeSize().height});
    browserPanel_->SetClipRect({
        0.0F, 0.0F,
        browserPanel_->NodeSize().width,
        browserPanel_->NodeSize().height});

    UpdateRecordPanelLayout();
    UpdateSongInformationLayout();
    UpdateSongBrowserLayout();

    ResizeBorderedPanel(*optionButton_, responsive.option, 3.0F);
    backButton_->SetBounds(responsive.back);
    goButton_->SetBounds(responsive.go);
    UpdateFooterLayout();
}

void LobbyScene::UpdateRecordPanelLayout()
{
    if (recordPanel_ == nullptr || recordSelector_ == nullptr ||
        emptyRecordMessage_ == nullptr)
    {
        return;
    }
    const float panelWidth =
        recordPanel_->NodeSize().width / DesignToCanvasScale;
    const float innerWidth = std::max(panelWidth - 44.0F, 1.0F);
    recordSelector_->SetBounds(ScaleTopLeftBounds(
        {22.0F, 22.0F, innerWidth, layout::RecordSelector.height},
        recordPanel_->NodeSize().height));
    emptyRecordMessage_->SetBounds(ScaleTopLeftBounds(
        {22.0F, 400.0F, innerWidth, layout::EmptyRecordMessage.height},
        recordPanel_->NodeSize().height));
}

void LobbyScene::UpdateSongInformationLayout()
{
    if (informationPanel_ == nullptr || preview_ == nullptr ||
        previewTitle_ == nullptr || previewEmpty_ == nullptr ||
        selectedSongLabel_ == nullptr || selectedArtistLabel_ == nullptr ||
        difficultyInformation_ == nullptr || difficultyHeading_ == nullptr ||
        creatorHeading_ == nullptr || selectedPatternLabel_ == nullptr ||
        selectedCreatorLabel_ == nullptr || informationDivider_ == nullptr ||
        selectedDetailsLabel_ == nullptr)
    {
        return;
    }

    const float panelWidth =
        informationPanel_->NodeSize().width / DesignToCanvasScale;
    const bool showPreview = panelWidth >= 500.0F;
    preview_->SetVisible(showPreview);

    float detailsX = 24.0F;
    if (showPreview)
    {
        const float previewWidth = std::clamp(
            layout::Preview.width * panelWidth /
                layout::InformationPanel.width,
            160.0F,
            layout::Preview.width);
        preview_->SetBounds(ScaleTopLeftBounds(
            {24.0F, 24.0F, previewWidth, previewWidth},
            informationPanel_->NodeSize().height));
        previewTitle_->SetBounds(ScaleTopLeftBounds(
            {20.0F,
             layout::PreviewTitle.y * previewWidth / layout::Preview.width,
             std::max(previewWidth - 40.0F, 1.0F),
             layout::PreviewTitle.height},
            preview_->NodeSize().height));
        previewEmpty_->SetBounds(ScaleTopLeftBounds(
            {20.0F,
             layout::PreviewEmpty.y * previewWidth / layout::Preview.width,
             std::max(previewWidth - 40.0F, 1.0F),
             layout::PreviewEmpty.height},
            preview_->NodeSize().height));
        detailsX += previewWidth + 20.0F;
    }

    const float detailsWidth = std::max(panelWidth - detailsX - 24.0F, 1.0F);
    selectedSongLabel_->SetBounds(ScaleTopLeftBounds(
        {detailsX, layout::SelectedSong.y,
         detailsWidth, layout::SelectedSong.height},
        informationPanel_->NodeSize().height));
    selectedArtistLabel_->SetBounds(ScaleTopLeftBounds(
        {detailsX, layout::SelectedArtist.y,
         detailsWidth, layout::SelectedArtist.height},
        informationPanel_->NodeSize().height));
    ResizeBorderedPanel(
        *difficultyInformation_,
        ScaleTopLeftBounds(
            {detailsX, layout::DifficultyInformation.y,
             detailsWidth, layout::DifficultyInformation.height},
            informationPanel_->NodeSize().height),
        1.0F);

    const float headingGap = std::clamp(
        (detailsWidth - 32.0F) * 0.03F, 2.0F, 8.71F);
    const float headingWidth = std::max(
        (detailsWidth - 32.0F - headingGap) * 0.5F,
        1.0F);
    const float creatorX = 16.0F + headingWidth + headingGap;
    difficultyHeading_->SetBounds(ScaleTopLeftBounds(
        {16.0F, layout::DifficultyHeading.y,
         headingWidth, layout::DifficultyHeading.height},
        difficultyInformation_->NodeSize().height));
    creatorHeading_->SetBounds(ScaleTopLeftBounds(
        {creatorX, layout::CreatorHeading.y,
         headingWidth, layout::CreatorHeading.height},
        difficultyInformation_->NodeSize().height));
    selectedPatternLabel_->SetBounds(ScaleTopLeftBounds(
        {16.0F, layout::SelectedDifficulty.y,
         headingWidth, layout::SelectedDifficulty.height},
        difficultyInformation_->NodeSize().height));
    selectedCreatorLabel_->SetBounds(ScaleTopLeftBounds(
        {creatorX, layout::SelectedCreator.y,
         headingWidth, layout::SelectedCreator.height},
        difficultyInformation_->NodeSize().height));
    const float informationInnerWidth = std::max(detailsWidth - 32.0F, 1.0F);
    informationDivider_->SetBounds(ScaleTopLeftBounds(
        {16.0F, layout::InformationDivider.y,
         informationInnerWidth, layout::InformationDivider.height},
        difficultyInformation_->NodeSize().height));
    selectedDetailsLabel_->SetBounds(ScaleTopLeftBounds(
        {16.0F, layout::SelectedDetails.y,
         informationInnerWidth, layout::SelectedDetails.height},
        difficultyInformation_->NodeSize().height));
}

void LobbyScene::UpdateSongBrowserLayout()
{
    if (browserPanel_ == nullptr || searchField_ == nullptr ||
        searchCountLabel_ == nullptr || sortSelector_ == nullptr ||
        songViewport_ == nullptr || songContent_ == nullptr ||
        scrollbarTrack_ == nullptr || scrollbarHandle_ == nullptr ||
        browserHint_ == nullptr)
    {
        return;
    }

    const float panelWidth =
        browserPanel_->NodeSize().width / DesignToCanvasScale;
    const float innerWidth = std::max(panelWidth - 28.0F, 1.0F);
    searchField_->SetBounds(ScaleTopLeftBounds(
        {14.0F, layout::SearchField.y,
         innerWidth, layout::SearchField.height},
        browserPanel_->NodeSize().height));
    auto& searchText = RequireComponent<
        mrg::visual2d::TextVisualComponent>(*searchField_);
    const float countWidth = std::min(96.0F, innerWidth);
    const float searchTextWidth = std::max(innerWidth - countWidth - 18.0F, 1.0F);
    searchText.SetContentBounds(ScaleTopLeftBounds(
        {14.0F, 0.0F, searchTextWidth, layout::SearchField.height},
        searchField_->NodeSize().height));
    searchCountLabel_->SetBounds(ScaleTopLeftBounds(
        {std::max(innerWidth - countWidth - 17.0F, 0.0F),
         0.0F, countWidth, layout::SearchCount.height},
        searchField_->NodeSize().height));

    sortSelector_->SetBounds(ScaleTopLeftBounds(
        {14.0F, layout::SortSelector.y,
         innerWidth, layout::SortSelector.height},
        browserPanel_->NodeSize().height));
    ResizeBorderedPanel(
        *songViewport_,
        ScaleTopLeftBounds(
            {14.0F, layout::SongViewport.y,
             innerWidth, layout::SongViewport.height},
            browserPanel_->NodeSize().height),
        1.5F);
    songViewport_->SetClipRect({
        0.0F, 0.0F,
        songViewport_->NodeSize().width,
        songViewport_->NodeSize().height});
    songContent_->SetSize(songViewport_->NodeSize());
    songContentWidth_ = std::max(innerWidth - 24.0F, 1.0F);

    const float trackX = std::max(innerWidth - 9.0F, 0.0F);
    scrollbarTrack_->SetBounds(ScaleTopLeftBounds(
        {trackX, layout::ScrollbarTrack.y,
         layout::ScrollbarTrack.width, layout::ScrollbarTrack.height},
        songViewport_->NodeSize().height));
    browserHint_->SetBounds(ScaleTopLeftBounds(
        {18.0F, layout::BrowserHint.y,
         std::max(panelWidth - 36.0F, 1.0F),
         layout::BrowserHint.height},
        browserPanel_->NodeSize().height));
    UpdateScrollbar();
}

void LobbyScene::UpdateFooterLayout()
{
    if (optionButton_ == nullptr || optionLabel_ == nullptr ||
        backButton_ == nullptr || backFill_ == nullptr ||
        backLabel_ == nullptr || goButton_ == nullptr ||
        goFill_ == nullptr || goLabel_ == nullptr)
    {
        return;
    }
    optionLabel_->SetBounds({
        0.0F, 0.0F,
        optionButton_->NodeSize().width,
        optionButton_->NodeSize().height});

    const auto resizeEdgeButton = [](
        mrg::visual2d::Visual2DNode& button,
        mrg::visual2d::Visual2DNode& fill,
        mrg::visual2d::Visual2DNode& label)
    {
        const mrg::visual2d::Size buttonSize = button.NodeSize();
        const float padding = std::min({
            4.0F * DesignToCanvasScale,
            buttonSize.width * 0.1F,
            buttonSize.height * 0.1F});
        fill.SetBounds({
            padding, padding,
            std::max(buttonSize.width - padding * 2.0F, 1.0F),
            std::max(buttonSize.height - padding * 2.0F, 1.0F)});
        label.SetBounds({0.0F, 0.0F, buttonSize.width, buttonSize.height});
    };
    resizeEdgeButton(*backButton_, *backFill_, *backLabel_);
    resizeEdgeButton(*goButton_, *goFill_, *goLabel_);
}

void LobbyScene::ResizeBorderedPanel(
    mrg::visual2d::Visual2DNode& panel,
    const mrg::visual2d::Rect bounds,
    const float penpotBorderWidth)
{
    panel.SetBounds(bounds);
    const float borderWidth = penpotBorderWidth * DesignToCanvasScale;
    for (const auto& child : panel.Children())
    {
        if (child->Name() == "Surface")
        {
            child->SetBounds({
                borderWidth,
                borderWidth,
                std::max(panel.NodeSize().width - borderWidth * 2.0F, 1.0F),
                std::max(panel.NodeSize().height - borderWidth * 2.0F, 1.0F)});
            break;
        }
    }
}

void LobbyScene::RebuildVisibleSongs()
{
    std::optional<std::size_t> previousCatalogIndex;
    if (focusedSongPosition_ < visibleSongIndices_.size())
    {
        previousCatalogIndex = visibleSongIndices_[focusedSongPosition_];
    }

    visibleSongIndices_.clear();
    for (std::size_t index = 0; index < catalog_.songs.size(); ++index)
    {
        if (MatchesSearch(catalog_.songs[index], searchText_))
        {
            visibleSongIndices_.push_back(index);
        }
    }

    const auto compareText = [this](
        const std::size_t leftIndex,
        const std::size_t rightIndex)
    {
        const auto& left = catalog_.songs[leftIndex];
        const auto& right = catalog_.songs[rightIndex];
        if (sortMode_ == SortMode::Artist)
        {
            return Lowercase(SongArtist(left)) < Lowercase(SongArtist(right));
        }
        return Lowercase(SongTitle(left)) < Lowercase(SongTitle(right));
    };
    // YMM/YMP currently has no chart LEVEL field. "Difficulty" therefore
    // preserves catalog order instead of inventing a score from note count or
    // judgement timing strictness.
    if (sortMode_ != SortMode::Difficulty)
    {
        std::stable_sort(
            visibleSongIndices_.begin(),
            visibleSongIndices_.end(),
            compareText);
    }

    focusedSongPosition_ = 0;
    if (previousCatalogIndex.has_value())
    {
        const auto found = std::ranges::find(
            visibleSongIndices_, *previousCatalogIndex);
        if (found != visibleSongIndices_.end())
        {
            focusedSongPosition_ = static_cast<std::size_t>(
                std::distance(visibleSongIndices_.begin(), found));
        }
    }
    selectedPatternIndex_ = 0;
    scrollOffset_ = 0.0F;
    RebuildSongCards();
    RefreshSelectionPresentation();
    RefreshSearchPresentation();
    EnsureFocusedCardVisible();
    SyncPreviewToFocusedSong();
}

void LobbyScene::RebuildSongCards()
{
    if (songContent_ == nullptr || canvas_ == nullptr)
    {
        return;
    }
    inputRouter_.Reset(*canvas_);
    std::vector<mrg::visual2d::NodeId> childIds;
    childIds.reserve(songContent_->Children().size());
    for (const auto& child : songContent_->Children())
    {
        childIds.push_back(child->Id());
    }
    for (const mrg::visual2d::NodeId id : childIds)
    {
        static_cast<void>(songContent_->RemoveChild(id));
    }
    songCards_.clear();
    difficultyButtonIds_.clear();

    if (visibleSongIndices_.empty())
    {
        AddLabel(
            *songContent_,
            {layout::EmptySongMessage.x,
             layout::EmptySongMessage.y,
             std::max(songContentWidth_ - 32.0F, 1.0F),
             layout::EmptySongMessage.height},
            L"NO SONGS AVAILABLE", 20.0F, DeepBlue, "NoSongs",
            mrg::visual2d::TextAlignment::Center);
        contentHeight_ = layout::ContentBottom;
        ApplyScrollOffset();
        return;
    }

    float top = layout::ContentTop;
    for (std::size_t position = 0;
         position < visibleSongIndices_.size(); ++position)
    {
        const std::size_t catalogIndex = visibleSongIndices_[position];
        const bool focused = position == focusedSongPosition_;
        const std::size_t difficultyCount =
            catalog_.songs[catalogIndex].patterns.size();
        const float height = focused
            ? layout::ExpandedBaseHeight +
                layout::DifficultyRowStep *
                    static_cast<float>(difficultyCount)
            : layout::NormalSongHeight;
        CreateSongCard(
            position, catalogIndex, top, height, focused);
        top += height + layout::SongGap;
    }
    contentHeight_ = top - layout::SongGap + layout::ContentTop;
    ApplyScrollOffset();
    inputRouter_.InvalidateHitTest();
}

void LobbyScene::CreateSongCard(
    const std::size_t visiblePosition,
    const std::size_t catalogIndex,
    const float top,
    const float height,
    const bool focused)
{
    const auto& song = catalog_.songs[catalogIndex];
    auto& button = mrg::visual2d::CreateButton(
        *songContent_,
        ScaleTopLeftBounds(
            {layout::ContentLeft, top, songContentWidth_, height},
            songContent_->NodeSize().height),
        L"",
        std::format("Song.{}", visiblePosition));
    ApplyButtonStyle(
        button,
        focused ? BorderBlue : PaleBorder,
        BorderBlue,
        DeepBlue);
    SetCornerRadius(button, 14.0F);
    AddPanel(
        button,
        {2.0F, 2.0F, std::max(songContentWidth_ - 4.0F, 1.0F),
         height - 4.0F},
        focused ? FocusBlue : RowBlue,
        "Surface",
        12.0F);

    const float titleWidth = std::max(songContentWidth_ - 28.0F, 1.0F);
    auto& title = AddLabel(
        button,
        {14.0F, focused ? 16.0F : 10.0F, titleWidth,
         focused ? 32.0F : 30.0F},
        SongTitle(song), focused ? 20.0F : 16.0F,
        DeepBlue, "Title");
    title.AddComponent<finger_drum::presentation::MarqueeTextComponent>(
        SongTitle(song), focused ? 31 : 37, 0.24);
    auto& artist = AddLabel(
        button,
        {14.0F, focused ? 50.0F : 40.0F, titleWidth, 20.0F},
        SongArtist(song), focused ? 12.0F : 11.0F,
        MutedBlue, "Artist");

    if (focused)
    {
        AddPanel(
            button, {14.0F, 80.0F, titleWidth, 1.0F},
            PaleBorder, "Divider");
        if (song.patterns.empty())
        {
            AddLabel(
                button, {14.0F, 92.0F, titleWidth, 24.0F},
                L"NO DIFFICULTIES", 12.0F, MutedBlue,
                "NoDifficulties",
                mrg::visual2d::TextAlignment::Center);
        }
        for (std::size_t index = 0; index < song.patterns.size(); ++index)
        {
            auto& difficulty = mrg::visual2d::CreateButton(
                button,
                ScaleTopLeftBounds(
                    {14.0F,
                     92.0F + layout::DifficultyRowStep *
                         static_cast<float>(index),
                     titleWidth,
                     layout::DifficultyRowHeight},
                    button.NodeSize().height),
                PatternName(song.patterns[index]),
                std::format("Difficulty.{}", index));
            const bool selected = index == selectedPatternIndex_;
            ApplyButtonStyle(
                difficulty,
                selected ? AccentBlue : PureWhite,
                selected ? AccentHover : PaleBlue,
                selected ? PureWhite : DeepBlue);
            SetCornerRadius(difficulty, 8.0F);
            auto& text = RequireComponent<
                mrg::visual2d::TextVisualComponent>(difficulty);
            text.SetFontSize(CanvasFontSize(12.0F));
            text.SetHorizontalAlignment(mrg::visual2d::TextAlignment::Leading);
            text.SetContentBounds(ScaleTopLeftBounds(
                {12.0F, 0.0F, titleWidth - 24.0F,
                 layout::DifficultyRowHeight},
                difficulty.NodeSize().height));
            difficultyButtonIds_.push_back({difficulty.Id(), index});
        }
    }
    songCards_.push_back({
        catalogIndex, top, height, &button, &title, &artist});
}

void LobbyScene::RefreshSelectionPresentation()
{
    if (selectedSongLabel_ == nullptr || selectedArtistLabel_ == nullptr ||
        selectedPatternLabel_ == nullptr || selectedCreatorLabel_ == nullptr ||
        selectedDetailsLabel_ == nullptr)
    {
        return;
    }

    auto& songMarquee = RequireComponent<
        finger_drum::presentation::MarqueeTextComponent>(*selectedSongLabel_);
    auto& artistMarquee = RequireComponent<
        finger_drum::presentation::MarqueeTextComponent>(*selectedArtistLabel_);
    if (visibleSongIndices_.empty())
    {
        songMarquee.SetText(L"NO SONG SELECTED");
        artistMarquee.SetText(L"");
        RequireComponent<mrg::visual2d::TextVisualComponent>(
            *selectedPatternLabel_).SetText(L"—");
        RequireComponent<mrg::visual2d::TextVisualComponent>(
            *selectedCreatorLabel_).SetText(L"—");
        RequireComponent<mrg::visual2d::TextVisualComponent>(
            *selectedDetailsLabel_).SetText(launchError_.empty()
                ? L"LEVEL —   BPM —   NOTES —   MODE —"
                : launchError_);
        return;
    }

    focusedSongPosition_ = std::min(
        focusedSongPosition_, visibleSongIndices_.size() - 1);
    const auto& song = catalog_.songs[
        visibleSongIndices_[focusedSongPosition_]];
    songMarquee.SetText(SongTitle(song));
    artistMarquee.SetText(SongArtist(song));
    if (song.patterns.empty())
    {
        RequireComponent<mrg::visual2d::TextVisualComponent>(
            *selectedPatternLabel_).SetText(L"NO DIFFICULTY");
        RequireComponent<mrg::visual2d::TextVisualComponent>(
            *selectedCreatorLabel_).SetText(L"—");
        RequireComponent<mrg::visual2d::TextVisualComponent>(
            *selectedDetailsLabel_).SetText(
                L"LEVEL —   BPM —   NOTES —   MODE —");
        return;
    }

    selectedPatternIndex_ = std::min(
        selectedPatternIndex_, song.patterns.size() - 1);
    const auto& selected = song.patterns[selectedPatternIndex_];
    RequireComponent<mrg::visual2d::TextVisualComponent>(
        *selectedPatternLabel_).SetText(PatternName(selected));
    RequireComponent<mrg::visual2d::TextVisualComponent>(
        *selectedCreatorLabel_).SetText(
            selected.pattern.makers.empty()
                ? L"—"
                : DecodeDisplayText(selected.pattern.makers.front()));
    std::wstring mode = selected.pattern.mode.empty()
        ? L"—"
        : DecodeDisplayText(selected.pattern.mode);
    std::ranges::transform(
        mode,
        mode.begin(),
        [](const wchar_t character)
        {
            return static_cast<wchar_t>(std::towupper(character));
        });
    if (!launchError_.empty())
    {
        RequireComponent<mrg::visual2d::TextVisualComponent>(
            *selectedDetailsLabel_).SetText(launchError_);
        return;
    }
    RequireComponent<mrg::visual2d::TextVisualComponent>(
        *selectedDetailsLabel_).SetText(std::format(
            L"LEVEL —   BPM {}   NOTES {}   MODE {}",
            BpmText(selected.pattern.baseBpm),
            selected.pattern.notes.size(),
            mode));
}

void LobbyScene::RefreshSearchPresentation()
{
    if (searchField_ != nullptr)
    {
        std::wstring display = searchText_.empty()
            ? L"SEARCH SONGS"
            : searchText_;
        if (searchFocused_)
        {
            display += L"_";
        }
        RequireComponent<mrg::visual2d::TextVisualComponent>(
            *searchField_).SetText(std::move(display));
    }
    if (searchCountLabel_ != nullptr)
    {
        RequireComponent<mrg::visual2d::TextVisualComponent>(
            *searchCountLabel_).SetText(std::format(
                L"{} SONGS", visibleSongIndices_.size()));
    }
}

void LobbyScene::EnsureFocusedCardVisible()
{
    if (focusedSongPosition_ >= songCards_.size())
    {
        scrollOffset_ = 0.0F;
        ApplyScrollOffset();
        return;
    }
    const SongCard& card = songCards_[focusedSongPosition_];
    const float visibleTop = layout::ContentTop + scrollOffset_;
    const float visibleBottom = layout::ContentBottom + scrollOffset_;
    if (card.top < visibleTop)
    {
        scrollOffset_ = card.top - layout::ContentTop;
    }
    else if (card.top + card.height > visibleBottom)
    {
        scrollOffset_ = card.top + card.height - layout::ContentBottom;
    }
    ApplyScrollOffset();
}

void LobbyScene::ApplyScrollOffset()
{
    const float maximum = std::max(
        contentHeight_ - layout::ContentBottom,
        0.0F);
    scrollOffset_ = std::clamp(scrollOffset_, 0.0F, maximum);
    if (songContent_ != nullptr)
    {
        songContent_->SetPosition({
            0.0F,
            scrollOffset_ * DesignToCanvasScale});
    }
    UpdateScrollbar();
    inputRouter_.InvalidateHitTest();
}

void LobbyScene::UpdateScrollbar()
{
    if (scrollbarHandle_ == nullptr || songViewport_ == nullptr)
    {
        return;
    }
    const float maximum = std::max(
        contentHeight_ - layout::ContentBottom,
        0.0F);
    const float handleHeight = maximum <= 0.0F
        ? layout::ScrollbarTrack.height
        : std::max(
            48.0F,
            layout::ScrollbarTrack.height *
                layout::ViewportVisibleHeight /
                std::max(contentHeight_ - layout::ContentTop,
                         layout::ViewportVisibleHeight));
    const float travel = layout::ScrollbarTrack.height - handleHeight;
    const float handleTop = layout::ScrollbarTrack.y +
        (maximum <= 0.0F ? 0.0F : travel * scrollOffset_ / maximum);
    const float trackX = std::max(
        songViewport_->NodeSize().width / DesignToCanvasScale - 9.0F,
        0.0F);
    scrollbarHandle_->SetBounds(ScaleTopLeftBounds(
        {trackX,
         handleTop,
         layout::ScrollbarTrack.width,
         handleHeight},
        songViewport_->NodeSize().height));
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
    pointer.wheelDelta = input.MouseWheelDelta();
    pointer.timestampTicks = LatestPointerTimestamp(input);
    inputRouter_.Process(*canvas_, pointer);

    if (canvasPointer.has_value() && songViewport_ != nullptr &&
        songViewport_->BoundsInCanvas().Contains(*canvasPointer) &&
        std::abs(input.MouseWheelDelta()) > 0.0F)
    {
        scrollOffset_ -= input.MouseWheelDelta() * layout::ScrollStep;
        ApplyScrollOffset();
    }
}

bool LobbyScene::ProcessActions(mrg::scene::SceneManager& scenes)
{
    for (const mrg::visual2d::Action& action : canvas_->TakeActions())
    {
        if (action.type == mrg::visual2d::ActionType::SelectionChanged &&
            action.source == sortSelectorId_)
        {
            sortMode_ = static_cast<SortMode>(std::min<std::size_t>(
                action.selectedIndex,
                static_cast<std::size_t>(SortMode::Artist)));
            RebuildVisibleSongs();
            return false;
        }
        if (action.type != mrg::visual2d::ActionType::Clicked)
        {
            continue;
        }
        if (action.source == backButtonId_)
        {
            return ReturnToLogo(scenes);
        }
        if (action.source == goButtonId_)
        {
            return StartSelectedPattern(scenes);
        }
        if (action.source == searchFieldId_)
        {
            searchFocused_ = true;
            RefreshSearchPresentation();
            return false;
        }
        for (std::size_t position = 0; position < songCards_.size(); ++position)
        {
            if (songCards_[position].button != nullptr &&
                action.source == songCards_[position].button->Id())
            {
                searchFocused_ = false;
                SelectVisibleSong(position);
                RefreshSearchPresentation();
                return false;
            }
        }
        for (const auto [id, patternIndex] : difficultyButtonIds_)
        {
            if (action.source == id)
            {
                searchFocused_ = false;
                SelectPattern(patternIndex);
                RefreshSearchPresentation();
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
        if (searchFocused_)
        {
            searchFocused_ = false;
            RefreshSearchPresentation();
            return false;
        }
        return ReturnToLogo(scenes);
    }
    if (input.IsKeyDown(VK_CONTROL) &&
        input.WasKeyPressed(static_cast<std::uint16_t>('F')))
    {
        searchFocused_ = true;
        RefreshSearchPresentation();
        return false;
    }
    if (ProcessSearchKeyboard(input))
    {
        return false;
    }
    if (visibleSongIndices_.empty())
    {
        return false;
    }

    if (input.WasKeyPressed(VK_LEFT))
    {
        MoveSongFocus(-1);
    }
    else if (input.WasKeyPressed(VK_RIGHT))
    {
        MoveSongFocus(1);
    }
    else if (input.WasKeyPressed(VK_UP))
    {
        MoveDifficultyFocus(-1);
    }
    else if (input.WasKeyPressed(VK_DOWN))
    {
        MoveDifficultyFocus(1);
    }

    if (input.WasKeyPressed(VK_RETURN) || input.WasKeyPressed(VK_SPACE))
    {
        return StartSelectedPattern(scenes);
    }
    return false;
}

bool LobbyScene::ProcessSearchKeyboard(
    const mrg::platform::InputState& input)
{
    if (!searchFocused_)
    {
        return false;
    }
    if (input.WasKeyPressed(VK_RETURN))
    {
        searchFocused_ = false;
        RefreshSearchPresentation();
        return true;
    }

    bool changed = false;
    if (input.WasKeyPressed(VK_BACK) && !searchText_.empty())
    {
        searchText_.pop_back();
        changed = true;
    }
    if (searchText_.size() < 48)
    {
        for (std::uint16_t key = 'A'; key <= 'Z'; ++key)
        {
            if (input.WasKeyPressed(key))
            {
                searchText_.push_back(static_cast<wchar_t>(key));
                changed = true;
            }
        }
        for (std::uint16_t key = '0'; key <= '9'; ++key)
        {
            if (input.WasKeyPressed(key))
            {
                searchText_.push_back(static_cast<wchar_t>(key));
                changed = true;
            }
        }
        if (input.WasKeyPressed(VK_SPACE))
        {
            searchText_.push_back(L' ');
            changed = true;
        }
    }
    if (changed)
    {
        RebuildVisibleSongs();
        searchFocused_ = true;
        RefreshSearchPresentation();
    }
    return changed;
}

void LobbyScene::MoveSongFocus(const int delta)
{
    if (visibleSongIndices_.empty())
    {
        return;
    }
    const std::ptrdiff_t target = std::clamp<std::ptrdiff_t>(
        static_cast<std::ptrdiff_t>(focusedSongPosition_) + delta,
        0,
        static_cast<std::ptrdiff_t>(visibleSongIndices_.size() - 1));
    SelectVisibleSong(static_cast<std::size_t>(target));
}

void LobbyScene::MoveDifficultyFocus(const int delta)
{
    if (visibleSongIndices_.empty())
    {
        return;
    }
    const auto& patterns = catalog_.songs[
        visibleSongIndices_[focusedSongPosition_]].patterns;
    if (delta < 0)
    {
        if (!patterns.empty() && selectedPatternIndex_ > 0)
        {
            SelectPattern(selectedPatternIndex_ - 1);
            return;
        }
        if (focusedSongPosition_ > 0)
        {
            SelectVisibleSong(focusedSongPosition_ - 1);
        }
        return;
    }
    if (!patterns.empty() && selectedPatternIndex_ + 1 < patterns.size())
    {
        SelectPattern(selectedPatternIndex_ + 1);
        return;
    }
    if (focusedSongPosition_ + 1 < visibleSongIndices_.size())
    {
        SelectVisibleSong(focusedSongPosition_ + 1);
    }
}

void LobbyScene::SelectVisibleSong(const std::size_t visiblePosition)
{
    if (visiblePosition >= visibleSongIndices_.size())
    {
        return;
    }
    focusedSongPosition_ = visiblePosition;
    selectedPatternIndex_ = 0;
    launchError_.clear();
    RebuildSongCards();
    RefreshSelectionPresentation();
    EnsureFocusedCardVisible();
    SyncPreviewToFocusedSong();
}

void LobbyScene::SyncPreviewToFocusedSong()
{
    if (!sceneActive_ || audioSystem_ == nullptr)
    {
        return;
    }

    if (visibleSongIndices_.empty() ||
        focusedSongPosition_ >= visibleSongIndices_.size())
    {
        if (currentPreviewSlot_.has_value())
        {
            previewSlots_[*currentPreviewSlot_].targetVolume = 0.0F;
            currentPreviewSlot_.reset();
        }
        return;
    }

    const std::size_t catalogIndex = visibleSongIndices_[focusedSongPosition_];
    if (currentPreviewSlot_.has_value())
    {
        const PreviewSlot& current = previewSlots_[*currentPreviewSlot_];
        if (current.catalogIndex == catalogIndex &&
            current.playbackId != mrg::audio::InvalidAudioPlaybackId)
        {
            return;
        }
    }

    StartSongPreview(catalogIndex);
}

void LobbyScene::StartSongPreview(const std::size_t catalogIndex)
{
    if (audioSystem_ == nullptr || catalogIndex >= catalog_.songs.size())
    {
        return;
    }

    if (currentPreviewSlot_.has_value())
    {
        previewSlots_[*currentPreviewSlot_].targetVolume = 0.0F;
    }

    const std::size_t nextSlotIndex = currentPreviewSlot_.has_value()
        ? 1U - *currentPreviewSlot_
        : (previewSlots_[0].playbackId == mrg::audio::InvalidAudioPlaybackId
               ? 0U
               : 1U);
    PreviewSlot& nextSlot = previewSlots_[nextSlotIndex];
    StopPreviewSlot(nextSlot);

    std::string errorMessage;
    std::unique_ptr<mrg::audio::AudioClip> loadedClip =
        audioSystem_->LoadSound(
            catalog_.songs[catalogIndex].audioPath,
            mrg::audio::AudioLoadMode::Stream,
            errorMessage);
    if (loadedClip == nullptr)
    {
        return;
    }

    auto clip = std::shared_ptr<mrg::audio::AudioClip>(std::move(loadedClip));
    mrg::audio::AudioPlaybackSettings settings;
    settings.volume = 0.0F;
    const mrg::audio::AudioPlaybackId playbackId = audioPlayback_.Play(
        clip, settings, nullptr, errorMessage);
    if (playbackId == mrg::audio::InvalidAudioPlaybackId)
    {
        return;
    }

    nextSlot.clip = std::move(clip);
    nextSlot.playbackId = playbackId;
    nextSlot.catalogIndex = catalogIndex;
    nextSlot.volume = 0.0F;
    nextSlot.targetVolume = 1.0F;
    currentPreviewSlot_ = nextSlotIndex;
}

void LobbyScene::UpdatePreviewAudio(const double deltaSeconds)
{
    const float fadeStep = static_cast<float>(
        std::max(deltaSeconds, 0.0) / PreviewFadeSeconds);

    for (std::size_t index = 0; index < previewSlots_.size(); ++index)
    {
        PreviewSlot& slot = previewSlots_[index];
        if (slot.playbackId == mrg::audio::InvalidAudioPlaybackId)
        {
            continue;
        }

        mrg::audio::AudioVoice* const voice =
            audioPlayback_.FindVoice(slot.playbackId);
        if (voice == nullptr)
        {
            slot = {};
            if (currentPreviewSlot_ == index)
            {
                currentPreviewSlot_.reset();
            }
            continue;
        }

        if (slot.volume < slot.targetVolume)
        {
            slot.volume = std::min(slot.volume + fadeStep, slot.targetVolume);
        }
        else if (slot.volume > slot.targetVolume)
        {
            slot.volume = std::max(slot.volume - fadeStep, slot.targetVolume);
        }

        std::string errorMessage;
        if (!voice->SetVolume(slot.volume, errorMessage))
        {
            StopPreviewSlot(slot);
            if (currentPreviewSlot_ == index)
            {
                currentPreviewSlot_.reset();
            }
            continue;
        }

        if (slot.targetVolume <= 0.0F && slot.volume <= 0.0F)
        {
            StopPreviewSlot(slot);
            if (currentPreviewSlot_ == index)
            {
                currentPreviewSlot_.reset();
            }
        }
    }
}

void LobbyScene::StopPreviewAudio() noexcept
{
    for (PreviewSlot& slot : previewSlots_)
    {
        StopPreviewSlot(slot);
    }
    currentPreviewSlot_.reset();
}

void LobbyScene::StopPreviewSlot(PreviewSlot& slot) noexcept
{
    if (slot.playbackId != mrg::audio::InvalidAudioPlaybackId)
    {
        try
        {
            std::string ignoredError;
            static_cast<void>(
                audioPlayback_.Stop(slot.playbackId, ignoredError));
        }
        catch (...)
        {
        }
    }
    slot = {};
}

void LobbyScene::SelectPattern(const std::size_t index)
{
    if (focusedSongPosition_ >= visibleSongIndices_.size())
    {
        return;
    }
    const auto& patterns = catalog_.songs[
        visibleSongIndices_[focusedSongPosition_]].patterns;
    if (index >= patterns.size())
    {
        return;
    }
    selectedPatternIndex_ = index;
    launchError_.clear();
    for (const auto [id, patternIndex] : difficultyButtonIds_)
    {
        if (mrg::visual2d::Visual2DNode* button = canvas_->FindNode(id))
        {
            const bool selected = patternIndex == selectedPatternIndex_;
            ApplyButtonStyle(
                *button,
                selected ? AccentBlue : PureWhite,
                selected ? AccentHover : PaleBlue,
                selected ? PureWhite : DeepBlue);
        }
    }
    RefreshSelectionPresentation();
}

bool LobbyScene::StartSelectedPattern(mrg::scene::SceneManager& scenes)
{
    if (focusedSongPosition_ >= visibleSongIndices_.size())
    {
        return false;
    }
    const auto& song = catalog_.songs[
        visibleSongIndices_[focusedSongPosition_]];
    if (song.patterns.empty() || selectedPatternIndex_ >= song.patterns.size())
    {
        return false;
    }
    const auto& pattern = song.patterns[selectedPatternIndex_];
    if (!pattern.pattern.mode.empty() && pattern.pattern.mode != "Taiko")
    {
        launchError_ = L"UNSUPPORTED MODE: " +
            DecodeDisplayText(pattern.pattern.mode);
        RefreshSelectionPresentation();
        return false;
    }

    try
    {
        finger_drum::mode::ModeLoadResult validation =
            finger_drum::mode::TaikoMode{}.LoadSession(
                pattern.patternPath,
                pattern.effectPath);
        if (!validation.Succeeded())
        {
            launchError_ = L"PATTERN ERROR: " + DecodeDisplayText(
                validation.diagnostics.empty()
                    ? std::string_view{"Unknown chart error."}
                    : std::string_view{validation.diagnostics.front().message});
            RefreshSelectionPresentation();
            return false;
        }
    }
    catch (const std::exception& exception)
    {
        launchError_ = L"PATTERN ERROR: " + DecodeDisplayText(exception.what());
        RefreshSelectionPresentation();
        return false;
    }

    launchError_.clear();
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
    std::string name,
    const float penpotCornerRadius)
{
    auto& panel = mrg::visual2d::CreatePanel(
        parent,
        ScaleTopLeftBounds(penpotBounds, parent.NodeSize().height),
        std::move(name));
    ApplyFlatStyle(panel, color);
    SetCornerRadius(panel, penpotCornerRadius);
    return panel;
}

mrg::visual2d::Visual2DNode& LobbyScene::AddBorderedPanel(
    mrg::visual2d::Visual2DNode& parent,
    const mrg::visual2d::Rect penpotBounds,
    const mrg::visual2d::Color fill,
    const mrg::visual2d::Color border,
    const float penpotBorderWidth,
    const float penpotCornerRadius,
    std::string name)
{
    auto& outer = AddPanel(
        parent, penpotBounds, border, std::move(name), penpotCornerRadius);
    AddPanel(
        outer,
        {penpotBorderWidth,
         penpotBorderWidth,
         std::max(penpotBounds.width - penpotBorderWidth * 2.0F, 0.0F),
         std::max(penpotBounds.height - penpotBorderWidth * 2.0F, 0.0F)},
        fill,
        "Surface",
        std::max(penpotCornerRadius - penpotBorderWidth, 0.0F));
    return outer;
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
        ScaleTopLeftBounds(penpotBounds, parent.NodeSize().height),
        std::move(text),
        std::move(name));
    auto& textComponent =
        RequireComponent<mrg::visual2d::TextVisualComponent>(label);
    textComponent.SetFontSize(CanvasFontSize(penpotFontSize));
    textComponent.SetTextColor(color);
    textComponent.SetHorizontalAlignment(alignment);
    return label;
}
