#pragma once
#include "App/AssetPaths.h"
#include "Catalog/SongCatalog.h"
#include "PhantomCore.h"
#include "Presentation/MarqueeTextComponent.h"
#include "Presentation/SkinVisualComponent.h"
#include "Presentation/UiColors.h"
#include <algorithm>
#include <cmath>
#include <cwctype>
#include <format>
#include <ranges>
#include <stdexcept>

namespace song_select
{
    constexpr float DesignToCanvasScale = 2.0F / 3.0F;
    constexpr double PreviewFadeSeconds = 0.2;

    [[nodiscard]] constexpr float ReadableFontScale(const float penpotFontSize) noexcept
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

    [[nodiscard]] constexpr float CanvasFontSize(const float penpotFontSize) noexcept
    {
        return penpotFontSize * DesignToCanvasScale * ReadableFontScale(penpotFontSize);
    }

    constexpr auto CanvasBlue = ui_colors::sky::Background, PanelWhite = ui_colors::sky::Surface;
    constexpr auto PureWhite = ui_colors::White, AccentBlue = ui_colors::sky::Accent;
    constexpr auto AccentHover = ui_colors::sky::AccentHover, BorderBlue = ui_colors::sky::Accent;
    constexpr auto SubtleBorder = ui_colors::sky::Border, PaleBorder = ui_colors::sky::PaleBorder;
    constexpr auto PaleBlue = ui_colors::sky::Pale, ViewportBlue = ui_colors::sky::Viewport;
    constexpr auto FocusBlue = ui_colors::sky::Focus, RowBlue = ui_colors::sky::Row;
    constexpr auto DeepBlue = ui_colors::sky::Ink, MutedBlue = ui_colors::sky::Muted;
    constexpr auto SoftTextBlue = ui_colors::sky::SoftText, DisabledBlue = ui_colors::sky::Disabled;

    namespace layout
    {
        constexpr Size CanvasSize{1280.0F, 720.0F};
        constexpr Rect DesignBoard{0.0F, 0.0F, 1920.0F, 1080.0F};

        constexpr Rect CategoryBar{54.0F, 20.0F, 1285.71F, 56.0F};
        constexpr Rect AllCategory{16.0F, 9.0F, 86.0F, 38.0F};

        constexpr Rect RecordPanel{54.0F, 94.0F, 506.29F, 852.0F};
        constexpr Rect RecordSelector{22.0F, 22.0F, 462.29F, 44.0F};
        constexpr Rect EmptyRecordMessage{22.0F, 400.0F, 462.29F, 54.0F};

        constexpr Rect InformationPanel{580.29F, 94.0F, 759.43F, 852.0F};
        constexpr Rect Preview{24.0F, 24.0F, 360.0F, 360.0F};
        constexpr Rect PreviewTitle{54.0F, 190.0F, 300.0F, 28.0F};
        constexpr Rect PreviewEmpty{54.0F, 224.0F, 300.0F, 20.0F};
        constexpr Rect SelectedSong{404.0F, 38.0F, 331.43F, 38.0F};
        constexpr Rect SelectedArtist{404.0F, 80.0F, 331.43F, 24.0F};
        constexpr Rect DifficultyInformation{404.0F, 136.0F, 331.43F, 126.0F};
        constexpr Rect DifficultyHeading{16.0F, 14.0F, 145.0F, 16.0F};
        constexpr Rect CreatorHeading{169.71F, 14.0F, 145.0F, 16.0F};
        constexpr Rect SelectedDifficulty{16.0F, 32.0F, 145.0F, 28.0F};
        constexpr Rect SelectedCreator{169.71F, 32.0F, 145.0F, 28.0F};
        constexpr Rect InformationDivider{16.0F, 70.0F, 299.43F, 1.0F};
        constexpr Rect SelectedDetails{16.0F, 86.0F, 299.43F, 22.0F};

        constexpr Rect BrowserPanel{1359.71F, 20.0F, 506.29F, 926.0F};
        constexpr Rect SearchField{14.0F, 22.0F, 478.29F, 54.0F};
        constexpr Rect SearchCount{365.0F, 0.0F, 96.0F, 54.0F};
        constexpr Rect SortSelector{14.0F, 88.0F, 478.29F, 42.0F};
        constexpr Rect SongViewport{14.0F, 142.0F, 478.29F, 734.0F};
        constexpr Rect EmptySongMessage{28.0F, 320.0F, 422.29F, 46.0F};
        constexpr Rect BrowserHint{18.0F, 884.0F, 470.29F, 22.0F};

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
        constexpr Rect ScrollbarTrack{469.29F, 14.0F, 5.0F, 706.0F};

