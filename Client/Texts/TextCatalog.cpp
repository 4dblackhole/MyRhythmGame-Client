#include "TextCatalog.h"
#include "EnglishTexts.generated.h"
#include "FingerDrumAssets.h"
#include <stdexcept>

namespace finger_drum::texts
{
    namespace
    {
        const TranslationEntries &EnglishDefaults()
        {
            static const auto english = []
            {
                auto parsed = ParseLanguageFile(EnglishDefaultIni, "built-in English");
                if (!parsed.diagnostics.empty() || parsed.entries.empty())
                    throw std::logic_error("Built-in English language data is invalid.");
                return parsed.entries;
            }();
            return english;
        }
        std::size_t Index(Language language) noexcept
        {
            return language == Language::Korean ? 0U : 1U;
        }
    }
    TextCatalog::TextCatalog() : TextCatalog(assets::UserAssetRoot() / L"Languages")
    {
    }
    TextCatalog::TextCatalog(const std::filesystem::path &languageDirectory)
    {
        const auto &english = EnglishDefaults();
        files_[0] = ReadLanguageFile(languageDirectory / L"ko.ini");
        files_[1] = ReadLanguageFile(languageDirectory / L"en.ini");
        for (std::size_t i = 0; i < files_.size(); ++i)
        {
            auto &file = files_[i];
            ValidateTranslations(file, english);
            for (const auto &diagnostic : file.diagnostics)
                diagnostics_.push_back(std::string(i == 0 ? "ko.ini: " : "en.ini: ") + diagnostic);
        }
        profiles_ = {{{Language::Korean,
                       Get(Language::Korean, "Languages.Korean"),
                       {mrg::visual2d::TextFontSource::System, L"Segoe UI"}},
                      {Language::English,
                       Get(Language::English, "Languages.English"),
                       {mrg::visual2d::TextFontSource::System, L"Segoe UI"}}}};
    }
    std::wstring_view TextCatalog::Get(std::string_view key) const noexcept
    {
        return Get(language_, key);
    }
    std::wstring_view TextCatalog::Get(Language language, std::string_view key) const noexcept
    {
        const auto &selected = files_[Index(language)].entries;
        if (const auto found = selected.find(key); found != selected.end())
            return found->second;
        // A partial Korean pack always falls back to English.
        if (const auto found = files_[1].entries.find(key); found != files_[1].entries.end())
            return found->second;
        const auto &english = EnglishDefaults();
        const auto found = english.find(key);
        return found == english.end() ? std::wstring_view{} : std::wstring_view{found->second};
    }
    std::span<const std::string> TextCatalog::Diagnostics() const noexcept
    {
        return diagnostics_;
    }
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
        return profiles_[Index(language_)];
    }
    std::span<const LanguageProfile> TextCatalog::Languages() const noexcept
    {
        return profiles_;
    }
    void TextCatalog::SetLanguage(Language language) noexcept
    {
        if (language_ != language)
        {
            language_ = language;
            ++revision_;
        }
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
