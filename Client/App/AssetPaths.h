#pragma once

#include "FingerDrumAssets.h"

#include <filesystem>

// Runtime asset paths are centralized here so source code does not duplicate
// directory strings when the asset layout changes.
namespace mrg_client::asset_paths
{
    [[nodiscard]] inline std::filesystem::path UserSongs()
    {
        return finger_drum::assets::UserSongsRoot();
    }

    namespace fonts
    {
        [[nodiscard]] inline std::filesystem::path FingerDrum()
        {
            return finger_drum::assets::ResolveBuiltInAsset(
                L"assets\\fonts\\Rajdhani-SemiBold.ttf");
        }

        [[nodiscard]] inline std::filesystem::path ExamplePixel()
        {
            return finger_drum::assets::ResolveBuiltInAsset(
                L"assets\\fonts\\PressStart2P-Regular.ttf");
        }
    }

    namespace default_skin
    {
        [[nodiscard]] inline std::filesystem::path InGame(
            const std::filesystem::path& file)
        {
            return finger_drum::assets::ResolveDefaultSkinAsset(
                std::filesystem::path(L"InGame") / file);
        }

        [[nodiscard]] inline std::filesystem::path JudgeImage(
            const std::filesystem::path& file)
        {
            return finger_drum::assets::ResolveDefaultSkinAsset(
                std::filesystem::path(L"JudgeImage") / file);
        }

        [[nodiscard]] inline std::filesystem::path NumberImage(
            const std::filesystem::path& file)
        {
            return finger_drum::assets::ResolveDefaultSkinAsset(
                std::filesystem::path(L"NumberImage") / file);
        }

        [[nodiscard]] inline std::filesystem::path TaikoHitSound(
            const std::filesystem::path& file)
        {
            return finger_drum::assets::ResolveDefaultSkinAsset(
                std::filesystem::path(L"HitSounds\\TaikoMode") / file);
        }

        namespace title
        {
            [[nodiscard]] inline std::filesystem::path LeftFade()
            {
                return finger_drum::assets::ResolveDefaultSkinAsset(
                    L"TitleImage\\Logo\\LeftFade.png");
            }

            [[nodiscard]] inline std::filesystem::path Center()
            {
                return finger_drum::assets::ResolveDefaultSkinAsset(
                    L"TitleImage\\Logo\\Center.png");
            }

            [[nodiscard]] inline std::filesystem::path RightFade()
            {
                return finger_drum::assets::ResolveDefaultSkinAsset(
                    L"TitleImage\\Logo\\RightFade.png");
            }

            [[nodiscard]] inline std::filesystem::path SelectionCursor()
            {
                return finger_drum::assets::ResolveDefaultSkinAsset(
                    L"TitleImage\\Menu\\SelectionCursor.png");
            }
        }

        namespace widget
        {
            [[nodiscard]] inline std::filesystem::path First()
            {
                return finger_drum::assets::ResolveDefaultSkinAsset(
                    L"TitleImage\\Widget\\Widget1.png");
            }

            [[nodiscard]] inline std::filesystem::path Second()
            {
                return finger_drum::assets::ResolveDefaultSkinAsset(
                    L"TitleImage\\Widget\\Widget2.png");
            }
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
