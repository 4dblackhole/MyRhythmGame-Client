#include "LobbyScene.h"

#include "GameFlow/FingerDrumSceneIds.h"
#include "Presentation/MarqueeTextComponent.h"

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
        constexpr float ContentWidth = 454.29F;
        constexpr float ViewportVisibleHeight = ContentBottom - ContentTop;
        constexpr float SongGap = 12.0F;
        constexpr float NormalSongHeight = 72.0F;
        constexpr float ExpandedBaseHeight = 144.0F;
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
    std::shared_ptr<finger_drum::GameplayLaunchRequest> launchRequest)
    : launchRequest_(std::move(launchRequest)),
      screenVisuals_(screenVisuals)
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
    canvasId_ = screenVisuals_.CreateCanvas({
        layout::CanvasSize,
        mrg::visual2d::CanvasScaleMode::FixedHeight});
    canvas_ = screenVisuals_.FindCanvas(canvasId_);
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

    AddPanel(*board_, layout::DesignBoard, CanvasBlue, "Background");
    CreateCategoryBar();
    CreateRecordPanel();
    CreateSongInformationPanel();
    CreateSongBrowser();
    CreateFooter();
    RebuildVisibleSongs();
}

void LobbyScene::BeginScene()
{
    static_cast<void>(screenVisuals_.SetCanvasVisible(canvasId_, true));
}

void LobbyScene::EndScene() noexcept
{
    static_cast<void>(screenVisuals_.SetCanvasVisible(canvasId_, false));
}

void LobbyScene::Update(
    const mrg::UpdateContext& context,
    mrg::scene::SceneManager& scenes)
{
    if (canvas_ == nullptr)
    {
        return;
    }
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
        inputRouter_.InvalidateHitTest();
    }
}

void LobbyScene::Shutdown() noexcept
{
    if (canvas_ != nullptr)
    {
        inputRouter_.Reset(*canvas_);
    }
    difficultyButtonIds_.clear();
    songCards_.clear();
    visibleSongIndices_.clear();
    selectedDetailsLabel_ = nullptr;
    selectedCreatorLabel_ = nullptr;
    selectedPatternLabel_ = nullptr;
    selectedArtistLabel_ = nullptr;
    selectedSongLabel_ = nullptr;
    searchCountLabel_ = nullptr;
    searchField_ = nullptr;
    scrollbarHandle_ = nullptr;
    songContent_ = nullptr;
    songViewport_ = nullptr;
    board_ = nullptr;
    static_cast<void>(screenVisuals_.RemoveCanvas(canvasId_));
    canvasId_ = mrg::visual2d::InvalidScreenCanvasId;
    canvas_ = nullptr;
    catalog_ = {};
}

void LobbyScene::CreateCategoryBar()
{
    auto& bar = AddBorderedPanel(
        *board_, layout::CategoryBar, PanelWhite, BorderBlue,
        3.0F, 20.0F, "Categories");
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
    auto& selector = mrg::visual2d::CreateComboBox(
        panel,
        ScaleTopLeftBounds(layout::RecordSelector, panel.NodeSize().height),
        {L"PERSONAL RECORD"},
        "RecordSelector");
    recordSelectorId_ = selector.Id();
    ApplySpriteStyle(selector, PaleBlue, PaleBlue);
    SetCornerRadius(selector, 10.0F);
    auto& recordBehavior = RequireComponent<
        mrg::visual2d::ComboBoxBehaviorComponent>(selector);
    recordBehavior.SetFontSize(10.0F * DesignToCanvasScale);
    recordBehavior.SetTextColor(DeepBlue);
    recordBehavior.SetSelectedTextColor(PureWhite);
    recordBehavior.SetPopupBackgroundColor(PureWhite);
    AddLabel(
        panel, layout::EmptyRecordMessage, L"NO RECORDS",
        28.0F, DeepBlue, "NoRecords",
        mrg::visual2d::TextAlignment::Center);
}

