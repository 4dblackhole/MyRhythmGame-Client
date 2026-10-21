#include "LanguageFile.h"
#include <Windows.h>
#include <algorithm>
#include <fstream>
#include <iterator>
#include <limits>
#include <optional>
#include <stdexcept>

namespace
{
    std::string_view Trim(std::string_view value)
    {
        const auto first = value.find_first_not_of(" \t\r");
        if (first == value.npos)
            return {};
        return value.substr(first, value.find_last_not_of(" \t\r") - first + 1);
    }
    bool Identifier(std::string_view value)
    {
        return !value.empty() && std::ranges::all_of(value,
                                                     [](unsigned char c)
                                                     {
                                                         return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                                                                (c >= '0' && c <= '9') || c == '.' || c == '_' ||
                                                                c == '-';
                                                     });
    }
    std::wstring Decode(std::string_view value)
    {
        if (value.empty())
            return {};
        if (value.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)()))
            throw std::runtime_error("Language file is too large to decode.");
        const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                                              static_cast<int>(value.size()), nullptr, 0);
        if (count <= 0 || value.find('\0') != value.npos)
            throw std::runtime_error("Language file must contain valid UTF-8 without NUL.");
        std::wstring result(static_cast<std::size_t>(count), L'\0');
        if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
                                result.data(), count) != count)
            throw std::runtime_error("Could not decode the language file.");
        return result;
    }
    std::wstring Value(std::string_view value)
    {
        value = Trim(value);
        if (!value.starts_with('"'))
            return Decode(value);
        if (value.size() < 2 || !value.ends_with('"'))
            throw std::runtime_error("Quoted value is not closed.");
        value = value.substr(1, value.size() - 2);
        std::string decoded;
        for (std::size_t i = 0; i < value.size(); ++i)
        {
            if (value[i] == '"')
                throw std::runtime_error("Quote inside a value must be escaped.");
            if (value[i] != '\\')
                decoded += value[i];
            else
            {
                if (++i == value.size())
                    throw std::runtime_error("Incomplete escape in a quoted value.");
                switch (value[i])
                {
                case 'n':
                    decoded += '\n';
                    break;
                case 'r':
                    decoded += '\r';
                    break;
                case 't':
                    decoded += '\t';
                    break;
                case '\\':
                    decoded += '\\';
                    break;
                case '"':
                    decoded += '"';
                    break;
                default:
                    throw std::runtime_error("Unknown escape in a quoted value.");
                }
            }
        }
        return Decode(decoded);
    }
    using Fields = std::vector<std::pair<std::size_t, std::wstring>>;
    std::optional<Fields> FormatFields(std::wstring_view value)
    {
        Fields fields;
        std::size_t automatic{};
        bool hasAutomatic = false, hasExplicit = false;
        for (std::size_t i = 0; i < value.size(); ++i)
        {
            if (value[i] != L'{' && value[i] != L'}')
                continue;
            if (i + 1 < value.size() && value[i + 1] == value[i])
            {
                ++i;
                continue;
            }
            if (value[i] == L'}')
                return {};
            const auto end = value.find(L'}', i + 1);
            if (end == value.npos)
                return {};
            const auto field = value.substr(i + 1, end - i - 1);
            if (field.find(L'{') != field.npos)
                return {};
            const auto colon = field.find(L':');
            const auto number = field.substr(0, colon);
            std::size_t index{};
            if (number.empty())
            {
                hasAutomatic = true;
                index = automatic++;
            }
            else
            {
                hasExplicit = true;
                for (const auto c : number)
                {
                    if (c < L'0' || c > L'9' ||
                        index > ((std::numeric_limits<std::size_t>::max)() - (c - L'0')) / 10)
                        return {};
                    index = index * 10 + static_cast<std::size_t>(c - L'0');
                }
            }
            if (hasAutomatic && hasExplicit)
                return {};
            fields.emplace_back(index, colon == field.npos ? L"" : std::wstring(field.substr(colon)));
            i = end;
        }
        std::ranges::sort(fields);
        return fields;
    }
} // namespace

LanguageFile ParseLanguageFile(std::string_view source, std::string_view name)
{
    LanguageFile file;
    if (source.starts_with("\xEF\xBB\xBF"))
        source.remove_prefix(3);
    // Encoding errors invalidate the file, while syntax/value errors only
    // discard their own entry. Each remaining key can still fall back.
    try
    {
        static_cast<void>(Decode(source));
    }
    catch (const std::runtime_error &error)
    {
        file.diagnostics.emplace_back(std::string(name) + ": " + error.what());
        return file;
    }
    std::string section;
    std::size_t lineNumber{};
    while (!source.empty())
    {
        const auto newline = source.find('\n');
        const auto line = Trim(source.substr(0, newline));
        source = newline == source.npos ? std::string_view{} : source.substr(newline + 1);
        ++lineNumber;
        if (line.empty() || line.starts_with(';') || line.starts_with('#'))
            continue;
        try
        {
            if (line.starts_with('['))
            {
                section.clear();
                if (!line.ends_with(']') || !Identifier(Trim(line.substr(1, line.size() - 2))))
                    throw std::runtime_error("Invalid INI section.");
                section = Trim(line.substr(1, line.size() - 2));
                continue;
            }
            const auto equals = line.find('=');
            const auto key = Trim(line.substr(0, equals));
            if (section.empty() || equals == line.npos || !Identifier(key))
                throw std::runtime_error("Expected a section and key=value.");
            const auto identity = section + "." + std::string(key);
            if (!file.entries.emplace(identity, Value(line.substr(equals + 1))).second)
                throw std::runtime_error("Duplicate translation key: " + identity);
        }
        catch (const std::runtime_error &error)
        {
            file.diagnostics.emplace_back(std::string(name) + ":" + std::to_string(lineNumber) + ": " +
                                          error.what());
        }
    }
    return file;
}
LanguageFile ReadLanguageFile(const std::filesystem::path &path)
{
    const auto utf8Path = path.generic_u8string();
    const std::string name(reinterpret_cast<const char *>(utf8Path.data()), utf8Path.size());
    std::error_code error;
    if (!std::filesystem::exists(path, error) && !error)
        return {};
    std::ifstream input(path, std::ios::binary);
    if (!input)
        return {{}, {"Could not open language file: " + name}};
    const std::string source{std::istreambuf_iterator<char>{input}, {}};
    if (input.bad())
        return {{}, {"Could not read language file: " + name}};
    return ParseLanguageFile(source, name);
}
void ValidateTranslations(LanguageFile &file, const TranslationEntries &english)
{
    for (auto entry = file.entries.begin(); entry != file.entries.end();)
    {
        const auto fallback = english.find(entry->first);
        const auto fields = FormatFields(entry->second);
        if (fallback != english.end() && fields && fields == FormatFields(fallback->second))
            ++entry;
        else
        {
            file.diagnostics.emplace_back("Unknown key or incompatible format placeholders: " + entry->first);
            entry = file.entries.erase(entry);
        }
    }
}