        constexpr Rect OptionButton{612.0F, 972.0F, 696.0F, 60.0F};
        constexpr Rect BackButton{-24.0F, 1006.0F, 280.0F, 82.0F};
        constexpr Rect BackLabel{34.0F, 10.0F, 210.0F, 44.0F};
        constexpr Rect GoButton{1664.0F, 1006.0F, 280.0F, 82.0F};
        constexpr Rect GoLabel{36.0F, 10.0F, 208.0F, 44.0F};
    } // namespace layout

    struct ResponsiveSongSelectLayout
    {
        Rect background;
        Rect category;
        Rect record;
        Rect information;
        Rect browser;
        Rect option;
        Rect back;
        Rect go;
    };

    [[nodiscard]] constexpr Rect CanvasTopLeftBounds(const float x, const float top,
                                                                    const float width,
                                                                    const float height) noexcept
    {
        return {x, layout::CanvasSize.height - top - height, width, height};
    }

    [[nodiscard]] constexpr ResponsiveSongSelectLayout CalculateResponsiveLayout(
        const float logicalWidth, const bool editorSongSelect = false) noexcept
    {
        constexpr float ReferenceOuterMargin = 54.0F * DesignToCanvasScale;
        constexpr float ReferenceColumnGap = 20.0F * DesignToCanvasScale;
        constexpr float ReferenceSideWidth = layout::RecordPanel.width * DesignToCanvasScale;
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
        const float availableWidth =
            std::max(logicalWidth - outerMargin * 2.0F - columnGap * 2.0F, 1.0F);
        const float columnScale = std::min(1.0F, availableWidth / ReferenceColumnWidth);
        const float sideWidth = ReferenceSideWidth * columnScale;
        const float informationWidth = std::max(availableWidth - sideWidth * 2.0F, 1.0F);
        const float recordX = outerMargin;
        const float informationX = recordX + sideWidth + columnGap;
        const float browserX = logicalWidth - outerMargin - sideWidth;

        constexpr float MainTop = 94.0F * DesignToCanvasScale;
        constexpr float MainHeight = 852.0F * DesignToCanvasScale;
        constexpr float BrowserTop = 20.0F * DesignToCanvasScale;
        constexpr float BrowserHeight = 926.0F * DesignToCanvasScale;
        constexpr float CategoryHeight = 56.0F * DesignToCanvasScale;
        const float categoryWidth = std::max(browserX - columnGap - recordX, 1.0F);

        const float footerScale = std::min(1.0F, logicalWidth / 640.0F);
        const float edgeWidth = layout::BackButton.width * DesignToCanvasScale * footerScale;
        const float edgeOverflow = -layout::BackButton.x * DesignToCanvasScale * footerScale;
        const float footerGap = ReferenceColumnGap * chromeScale;
        const float optionWidth =
            std::min(layout::OptionButton.width * DesignToCanvasScale,
                     std::max(logicalWidth - 2.0F * (edgeWidth - edgeOverflow + footerGap), 1.0F));
        const float optionX = (logicalWidth - optionWidth) * 0.5F;

        ResponsiveSongSelectLayout result{
            {0.0F, 0.0F, logicalWidth, layout::CanvasSize.height},
            CanvasTopLeftBounds(recordX, BrowserTop, categoryWidth, CategoryHeight),
            CanvasTopLeftBounds(recordX, MainTop, sideWidth, MainHeight),
            CanvasTopLeftBounds(informationX, MainTop, informationWidth, MainHeight),
            CanvasTopLeftBounds(browserX, BrowserTop, sideWidth, BrowserHeight),
            CanvasTopLeftBounds(optionX, layout::OptionButton.y * DesignToCanvasScale, optionWidth,
                                layout::OptionButton.height * DesignToCanvasScale),
            CanvasTopLeftBounds(-edgeOverflow, layout::BackButton.y * DesignToCanvasScale,
                                edgeWidth, layout::BackButton.height * DesignToCanvasScale),
            CanvasTopLeftBounds(logicalWidth - edgeWidth + edgeOverflow,
                                layout::GoButton.y * DesignToCanvasScale, edgeWidth,
                                layout::GoButton.height * DesignToCanvasScale)};
        if (editorSongSelect)
        {
            result.information.x = result.record.x;
            result.information.width =
                std::max(result.browser.x - columnGap - result.record.x, 1.0F);
        }
        return result;
    }

