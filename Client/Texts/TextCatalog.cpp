#include "TextCatalog.h"

#include <array>

namespace finger_drum::texts
{
    namespace
    {
        const std::array<LanguageProfile, 2> Profiles{{
            {Language::Korean,
             L"한국어",
             {mrg::visual2d::TextFontSource::System, L"Segoe UI"}},
            {Language::English,
             L"English",
             {mrg::visual2d::TextFontSource::System, L"Segoe UI"}},
        }};

        [[nodiscard]] const LanguageProfile &Profile(const Language language) noexcept
        {
            return Profiles[language == Language::English ? 1U : 0U];
        }
    } // namespace

    Language TextCatalog::CurrentLanguage() const noexcept
    {
        return language_;
    }

    std::uint64_t TextCatalog::Revision() const noexcept
    {
        return revision_;
    }

    const LanguageProfile &TextCatalog::CurrentProfile() const noexcept
    {
        return Profile(language_);
    }

    std::span<const LanguageProfile> TextCatalog::Languages() const noexcept
    {
        return Profiles;
    }

    void TextCatalog::SetLanguage(const Language language) noexcept
    {
        if (language_ == language)
        {
            return;
        }
        language_ = language;
        ++revision_;
    }

    void TextCatalog::ApplyFont(mrg::visual2d::TextVisualComponent &text) const
    {
        text.SetFont(CurrentProfile().font);
    }

    void TextCatalog::ApplyFont(mrg::visual2d::ComboBoxBehaviorComponent &combo) const
    {
        combo.SetFont(CurrentProfile().font);
    }
} // namespace finger_drum::texts
