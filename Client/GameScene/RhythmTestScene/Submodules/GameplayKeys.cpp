#include "GameplayPresenter.h"
#include "GameplaySupport.h"
using namespace gameplay;

void GameplayPresenter::CreateKeyIndicators(mrg::visual2d::Visual2DNode &inputPanel)
{
    struct KeySpec
    {
        mrg::visual2d::Rect bounds;
        mrg::visual2d::Color baseColor;
        std::wstring_view image;
    };
    constexpr std::array<KeySpec, 4> keys{{
        {{16.0F, 68.0F, 39.0F, 70.0F}, InputKeyBaseColors[0], L"KeyButtonLeftKat.png"},
        {{68.0F, 92.0F, 39.0F, 70.0F}, InputKeyBaseColors[1], L"KeyButtonLeftDon.png"},
        {{120.0F, 92.0F, 39.0F, 70.0F}, InputKeyBaseColors[2], L"KeyButtonRightDon.png"},
        {{172.0F, 68.0F, 39.0F, 70.0F}, InputKeyBaseColors[3], L"KeyButtonRightKat.png"},
    }};

    strongKeyLightImage_ = screenVisuals_.RegisterImage(InGameSkinAssetPath(L"KeyLightStrong.png"));
    weakKeyLightImage_ = screenVisuals_.RegisterImage(InGameSkinAssetPath(L"KeyLightWeak.png"));
    keyPressFlashImage_ = screenVisuals_.RegisterImage(InGameSkinAssetPath(L"KeyPressFlash.png"));
    const auto lightSize = ScaledImageSize(screenVisuals_, strongKeyLightImage_);
    const auto pressFlashSize = ScaledImageSize(screenVisuals_, keyPressFlashImage_);
    for (std::size_t index = 0; index < keys.size(); ++index)
    {
        const mrg::visual2d::Rect bounds{keys[index].bounds.x * InGameAssetScale,
                                         inputPanelSize_.height -
                                             (keys[index].bounds.y + keys[index].bounds.height) *
                                                 InGameAssetScale,
                                         keys[index].bounds.width * InGameAssetScale,
                                         keys[index].bounds.height * InGameAssetScale};
        auto &glow =
            mrg::visual2d::CreateSprite(inputPanel,
                                        {bounds.x - (lightSize.width - bounds.width) * 0.5F,
                                         bounds.y - (lightSize.height - bounds.height) * 0.5F,
                                         lightSize.width, lightSize.height},
                                        strongKeyLightImage_, "Input.Key.Glow");
        RequireComponent<mrg::visual2d::SpriteVisualComponent>(glow).SetTint(keys[index].baseColor);
        glow.SetVisible(false);
        glow.SetZIndex(1);
        keyGlows_[index] = &glow;

        const auto keyImage = screenVisuals_.RegisterImage(InGameSkinAssetPath(keys[index].image));
        auto &face = mrg::visual2d::CreateSprite(inputPanel, bounds, keyImage, "Input.Key.Face");
        RequireComponent<mrg::visual2d::SpriteVisualComponent>(face).SetTint(keys[index].baseColor);
        face.SetZIndex(2);
        keyIndicators_[index] = &face;

        auto &pressFlash =
            mrg::visual2d::CreateSprite(inputPanel,
                                        {bounds.x - (pressFlashSize.width - bounds.width) * 0.5F,
                                         bounds.y - (pressFlashSize.height - bounds.height) * 0.5F,
                                         pressFlashSize.width, pressFlashSize.height},
                                        keyPressFlashImage_, "Input.Key.PressFlash");
        pressFlash.SetVisible(false);
        pressFlash.SetZIndex(3);
        keyPressFlashes_[index] = &pressFlash;
    }
}

void GameplayPresenter::UpdateInputPresentation(const mrg::platform::InputState &input,
                                                const double deltaSeconds)
{
    for (std::size_t index = 0; index < keyIndicators_.size(); ++index)
    {
        const finger_drum::mode::TaikoInputBinding &binding =
            finger_drum::mode::TaikoInputBindings[index];
        const bool primaryPressed = input.IsKeyDown(binding.primaryKey);
        const bool secondaryPressed = std::ranges::any_of(
            binding.secondaryKeys,
            [&input](const finger_drum::rhythm::PhysicalKey key) { return input.IsKeyDown(key); });
        const bool pressedThisFrame =
            input.WasKeyPressed(binding.primaryKey) ||
            std::ranges::any_of(binding.secondaryKeys,
                                [&input](const finger_drum::rhythm::PhysicalKey key) {
                                    return input.WasKeyPressed(key);
                                });
        const mrg::visual2d::Color &faceColor =
            primaryPressed
                ? InputKeyStrongColors[index]
                : (secondaryPressed ? InputKeyWeakColors[index] : InputKeyBaseColors[index]);
        if (keyIndicators_[index] != nullptr)
        {
            RequireComponent<mrg::visual2d::SpriteVisualComponent>(*keyIndicators_[index])
                .SetTint(faceColor);
        }
        if (keyGlows_[index] != nullptr)
        {
            auto &glow = RequireComponent<mrg::visual2d::SpriteVisualComponent>(*keyGlows_[index]);
            glow.SetImage(primaryPressed ? strongKeyLightImage_ : weakKeyLightImage_);
            glow.SetTint(primaryPressed ? InputKeyStrongGlowColors[index]
                                        : InputKeyWeakGlowColors[index]);
            keyGlows_[index]->SetVisible(primaryPressed || secondaryPressed);
        }
        if (keyPressFlashes_[index] != nullptr)
        {
            double &remaining = keyPressFlashRemainingSeconds_[index];
            if (pressedThisFrame)
            {
                remaining = KeyPressFlashDurationSeconds;
            }
            else
            {
                remaining = std::max(0.0, remaining - std::max(deltaSeconds, 0.0));
            }
            auto &flash =
                RequireComponent<mrg::visual2d::SpriteVisualComponent>(*keyPressFlashes_[index]);
            flash.SetTint(
                {1.0F, 1.0F, 1.0F, static_cast<float>(remaining / KeyPressFlashDurationSeconds)});
            keyPressFlashes_[index]->SetVisible(remaining > 0.0);
        }
    }
}
