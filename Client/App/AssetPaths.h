#pragma once

#include "YOUMUAssets.h"
#include "App/SkinSetSelection.h"

#include <filesystem>

// Runtime asset paths are centralized here so source code does not duplicate
// directory strings when the asset layout changes.

[[nodiscard]] inline std::filesystem::path UserSongs()
{
    return UserSongsRoot();
}

[[nodiscard]] inline std::filesystem::path NotoSansKR()
{
    return ResolveBuiltInAsset(L"assets\\fonts\\NotoSansKR.ttf");
}
[[nodiscard]] inline std::filesystem::path YOUMU()
{
    return ResolveBuiltInAsset(
        L"assets\\fonts\\Rajdhani-SemiBold.ttf");
}

[[nodiscard]] inline std::filesystem::path ExamplePixel()
{
    return ResolveBuiltInAsset(
        L"assets\\fonts\\PressStart2P-Regular.ttf");
}

[[nodiscard]] inline std::filesystem::path EditorSound(const std::filesystem::path &file)
{
    return SkinSetSelection::Instance().Resolve(std::filesystem::path(L"Editor") / file);
}
[[nodiscard]] inline std::filesystem::path Result(const std::filesystem::path &file)
{
    return SkinSetSelection::Instance().Image("Result", file.generic_string());
}
[[nodiscard]] inline std::filesystem::path InGame(
    const std::filesystem::path& file)
{
    return SkinSetSelection::Instance().Image("InGame", file.generic_string());
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

[[nodiscard]] inline std::filesystem::path LeftFade()
{
    return SkinSetSelection::Instance().Image("Logo", "LogoLeft");
}

[[nodiscard]] inline std::filesystem::path Center()
{
    return SkinSetSelection::Instance().Image("Logo", "LogoCenter");
}

[[nodiscard]] inline std::filesystem::path RightFade()
{
    return SkinSetSelection::Instance().Image("Logo", "LogoRight");
}

[[nodiscard]] inline std::filesystem::path SelectionCursor()
{
    return SkinSetSelection::Instance().Image("Logo", "SelectionCursor");
}

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