    constexpr ResponsiveSongSelectLayout PortraitLayout = CalculateResponsiveLayout(405.0F);
    static_assert(PortraitLayout.record.x >= 0.0F);
    static_assert(PortraitLayout.record.x + PortraitLayout.record.width <
                  PortraitLayout.information.x);
    static_assert(PortraitLayout.information.x + PortraitLayout.information.width <
                  PortraitLayout.browser.x);
    static_assert(PortraitLayout.browser.x + PortraitLayout.browser.width <= 405.0F);
    static_assert(PortraitLayout.back.x + PortraitLayout.back.width < PortraitLayout.option.x);
    static_assert(PortraitLayout.option.x + PortraitLayout.option.width < PortraitLayout.go.x);

    constexpr ResponsiveSongSelectLayout UltrawideLayout = CalculateResponsiveLayout(1680.0F);
    static_assert(UltrawideLayout.information.width >
                  layout::InformationPanel.width * DesignToCanvasScale);
    static_assert(UltrawideLayout.option.x + UltrawideLayout.option.width * 0.5F == 840.0F);

    constexpr ResponsiveSongSelectLayout EditorReferenceLayout =
        CalculateResponsiveLayout(1280.0F, true);
    static_assert(EditorReferenceLayout.information.x == EditorReferenceLayout.record.x);
    static_assert(EditorReferenceLayout.information.x + EditorReferenceLayout.information.width <
                  EditorReferenceLayout.browser.x);

    [[nodiscard]] inline Rect ScaleTopLeftBounds(const Rect bounds,
                                                                const float parentHeight) noexcept
    {
        const float scaledHeight = bounds.height * DesignToCanvasScale;
        return {bounds.x * DesignToCanvasScale,
                parentHeight - bounds.y * DesignToCanvasScale - scaledHeight,
                bounds.width * DesignToCanvasScale, scaledHeight};
    }

    [[nodiscard]] inline std::wstring BpmText(const double bpm)
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
    [[nodiscard]] inline ComponentType &RequireComponent(Visual2DNode &node)
    {
        ComponentType *const component = node.GetComponent<ComponentType>();
        if (component == nullptr)
        {
            throw std::logic_error("Song-select node '" + node.Name() +
                                   "' is missing a required component.");
        }
        return *component;
    }

    inline void ApplyFlatStyle(Visual2DNode &node, const Color color)
    {
        VisualStyle style{};
        style.normal = color;
        style.hovered = color;
        style.pressed = color;
        style.disabled = {color.red, color.green, color.blue, 0.45F};
        SetSkinBaseStyle(node, style);
    }

    inline void ApplySpriteStyle(Visual2DNode &node,
                                 const Color normal,
                                 const Color hovered)
    {
        VisualStyle style{};
        style.normal = normal;
        style.hovered = hovered;
        style.pressed = AccentBlue;
        style.disabled = DisabledBlue;
        SetSkinBaseStyle(node, style);
    }

    inline void ApplyButtonStyle(Visual2DNode &node,
                                 const Color normal,
                                 const Color hovered,
                                 const Color textColor)
    {
        ApplySpriteStyle(node, normal, hovered);
        RequireComponent<TextVisualComponent>(node).SetTextColor(textColor);
    }

    inline void SetCornerRadius(Visual2DNode &node, const float penpotRadius)
    {
        RequireComponent<SpriteVisualComponent>(node).SetCornerRadius(
            penpotRadius * DesignToCanvasScale);
    }

    [[nodiscard]] inline std::int64_t LatestPointerTimestamp(
        const InputState &input) noexcept
    {
        std::int64_t result{};
        for (const InputEvent &event : input.Events())
        {
            if (event.type == InputEventType::MouseMoved ||
                event.type == InputEventType::MouseButtonPressed ||
                event.type == InputEventType::MouseButtonReleased)
            {
                result = event.performanceCounterTicks;
            }
        }
        return result;
    }
} // namespace song_select