void LobbyScene::CreateSongInformationPanel()
{
    auto& panel = AddBorderedPanel(
        *board_, layout::InformationPanel, PanelWhite, BorderBlue,
        3.0F, 24.0F, "SongInformation");
    auto& preview = AddPanel(
        panel, layout::Preview,
        {0.765F, 0.906F, 0.980F, 1.0F},
        "BackgroundPreview", 12.0F);
    AddLabel(
        preview, layout::PreviewTitle, L"BACKGROUND PREVIEW",
        13.0F, MutedBlue, "PreviewTitle",
        mrg::visual2d::TextAlignment::Center);
    AddLabel(
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
    AddLabel(
        information, layout::DifficultyHeading, L"DIFFICULTY",
        9.0F, SoftTextBlue, "DifficultyHeading");
    AddLabel(
        information, layout::CreatorHeading, L"CREATOR",
        9.0F, SoftTextBlue, "CreatorHeading");
    selectedPatternLabel_ = &AddLabel(
        information, layout::SelectedDifficulty, L"—",
        18.0F, AccentBlue, "SelectedDifficulty");
    selectedCreatorLabel_ = &AddLabel(
        information, layout::SelectedCreator, L"—",
        18.0F, DeepBlue, "SelectedCreator");
    AddPanel(
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
    searchText.SetFontSize(11.0F * DesignToCanvasScale);
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
    sortSelectorId_ = sort.Id();
    ApplySpriteStyle(sort, PaleBlue, PaleBlue);
    SetCornerRadius(sort, 10.0F);
    auto& sortBehavior =
        RequireComponent<mrg::visual2d::ComboBoxBehaviorComponent>(sort);
    sortBehavior.SetItemHeight(38.0F * DesignToCanvasScale);
    sortBehavior.SetMaxVisibleItems(3);
    sortBehavior.SetFontSize(12.0F * DesignToCanvasScale);
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
    AddPanel(
        viewport, layout::ScrollbarTrack,
        {0.835F, 0.910F, 0.957F, 1.0F},
        "ScrollbarTrack", 3.0F).SetZIndex(2);
    scrollbarHandle_ = &AddPanel(
        viewport, layout::ScrollbarTrack,
        AccentBlue, "ScrollbarHandle", 3.0F);
    scrollbarHandle_->SetZIndex(3);

    AddLabel(
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
    AddLabel(
        option, {0.0F, 0.0F, 696.0F, 60.0F},
        L"OPTION SELECT", 14.0F, MutedBlue, "OptionLabel",
        mrg::visual2d::TextAlignment::Center);
    optionButtonId_ = option.Id();

    auto createEdgeButton = [this](
        const mrg::visual2d::Rect bounds,
        const mrg::visual2d::Rect labelBounds,
        const std::wstring& text,
        const std::string& name) -> mrg::visual2d::NodeId
    {
        auto& button = mrg::visual2d::CreateButton(
            *board_,
            ScaleTopLeftBounds(bounds, board_->NodeSize().height),
            L"",
            name);
        ApplyButtonStyle(button, PureWhite, PureWhite, PureWhite);
        SetCornerRadius(button, 28.0F);
        AddPanel(
            button,
            {4.0F, 4.0F, bounds.width - 8.0F, bounds.height - 8.0F},
            AccentBlue,
            name + ".Fill",
            24.0F);
        AddLabel(
            button, labelBounds, text, 27.0F, PureWhite,
            name + ".Label", mrg::visual2d::TextAlignment::Center);
        return button.Id();
    };
    backButtonId_ = createEdgeButton(
        layout::BackButton, layout::BackLabel, L"BACK", "Back");
    goButtonId_ = createEdgeButton(
        layout::GoButton, layout::GoLabel, L"GO", "Go");
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
            *songContent_, layout::EmptySongMessage,
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
            {layout::ContentLeft, top, layout::ContentWidth, height},
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
        {2.0F, 2.0F, layout::ContentWidth - 4.0F, height - 4.0F},
        focused ? FocusBlue : RowBlue,
        "Surface",
        12.0F);

    const float titleWidth = layout::ContentWidth - 28.0F;
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
        AddLabel(
            button, {14.0F, 92.0F, titleWidth, 18.0F},
            L"CHOOSE DIFFICULTY", 9.0F, SoftTextBlue,
            "DifficultyHeading");
        if (song.patterns.empty())
        {
            AddLabel(
                button, {14.0F, 116.0F, titleWidth, 24.0F},
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
                     120.0F + layout::DifficultyRowStep *
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
            text.SetFontSize(12.0F * DesignToCanvasScale);
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
            *selectedDetailsLabel_).SetText(
                L"LEVEL —   BPM —   NOTES —   MODE —");
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
    scrollbarHandle_->SetBounds(ScaleTopLeftBounds(
        {layout::ScrollbarTrack.x,
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
    RebuildSongCards();
    RefreshSelectionPresentation();
    EnsureFocusedCardVisible();
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
    textComponent.SetFontSize(penpotFontSize * DesignToCanvasScale);
    textComponent.SetTextColor(color);
    textComponent.SetHorizontalAlignment(alignment);
    return label;
}
