#include "Texts/EditorScene/EditorTexts.h"
#include "Texts/GameScene/FingerDrumLogoScene/LogoTexts.h"
#include "Texts/GameScene/ResultScene/ResultTexts.h"
#include "Texts/TextCatalog.h"
#include <Windows.h>
#include <format>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <type_traits>

namespace
{
    namespace t = finger_drum::texts;
    void Check(bool value, const char *message)
    {
        if (!value)
            throw std::runtime_error(message);
    }
    void Write(const std::filesystem::path &path, std::string_view bytes)
    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        if (!out)
            throw std::runtime_error("Could not write the isolated language fixture.");
    }
    void Parser()
    {
        const auto parsed = t::ParseLanguageFile("\xEF\xBB\xBF[Logo]\r\nStart=\"시작 😀\"\r\n"
                                                 R"ini(Options="  Options; # = \"quoted\"\nnext\tline\\  "
)ini"
                                                 "Exit=\"\"\nStart=duplicate\nBroken=\"unterminated\nEditor=Editor",
                                                 "fixture");
        Check(parsed.entries.at("Logo.Start") == L"시작 😀" && parsed.entries.at("Logo.Editor") == L"Editor" &&
                  parsed.entries.at("Logo.Exit").empty(),
              "BOM/UTF-8/CRLF/empty values must be supported.");
        Check(parsed.entries.at("Logo.Options") == L"  Options; # = \"quoted\"\nnext\tline\\  ",
              "Quoted escapes, delimiters and edge whitespace must round-trip.");
        Check(parsed.diagnostics.size() == 2, "Only duplicate and malformed entries must be diagnosed.");
        const auto invalid = t::ParseLanguageFile("[Logo]\nStart=\xFF", "invalid UTF-8");
        Check(invalid.entries.empty() && !invalid.diagnostics.empty(), "Invalid UTF-8 must not enter UI text.");
        auto formats =
            t::ParseLanguageFile("[Result]\npercent=\"{999999999999999999999999999999999999}%\"\n", "overflow");
        t::ValidateTranslations(formats, {{"Result.percent", L"{:.6f}%"}});
        Check(formats.entries.empty(), "An overflowing argument index must fall back safely.");
    }
    void Catalog(const std::filesystem::path &directory)
    {
        t::TextCatalog empty(directory);
        Check(t::Logo(empty).menu[0] == L"Game Start" && t::Editor(empty).save == L"SAVE",
              "Missing Korean and English files must use built-in English for every screen.");
        Write(directory / "en.ini", "[Logo]\nOptions=Preferences\n");
        Write(directory / "ko.ini", "[Logo]\nStart=\"직접 시작\"\nExit=\"\"\n"
                                    "[Result]\npercent=\"{:d}%\"\nnoteCounts=\"Pending {2} / total {0} / done {1}\"\n"
                                    "[Unknown]\nkey=ignored\n");
        t::TextCatalog catalog(directory);
        const auto logo = t::Logo(catalog);
        Check(logo.menu[0] == L"직접 시작" && logo.menu[1] == L"Editor" && logo.menu[2] == L"Preferences" &&
                  logo.menu[3].empty(),
              "Selected key -> external English -> built-in English must resolve per entry.");
        Check(t::Result(catalog).percent == L"{:.6f}%" && catalog.Diagnostics().size() == 2,
              "Incompatible format types and unknown keys must not replace defaults.");
        const int total = 12, done = 5, pending = 7;
        Check(catalog.Format("Result.noteCounts", total, done, pending) ==
                  L"Pending 7 / total 12 / done 5",
              "Translations may reorder existing arguments by index.");
        const auto revision = catalog.Revision();
        catalog.SetLanguage(t::Language::English);
        catalog.SetLanguage(t::Language::English);
        Check(catalog.Revision() == revision + 1 && t::Logo(catalog).menu[0] == L"Game Start" &&
                  logo.menu[0] == L"직접 시작",
              "Switching languages must retain previous string-view lifetimes.");
        Write(directory / "ko.ini", "[Logo]\nStart=\"다음 실행\"\n");
        catalog.SetLanguage(t::Language::Korean);
        Check(t::Logo(catalog).menu[0] == L"직접 시작",
              "File I/O must not occur during text lookup or language switching.");
        const t::TextCatalog restarted(directory);
        Check(t::Logo(restarted).menu[0] == L"다음 실행", "A new catalog must read replaced language files.");
        Write(directory / "ko.ini", "[Logo]\nStart=\xFF");
        const t::TextCatalog corrupt(directory);
        Check(t::Logo(corrupt).menu[0] == L"Game Start", "A corrupt Korean pack must still fall back to English.");
    }
} // namespace
int main()
{
    try
    {
        static_assert(!std::is_copy_constructible_v<finger_drum::texts::TextCatalog>);
        Parser();
        const auto directory =
            std::filesystem::absolute("build/obj/LanguageFileTests/Fixtures") /
            (std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64()));
        std::filesystem::create_directories(directory);
        Catalog(directory);
        const finger_drum::texts::TextCatalog shipped("Client/Texts/Languages");
        Check(shipped.Diagnostics().empty() && finger_drum::texts::Logo(shipped).menu[0] == L"게임 시작",
              "Both shipped language files must parse and preserve the existing Korean translation.");
        std::cout
            << "UTF-8 language files, English fallback, format validation and immutable catalog ownership passed.\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
