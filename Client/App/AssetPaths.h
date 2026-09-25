#pragma once

#include "FingerDrumAssets.h"
#include "App/SkinSetSelection.h"

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

    namespace skin
    {
        [[nodiscard]] inline std::filesystem::path InGame(
            const std::filesystem::path& file)
        {
            return SkinSetSelection::Instance().Resolve(
                std::filesystem::path(L"InGame") / file);
        }

        [[nodiscard]] inline std::filesystem::path JudgeImage(
            const std::filesystem::path& file)
        {
            return SkinSetSelection::Instance().Resolve(
                std::filesystem::path(L"JudgeImage") / file);
        }

        [[nodiscard]] inline std::filesystem::path NumberImage(
            const std::filesystem::path& file)
        {
            return SkinSetSelection::Instance().Resolve(
                std::filesystem::path(L"NumberImage") / file);
        }

        [[nodiscard]] inline std::filesystem::path TaikoHitSound(
            const std::filesystem::path& file)
        {
            return SkinSetSelection::Instance().Resolve(
                std::filesystem::path(L"HitSounds\\TaikoMode") / file);
        }

        namespace title
        {
            [[nodiscard]] inline std::filesystem::path LeftFade()
            {
                return SkinSetSelection::Instance().Resolve(
                    L"TitleImage\\Logo\\LeftFade.png");
            }

            [[nodiscard]] inline std::filesystem::path Center()
            {
                return SkinSetSelection::Instance().Resolve(
                    L"TitleImage\\Logo\\Center.png");
            }

            [[nodiscard]] inline std::filesystem::path RightFade()
            {
                return SkinSetSelection::Instance().Resolve(
                    L"TitleImage\\Logo\\RightFade.png");
            }

            [[nodiscard]] inline std::filesystem::path SelectionCursor()
            {
                return SkinSetSelection::Instance().Resolve(
                    L"TitleImage\\Menu\\SelectionCursor.png");
            }
        }

        namespace widget
        {
            [[nodiscard]] inline std::filesystem::path First()
            {
                return SkinSetSelection::Instance().Resolve(
                    L"TitleImage\\Widget\\Widget1.png");
            }

            [[nodiscard]] inline std::filesystem::path Second()
            {
                return SkinSetSelection::Instance().Resolve(
                    L"TitleImage\\Widget\\Widget2.png");
            }
        }
    }

}
