#pragma once
#include "MRG_Core.h"
#include "App/AssetPaths.h"
#include "Taiko/TaikoMode.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <limits>
#include <stdexcept>

namespace gameplay
{
    using finger_drum::chart::MusicalPosition;
    using finger_drum::chart::PatternNote;
    using finger_drum::chart::Rational;
    using finger_drum::rhythm::NoteState;

    constexpr mrg::visual2d::Color DonRed{0.95F, 0.25F, 0.29F, 1.0F};
    constexpr mrg::visual2d::Color KatBlue{0.18F, 0.58F, 0.95F, 1.0F};
    constexpr mrg::visual2d::Color RollGold{1.0F, 0.64F, 0.12F, 1.0F};
    constexpr std::array<mrg::visual2d::Color, 4> InputKeyBaseColors{{
        {0.04F, 0.57F, 0.81F, 1.0F},
        {0.92F, 0.07F, 0.11F, 1.0F},
        {0.92F, 0.07F, 0.11F, 1.0F},
        {0.04F, 0.57F, 0.81F, 1.0F},
    }};
    constexpr std::array<mrg::visual2d::Color, 4> InputKeyWeakColors{{
        {0.17F, 0.72F, 0.96F, 1.0F},
        {1.0F, 0.18F, 0.25F, 1.0F},
        {1.0F, 0.18F, 0.25F, 1.0F},
        {0.17F, 0.72F, 0.96F, 1.0F},
    }};
    constexpr std::array<mrg::visual2d::Color, 4> InputKeyStrongColors{{
        {0.56F, 0.90F, 1.0F, 1.0F},
        {1.0F, 0.45F, 0.49F, 1.0F},
        {1.0F, 0.45F, 0.49F, 1.0F},
        {0.56F, 0.90F, 1.0F, 1.0F},
    }};
    constexpr std::array<mrg::visual2d::Color, 4> InputKeyWeakGlowColors{{
        {KatBlue.red, KatBlue.green, KatBlue.blue, 0.45F},
        {DonRed.red, DonRed.green, DonRed.blue, 0.45F},
        {DonRed.red, DonRed.green, DonRed.blue, 0.45F},
        {KatBlue.red, KatBlue.green, KatBlue.blue, 0.45F},
    }};
    constexpr std::array<mrg::visual2d::Color, 4> InputKeyStrongGlowColors{{
        {0.02F, 0.52F, 1.0F, 1.0F},
        {1.0F, 0.06F, 0.12F, 1.0F},
        {1.0F, 0.06F, 0.12F, 1.0F},
        {0.02F, 0.52F, 1.0F, 1.0F},
    }};
#if defined(_DEBUG)
    // Match the former RPG project's single compile-time switch. Set this to
    // false when a Debug build should use only the real QPC/DSP timeline.
    constexpr bool ReferenceTimeDebug = true;
#else
    constexpr bool ReferenceTimeDebug = false;
#endif
    constexpr float CanvasReferenceWidth = 1280.0F;
    constexpr float CanvasReferenceHeight = 720.0F;
    constexpr float InGameAssetScale = 2.0F / 3.0F;
    constexpr float GearMargin = 20.0F;
    constexpr float GearPadding = 4.0F;
    constexpr float GearRightMargin = 0.0F;
    constexpr float LaneCenterY = 0.0F;
    constexpr double KeyPressFlashDurationSeconds = 0.1;
    // A sixteenth note spans 85% of the normal head diameter at scroll 1x.
    constexpr float SixteenthSpacingInHeadDiameters = 0.85F;
    constexpr finger_drum::rhythm::RhythmDuration MissedTravelDuration{220'000};
    constexpr finger_drum::rhythm::RhythmDuration FocusSuccessDuration{200'000};
    constexpr float TwoPi = 6.28318530717958647692F;

    [[nodiscard]] inline mrg::visual2d::Size ScaledImageSize(
        const mrg::visual2d::ScreenVisual2DManager &rendering,
        const mrg::visual2d::ImageHandle image)
    {
        const mrg::visual2d::Size imageSize = rendering.GetImageSize(image);
        if (imageSize.width <= 0.0F || imageSize.height <= 0.0F)
        {
            throw std::logic_error("An in-game skin image must have a non-zero native size.");
        }
        return {imageSize.width * InGameAssetScale, imageSize.height * InGameAssetScale};
    }

    template <typename ComponentType>
    [[nodiscard]] inline ComponentType &RequireComponent(mrg::visual2d::Visual2DNode &node)
    {
        ComponentType *const component = node.GetComponent<ComponentType>();
        if (component == nullptr)
        {
            throw std::logic_error("A gameplay node is missing a component.");
        }
        return *component;
    }

    inline void SetColor(mrg::visual2d::Visual2DNode &node, const mrg::visual2d::Color color)
    {
        mrg::visual2d::VisualStyle style{};
        style.normal = color;
        style.hovered = color;
        style.pressed = color;
        style.disabled = {color.red, color.green, color.blue, 0.25F};
        RequireComponent<mrg::visual2d::SpriteVisualComponent>(node).SetStyle(style);
    }

    [[nodiscard]] inline std::filesystem::path InGameSkinAssetPath(
        const std::filesystem::path &file)
    {
        return mrg_client::asset_paths::default_skin::InGame(file);
    }

    [[nodiscard]] inline std::filesystem::path TaikoHitSoundAssetPath(
        const std::filesystem::path &file)
    {
        return mrg_client::asset_paths::default_skin::TaikoHitSound(file);
    }

    [[nodiscard]] inline std::wstring Utf8ToWide(
        const std::string_view value,
        const std::wstring_view invalidText = L"Audio initialization failed.")
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

    inline void AddPatternNote(finger_drum::chart::PatternDocument &pattern,
                               const std::int64_t measure, const std::int64_t numerator,
                               const std::int64_t denominator,
                               const finger_drum::mode::TaikoNoteType type,
                               const finger_drum::mode::TaikoPatternAction action,
                               std::vector<std::string> extraData = {})
    {
        PatternNote note;
        note.position = MusicalPosition{measure, Rational{numerator, denominator}};
        note.keyType = static_cast<int>(type);
        note.actionType = static_cast<int>(action);
        note.extraData = std::move(extraData);
        note.sourceOrder = pattern.notes.size();
        pattern.notes.push_back(std::move(note));
    }

    [[nodiscard]] inline bool IsBigVisual(
        const finger_drum::mode::NoteVisualKind visualKind) noexcept
    {
        return visualKind == finger_drum::mode::NoteVisualKind::BigDon ||
               visualKind == finger_drum::mode::NoteVisualKind::BigKat ||
               visualKind == finger_drum::mode::NoteVisualKind::BigRoll ||
               visualKind == finger_drum::mode::NoteVisualKind::Balloon ||
               visualKind == finger_drum::mode::NoteVisualKind::DengDeng ||
               visualKind == finger_drum::mode::NoteVisualKind::Purple;
    }

    [[nodiscard]] inline bool IsLongVisual(
        const finger_drum::mode::NoteVisualKind visualKind) noexcept
    {
        return visualKind == finger_drum::mode::NoteVisualKind::Roll ||
               visualKind == finger_drum::mode::NoteVisualKind::BigRoll ||
               visualKind == finger_drum::mode::NoteVisualKind::Balloon ||
               visualKind == finger_drum::mode::NoteVisualKind::DengDeng ||
               visualKind == finger_drum::mode::NoteVisualKind::DonBuzz ||
               visualKind == finger_drum::mode::NoteVisualKind::KatBuzz;
    }

    [[nodiscard]] inline mrg::visual2d::Color AmbientColor(
        const finger_drum::mode::NoteVisualKind visualKind) noexcept
    {
        if (visualKind == finger_drum::mode::NoteVisualKind::Purple)
        {
            return {0.65F, 0.25F, 0.90F, 1.0F};
        }
        if (visualKind == finger_drum::mode::NoteVisualKind::Kat ||
            visualKind == finger_drum::mode::NoteVisualKind::BigKat ||
            visualKind == finger_drum::mode::NoteVisualKind::KatBuzz)
        {
            return KatBlue;
        }
        if (visualKind == finger_drum::mode::NoteVisualKind::Roll ||
            visualKind == finger_drum::mode::NoteVisualKind::BigRoll ||
            visualKind == finger_drum::mode::NoteVisualKind::Balloon ||
            visualKind == finger_drum::mode::NoteVisualKind::DengDeng)
        {
            return RollGold;
        }
        return DonRed;
    }
} // namespace gameplay
