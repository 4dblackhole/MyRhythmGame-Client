#pragma once

#include "MRG_Core.h"

#include <cstddef>
#include <string>
#include <string_view>

namespace finger_drum::presentation
{
    // Keeps the complete Unicode string and moves its prepared glyphs inside
    // a fixed clip. The engine supplies measured width and a shared second copy.
    class MarqueeTextComponent final : public mrg::visual2d::Visual2DComponent
    {
      public:
        explicit MarqueeTextComponent(std::wstring text = {}, double secondsPerCharacter = 0.28);

        void SetText(std::wstring text);
        [[nodiscard]] std::wstring_view Text() const noexcept;
        void Update(double elapsedSeconds) override;

      private:
        void RefreshDisplayedText();
        [[nodiscard]] std::size_t CharacterCount() const noexcept;
        [[nodiscard]] mrg::visual2d::TextVisualComponent &Visual();

        static constexpr std::size_t GapCharacters = 5;
        static constexpr double EndPauseSeconds = 1.0;

        std::wstring text_;
        double secondsPerCharacter_{0.28};
        double elapsedSeconds_{};
        double offset_{};
        std::size_t characters_{1};
        bool displayDirty_{true};
    };
} // namespace finger_drum::presentation
