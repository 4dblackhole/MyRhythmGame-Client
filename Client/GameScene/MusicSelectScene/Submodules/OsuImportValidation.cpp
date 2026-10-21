#include "OsuImportValidation.h"
#include "Mode/PlayGameModeFactory.h"
#include "Parsing/ChartParser.h"
#include <Windows.h>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace
{
    struct ConvertedFiles
    {
        std::filesystem::path music, pattern, effects;
        std::optional<std::size_t> sourceObjects, noteRows;
    };

    // Reads only the bundled converter's JSON v1 result. Unknown fields remain
    // extensible, but the complete document must still have valid JSON syntax.
    class ConverterResultReader final
    {
      public:
        explicit ConverterResultReader(std::string_view input) : input_(input) {}

        ConvertedFiles Read()
        {
            ConvertedFiles result;
            bool version{}, success{}, files{};
            Object([&](const std::string &key) {
                if (key == "version") version = Unsigned() == 1;
                else if (key == "success") { Literal("true"); success = true; }
                else if (key == "sourceObjects") result.sourceObjects = Unsigned();
                else if (key == "noteRows") result.noteRows = Unsigned();
                else if (key == "files")
                {
                    Object([&](const std::string &file) {
                        if (file == "ymm") result.music = Path();
                        else if (file == "ymp") result.pattern = Path();
                        else if (file == "yme") result.effects = Path();
                        else Skip(0);
                    });
                    files = true;
                }
                else Skip(0);
            });
            Whitespace();
            if (!input_.empty() || !version || !success || !files || !result.sourceObjects || !result.noteRows ||
                result.music.empty() || result.pattern.empty() || result.effects.empty()) Fail();
            return result;
        }

      private:
        [[noreturn]] static void Fail() { throw std::runtime_error("Invalid converter JSON v1 result."); }
        void Whitespace()
        {
            const auto end = input_.find_first_not_of(" \t\r\n");
            input_.remove_prefix(end == input_.npos ? input_.size() : end);
        }
        bool Take(char value)
        {
            Whitespace();
            if (input_.empty() || input_.front() != value) return false;
            input_.remove_prefix(1);
            return true;
        }
        void Require(char value) { if (!Take(value)) Fail(); }
        void Literal(std::string_view value)
        {
            Whitespace();
            if (!input_.starts_with(value)) Fail();
            input_.remove_prefix(value.size());
        }
        template <typename Field> void Object(Field field)
        {
            Require('{');
            std::unordered_set<std::string> keys;
            if (Take('}')) return;
            do
            {
                const auto key = String();
                if (!keys.insert(key).second) Fail();
                Require(':');
                field(key);
                if (Take('}')) return;
                Require(',');
            } while (true);
        }
        std::uint32_t Hex()
        {
            if (input_.size() < 4) Fail();
            std::uint32_t result{};
            const auto parsed = std::from_chars(input_.data(), input_.data() + 4, result, 16);
            if (parsed.ec != std::errc{} || parsed.ptr != input_.data() + 4) Fail();
            input_.remove_prefix(4);
            return result;
        }
        static void AppendUtf8(std::string &out, std::uint32_t code)
        {
            if (code < 0x80) out.push_back(static_cast<char>(code));
            else
            {
                const unsigned groups = code < 0x800 ? 1U : code < 0x10000 ? 2U : 3U;
                out.push_back(static_cast<char>((groups == 1 ? 0xC0U : groups == 2 ? 0xE0U : 0xF0U) |
                                                (code >> (groups * 6))));
                for (unsigned n = groups; n > 0; --n)
                    out.push_back(static_cast<char>(0x80U | ((code >> ((n - 1) * 6)) & 0x3FU)));
            }
        }
        std::string String()
        {
            Require('"');
            std::string out;
            while (!input_.empty())
            {
                const unsigned char value = static_cast<unsigned char>(input_.front());
                input_.remove_prefix(1);
                if (value == '"') return out;
                if (value < 0x20) Fail();
                if (value != '\\') { out.push_back(static_cast<char>(value)); continue; }
                if (input_.empty()) Fail();
                const auto escape = input_.front();
                input_.remove_prefix(1);
                switch (escape)
                {
                case '"': case '\\': case '/': out.push_back(escape); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'u':
                {
                    auto code = Hex();
                    if (code >= 0xD800 && code <= 0xDBFF)
                    {
                        if (!input_.starts_with("\\u")) Fail();
                        input_.remove_prefix(2);
                        const auto low = Hex();
                        if (low < 0xDC00 || low > 0xDFFF) Fail();
                        code = 0x10000 + ((code - 0xD800) << 10) + low - 0xDC00;
                    }
                    else if (code >= 0xDC00 && code <= 0xDFFF) Fail();
                    AppendUtf8(out, code);
                    break;
                }
                default: Fail();
                }
            }
            Fail();
        }
        std::filesystem::path Path()
        {
            const auto value = String();
            if (value.find('\0') != value.npos || value.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
                (!value.empty() && !MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                                                       static_cast<int>(value.size()), nullptr, 0))) Fail();
            return std::filesystem::path(std::u8string_view(reinterpret_cast<const char8_t *>(value.data()), value.size()));
        }
        std::string_view Number()
        {
            Whitespace();
            const auto end = input_.find_first_of(" \t\r\n,}]");
            const auto value = input_.substr(0, end);
            if (value.empty() || (value[0] != '-' && (value[0] < '0' || value[0] > '9'))) Fail();
            std::size_t index = value[0] == '-' ? 1 : 0;
            const auto digit = [&](std::size_t i) { return i < value.size() && value[i] >= '0' && value[i] <= '9'; };
            if (!digit(index)) Fail();
            if (value[index] == '0') ++index;
            else while (digit(index)) ++index;
            if (index < value.size() && value[index] == '.')
            {
                if (!digit(++index)) Fail();
                while (digit(index)) ++index;
            }
            if (index < value.size() && (value[index] == 'e' || value[index] == 'E'))
            {
                ++index;
                if (index < value.size() && (value[index] == '+' || value[index] == '-')) ++index;
                if (!digit(index)) Fail();
                while (digit(index)) ++index;
            }
            if (index != value.size()) Fail();
            double number{};
            const auto parsed = std::from_chars(value.data(), value.data() + value.size(), number);
            if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || !std::isfinite(number)) Fail();
            input_.remove_prefix(value.size());
            return value;
        }
        std::size_t Unsigned()
        {
            const auto value = Number();
            std::size_t result{};
            const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
            if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()) Fail();
            return result;
        }
        void Skip(unsigned depth)
        {
            if (depth >= 32) Fail();
            Whitespace();
            if (input_.empty()) Fail();
            if (input_.front() == '{') Object([&](const std::string &) { Skip(depth + 1); });
            else if (Take('['))
            {
                if (Take(']')) return;
                do { Skip(depth + 1); if (Take(']')) return; Require(','); } while (true);
            }
            else if (input_.front() == '"') (void)String();
            else if (input_.front() == 't') Literal("true");
            else if (input_.front() == 'f') Literal("false");
            else if (input_.front() == 'n') Literal("null");
            else (void)Number();
        }
        std::string_view input_;
    };

    std::filesystem::path ImportedPath(const std::filesystem::path &path, const std::filesystem::path &root)
    {
        const auto file = std::filesystem::canonical(path);
        auto parent = root.begin(), child = file.begin();
        for (; parent != root.end(); ++parent, ++child)
            if (child == file.end() || CompareStringOrdinal(parent->c_str(), -1, child->c_str(), -1, TRUE) != CSTR_EQUAL)
                throw std::runtime_error("Converted file is outside the selected Songs folder.");
        if (!std::filesystem::is_regular_file(file)) throw std::runtime_error("Converted file is missing.");
        return file;
    }
    void CheckImportDiagnostics(const std::vector<Diagnostic> &diagnostics)
    {
        if (!diagnostics.empty()) throw std::runtime_error(diagnostics.front().message);
    }
}

