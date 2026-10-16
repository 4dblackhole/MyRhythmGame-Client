#pragma once

#include "MRG_Core.h"
#include "Submodules/LanguageFile.h"

#include <array>
#include <cstdint>
#include <format>
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
        TextCatalog();
        explicit TextCatalog(const std::filesystem::path &languageDirectory);
        TextCatalog(const TextCatalog &) = delete;
        TextCatalog &operator=(const TextCatalog &) = delete;
        // Strings are immutable for this catalog's lifetime. Views may retain
        // them until the owning game shuts down.
        [[nodiscard]] std::wstring_view Get(std::string_view key) const noexcept;
        [[nodiscard]] std::wstring_view Get(Language language, std::string_view key) const noexcept;
        [[nodiscard]] std::span<const std::string> Diagnostics() const noexcept;
        template <typename... Args> std::wstring Format(std::string_view key, Args &&...args) const
        {
            return std::vformat(Get(key), std::make_wformat_args(args...));
        }
        [[nodiscard]] Language CurrentLanguage() const noexcept;
        [[nodiscard]] std::uint64_t Revision() const noexcept;
        [[nodiscard]] const LanguageProfile &CurrentProfile() const noexcept;
        [[nodiscard]] std::span<const LanguageProfile> Languages() const noexcept;
        void SetLanguage(Language language) noexcept;
        void ApplyFont(mrg::visual2d::TextVisualComponent &text) const;
        void ApplyFont(mrg::visual2d::ComboBoxBehaviorComponent &combo) const;

      private:
        std::array<LanguageFile, 2> files_;
        std::array<LanguageProfile, 2> profiles_;
        std::vector<std::string> diagnostics_;
        Language language_{Language::Korean};
        std::uint64_t revision_{};
    };
} // namespace finger_drum::texts
