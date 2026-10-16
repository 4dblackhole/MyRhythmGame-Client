#pragma once
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace finger_drum::texts
{
    using TranslationEntries = std::map<std::string, std::wstring, std::less<>>;
    struct LanguageFile
    {
        TranslationEntries entries;
        std::vector<std::string> diagnostics;
    };
    [[nodiscard]] LanguageFile ParseLanguageFile(std::string_view source, std::string_view name);
    [[nodiscard]] LanguageFile ReadLanguageFile(const std::filesystem::path &path);
    // Reject individual invalid translations before any UI keeps a string view.
    void ValidateTranslations(LanguageFile &file, const TranslationEntries &english);
} // namespace finger_drum::texts
