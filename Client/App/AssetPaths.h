#pragma once

// Runtime asset paths are centralized here so source code does not duplicate
// directory strings when the asset layout changes.
namespace mrg_client::asset_paths
{
    inline constexpr wchar_t Songs[] = L"assets\\songs";

    namespace fonts
    {
        inline constexpr wchar_t FingerDrum[] =
            L"assets\\fonts\\Rajdhani-SemiBold.ttf";
        inline constexpr wchar_t ExamplePixel[] =
            L"assets\\fonts\\PressStart2P-Regular.ttf";
    }

    namespace default_skin
    {
        inline constexpr wchar_t InGame[] =
            L"assets\\skins\\Default Skin\\InGame";
        inline constexpr wchar_t JudgeImages[] =
            L"assets\\skins\\Default Skin\\JudgeImage";
        inline constexpr wchar_t NumberImages[] =
            L"assets\\skins\\Default Skin\\NumberImage";
        inline constexpr wchar_t MeasureLine[] =
            L"assets\\skins\\Default Skin\\InGame\\MeasureLine.png";
        inline constexpr wchar_t TaikoHitSounds[] =
            L"assets\\skins\\Default Skin\\HitSounds\\TaikoMode";

        namespace title
        {
            inline constexpr wchar_t LeftFade[] =
                L"assets\\skins\\Default Skin\\TitleImage\\Logo\\LeftFade.png";
            inline constexpr wchar_t Center[] =
                L"assets\\skins\\Default Skin\\TitleImage\\Logo\\Center.png";
            inline constexpr wchar_t RightFade[] =
                L"assets\\skins\\Default Skin\\TitleImage\\Logo\\RightFade.png";
            inline constexpr wchar_t SelectionCursor[] =
                L"assets\\skins\\Default Skin\\TitleImage\\Menu\\SelectionCursor.png";
        }

        namespace widget
        {
            inline constexpr wchar_t First[] =
                L"assets\\skins\\Default Skin\\TitleImage\\Widget\\Widget1.png";
            inline constexpr wchar_t Second[] =
                L"assets\\skins\\Default Skin\\TitleImage\\Widget\\Widget2.png";
        }
    }

    namespace unused_examples
    {
        inline constexpr wchar_t WhiteCubeTexture[] =
            L"assets\\Unused\\awhc.png";
        inline constexpr wchar_t BlackCubeTexture[] =
            L"assets\\Unused\\bwhc.png";
        inline constexpr wchar_t PopSound[] =
            L"assets\\Unused\\Sounds\\pop.wav";
    }
}
