#pragma once

#include "MRG_Core.h"

#include <cstddef>
#include <string>
#include <string_view>

namespace finger_drum::presentation
{
    // Keeps text inside a fixed layout box by exposing a moving character
    // window only when the full value is too long. Short values stay still.
    // This avoids draw overflow even before Visual2D gains per-node clipping.
    class MarqueeTextComponent final
        : public mrg::visual2d::Visual2DComponent
    {
    public:
        explicit MarqueeTextComponent(
            std::wstring text = {},
            std::size_t maximumVisibleCharacters = 32,
            double secondsPerCharacter = 0.28);

        void SetText(std::wstring text);
        [[nodiscard]] std::wstring_view Text() const noexcept;
        void Update(double elapsedSeconds) override;

    private:
        void RefreshDisplayedText();
        [[nodiscard]] std::wstring BuildWindow() const;

        static constexpr std::size_t GapCharacters = 5;
        static constexpr double EndPauseSeconds = 1.0;

        std::wstring text_;
        std::size_t maximumVisibleCharacters_{32};
        double secondsPerCharacter_{0.28};
        double elapsedSeconds_{};
        std::size_t characterOffset_{};
        bool displayDirty_{true};
    };
}
