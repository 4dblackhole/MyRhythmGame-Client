#pragma once
#include "PhantomCore.h"
#include "App/AssetPaths.h"
#include "Mode/Submodules/PlaySessionTypes.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <limits>
#include <stdexcept>

constexpr Color DonRed{0.95F, 0.25F, 0.29F, 1.0F};
constexpr Color KatBlue{0.18F, 0.58F, 0.95F, 1.0F};
constexpr Color RollGold{1.0F, 0.64F, 0.12F, 1.0F};
constexpr std::array<Color, 4> InputKeyBaseColors{{
    {0.04F, 0.57F, 0.81F, 1.0F},
    {0.92F, 0.07F, 0.11F, 1.0F},
    {0.92F, 0.07F, 0.11F, 1.0F},
    {0.04F, 0.57F, 0.81F, 1.0F},
}};
constexpr std::array<Color, 4> InputKeyWeakColors{{
    {0.17F, 0.72F, 0.96F, 1.0F},
    {1.0F, 0.18F, 0.25F, 1.0F},
    {1.0F, 0.18F, 0.25F, 1.0F},
    {0.17F, 0.72F, 0.96F, 1.0F},
}};
constexpr std::array<Color, 4> InputKeyStrongColors{{
    {0.56F, 0.90F, 1.0F, 1.0F},
    {1.0F, 0.45F, 0.49F, 1.0F},
    {1.0F, 0.45F, 0.49F, 1.0F},
    {0.56F, 0.90F, 1.0F, 1.0F},
}};
constexpr std::array<Color, 4> InputKeyWeakGlowColors{{
    {KatBlue.red, KatBlue.green, KatBlue.blue, 0.45F},
    {DonRed.red, DonRed.green, DonRed.blue, 0.45F},
    {DonRed.red, DonRed.green, DonRed.blue, 0.45F},
    {KatBlue.red, KatBlue.green, KatBlue.blue, 0.45F},
}};
constexpr std::array<Color, 4> InputKeyStrongGlowColors{{
    {0.02F, 0.52F, 1.0F, 1.0F},
    {1.0F, 0.06F, 0.12F, 1.0F},
    {1.0F, 0.06F, 0.12F, 1.0F},
    {0.02F, 0.52F, 1.0F, 1.0F},
}};
constexpr float CanvasReferenceWidth = 1280.0F;
constexpr float CanvasReferenceHeight = 720.0F;
constexpr float InGameAssetScale = 2.0F / 3.0F;
constexpr float GearMargin = 20.0F;
constexpr float GearPadding = 4.0F;
constexpr float GearRightMargin = 0.0F;
constexpr double KeyPressFlashDurationSeconds = 0.1;
// A sixteenth note spans 85% of the normal head diameter at scroll 1x.
constexpr float SixteenthSpacingInHeadDiameters = 0.85F;
constexpr RhythmDuration MissedTravelDuration{220'000};
constexpr RhythmDuration FocusSuccessDuration{200'000};
constexpr float TwoPi = 6.28318530717958647692F;

[[nodiscard]] inline Size ScaledImageSize(
    const ScreenVisual2DManager &rendering,
    const ImageHandle image)
{
    const Size imageSize = rendering.GetImageSize(image);
    if (imageSize.width <= 0.0F || imageSize.height <= 0.0F)
    {
        throw std::logic_error("An in-game skin image must have a non-zero native size.");
    }
    return {imageSize.width * InGameAssetScale, imageSize.height * InGameAssetScale};
}

template <typename ComponentType>
[[nodiscard]] inline ComponentType &RequireComponent(Visual2DNode &node)
{
    ComponentType *const component = node.GetComponent<ComponentType>();
    if (component == nullptr)
    {
        throw std::logic_error("A gameplay node is missing a component.");
    }
    return *component;
}

inline void SetColor(Visual2DNode &node, const Color color)
{
    VisualStyle style{};
    style.normal = color;
    style.hovered = color;
    style.pressed = color;
    style.disabled = {color.red, color.green, color.blue, 0.25F};
    RequireComponent<SpriteVisualComponent>(node).SetStyle(style);
}

[[nodiscard]] inline std::filesystem::path InGameSkinAssetPath(
    const std::filesystem::path &file)
{
    return InGame(file);
}

[[nodiscard]] inline std::wstring Utf8ToWide(const std::string_view value, const std::wstring_view invalidText)
{
    if (value.empty())
    {
        return {};
    }
    const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                                           static_cast<int>(value.size()), nullptr, 0);
    if (length <= 0)
    {
        return std::wstring(invalidText);
    }
    std::wstring result(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                        static_cast<int>(value.size()), result.data(), length);
    return result;
}

[[nodiscard]] inline bool IsBigVisual(
    const NoteVisualKind visualKind) noexcept
{
    return visualKind == NoteVisualKind::BigDon ||
           visualKind == NoteVisualKind::BigKat ||
           visualKind == NoteVisualKind::BigRoll ||
           visualKind == NoteVisualKind::Balloon ||
           visualKind == NoteVisualKind::DengDeng ||
           visualKind == NoteVisualKind::Purple;
}

[[nodiscard]] inline bool IsLongVisual(
    const NoteVisualKind visualKind) noexcept
{
    return visualKind == NoteVisualKind::Roll ||
           visualKind == NoteVisualKind::BigRoll ||
           visualKind == NoteVisualKind::Balloon ||
           visualKind == NoteVisualKind::DengDeng ||
           visualKind == NoteVisualKind::DonBuzz ||
           visualKind == NoteVisualKind::KatBuzz;
}

[[nodiscard]] inline Color AmbientColor(
    const NoteVisualKind visualKind) noexcept
{
    if (visualKind == NoteVisualKind::Purple)
    {
        return {0.65F, 0.25F, 0.90F, 1.0F};
    }
    if (visualKind == NoteVisualKind::Kat ||
        visualKind == NoteVisualKind::BigKat ||
        visualKind == NoteVisualKind::KatBuzz)
    {
        return KatBlue;
    }
    if (visualKind == NoteVisualKind::Roll ||
        visualKind == NoteVisualKind::BigRoll ||
        visualKind == NoteVisualKind::Balloon ||
        visualKind == NoteVisualKind::DengDeng)
    {
        return RollGold;
    }
    return DonRed;
}