void ValidateConvertedOsuResult(std::string_view output, const std::filesystem::path &songsRoot)
{
    // Validate the producer result and saved references before constructing any
    // play session. All objects remain local to the import worker.
    const auto files = ConverterResultReader(output).Read();
    const auto root = std::filesystem::canonical(songsRoot);
    const auto musicFile = ImportedPath(files.music, root);
    const auto patternFile = ImportedPath(files.pattern, root);
    const auto effectFile = ImportedPath(files.effects, root);
    ChartParser parser;
    const auto music = parser.ParseMusicFile(musicFile);
    const auto pattern = parser.ParsePatternFile(patternFile);
    const auto effects = parser.ParseEffectFile(effectFile);
    CheckImportDiagnostics(music.diagnostics);
    CheckImportDiagnostics(pattern.diagnostics);
    CheckImportDiagnostics(effects.diagnostics);
    (void)ImportedPath(musicFile.parent_path() / music.document.audioFile, root);
    if (!std::filesystem::equivalent(root / pattern.document.musicMetadataFile, musicFile) ||
        !std::filesystem::equivalent(patternFile.parent_path() / pattern.document.effectFile, effectFile) ||
        pattern.document.notes.size() != *files.noteRows)
        throw std::runtime_error("Converted file references or note rows do not match the result.");

    // Gameplay compatibility belongs to the Client. A converter success alone
    // does not guarantee the selected mode accepts every source object.
    const auto mode = CreatePlayGameMode(pattern.document.mode);
    if (!mode) throw std::runtime_error("Converted pattern uses an unsupported game mode.");
    const auto session = mode->CreateSession(pattern.document, effects.document);
    CheckImportDiagnostics(session.diagnostics);
    if (!session.Succeeded()) throw std::runtime_error("Converted pattern cannot create a play session.");
    std::size_t notes{};
    for (const auto &lane : session.session->Gear().Lanes()) notes += lane->Notes().size();
    if (notes != *files.sourceObjects) throw std::runtime_error("YOUMU discarded one or more converted hit objects.");
}
