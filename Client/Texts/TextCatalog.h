#pragma once

#include "MRG_Core.h"

#include <cstdint>
#include <span>
#include <string_view>

namespace finger_drum::texts
{
    enum class Language : std::uint8_t
    {
        Korean,
        English,
    };

    struct LanguageProfile
    {
        Language language{};
        std::wstring_view displayName;
        mrg::visual2d::TextFont font;
    };

    class TextCatalog final
    {
      public:
        [[nodiscard]] Language CurrentLanguage() const noexcept;
        [[nodiscard]] std::uint64_t Revision() const noexcept;
        [[nodiscard]] const LanguageProfile &CurrentProfile() const noexcept;
        [[nodiscard]] std::span<const LanguageProfile> Languages() const noexcept;
        void SetLanguage(Language language) noexcept;
        void ApplyFont(mrg::visual2d::TextVisualComponent &text) const;
        void ApplyFont(mrg::visual2d::ComboBoxBehaviorComponent &combo) const;

      private:
        Language language_{Language::Korean};
        std::uint64_t revision_{};
    };
} // namespace finger_drum::texts
