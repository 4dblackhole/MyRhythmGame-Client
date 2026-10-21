#include "Texts/EditorScene/EditorTexts.h"
#include "Texts/GameScene/YOUMULogoScene/LogoTexts.h"
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
        const auto parsed = ParseLanguageFile("\xEF\xBB\xBF[Logo]\r\nStart=\"시작 😀\"\r\n"
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
        const auto invalid = ParseLanguageFile("[Logo]\nStart=\xFF", "invalid UTF-8");
        Check(invalid.entries.empty() && !invalid.diagnostics.empty(), "Invalid UTF-8 must not enter UI text.");
        auto formats =
            ParseLanguageFile("[Result]\npercent=\"{999999999999999999999999999999999999}%\"\n", "overflow");
        ValidateTranslations(formats, {{"Result.percent", L"{:.6f}%"}});
        Check(formats.entries.empty(), "An overflowing argument index must fall back safely.");
    }
    void Catalog(const std::filesystem::path &directory)
    {
        TextCatalog empty(directory);
        Check(Logo(empty).menu[0] == L"Game Start" && Editor(empty).save == L"SAVE",
              "Missing Korean and English files must use built-in English for every screen.");
        Write(directory / "en.ini", "[Logo]\nOptions=Preferences\n");
        Write(directory / "ko.ini", "[Logo]\nStart=\"직접 시작\"\nExit=\"\"\n"
                                    "[Result]\npercent=\"{:d}%\"\nnoteCounts=\"Pending {2} / total {0} / done {1}\"\n"
                                    "[Unknown]\nkey=ignored\n");
        TextCatalog catalog(directory);
        const auto logo = Logo(catalog);
        Check(logo.menu[0] == L"직접 시작" && logo.menu[1] == L"Editor" && logo.menu[2] == L"Preferences" &&
                  logo.menu[3].empty(),
              "Selected key -> external English -> built-in English must resolve per entry.");
        Check(Result(catalog).percent == L"{:.6f}%" && catalog.Diagnostics().size() == 2,
              "Incompatible format types and unknown keys must not replace defaults.");
        const int total = 12, done = 5, pending = 7;
        Check(catalog.Format("Result.noteCounts", total, done, pending) ==
                  L"Pending 7 / total 12 / done 5",
              "Translations may reorder existing arguments by index.");
        const auto revision = catalog.Revision();
        catalog.SetLanguage(Language::English);
        catalog.SetLanguage(Language::English);
        Check(catalog.Revision() == revision + 1 && Logo(catalog).menu[0] == L"Game Start" &&
                  logo.menu[0] == L"직접 시작",
              "Switching languages must retain previous string-view lifetimes.");
        Write(directory / "ko.ini", "[Logo]\nStart=\"다음 실행\"\n");
        catalog.SetLanguage(Language::Korean);
        Check(Logo(catalog).menu[0] == L"직접 시작",
              "File I/O must not occur during text lookup or language switching.");
        const TextCatalog restarted(directory);
        Check(Logo(restarted).menu[0] == L"다음 실행", "A new catalog must read replaced language files.");
        Write(directory / "ko.ini", "[Logo]\nStart=\xFF");
        const TextCatalog corrupt(directory);
        Check(Logo(corrupt).menu[0] == L"Game Start", "A corrupt Korean pack must still fall back to English.");
    }
} // namespace
int main()
{
    try
    {
        static_assert(!std::is_copy_constructible_v<TextCatalog>);
        Parser();
        const auto directory =
            std::filesystem::absolute("build/obj/LanguageFileTests/Fixtures") /
            (std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64()));
        std::filesystem::create_directories(directory);
        Catalog(directory);
        const TextCatalog shipped("Client/Texts/Languages");
        Check(shipped.Diagnostics().empty() && Logo(shipped).menu[0] == L"게임 시작",
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
