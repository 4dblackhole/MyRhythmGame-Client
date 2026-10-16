#include "SkinManifest.h"
#include <Windows.h>
#include <algorithm>
#include <fstream>
#include <iterator>
#include <limits>
#include <optional>
#include <string_view>
#include <utility>

namespace mrg_client
{
    std::string SkinImageKey(std::string_view scene, std::string_view key)
    {
        return std::string(scene) + "." + std::string(key);
    }
    namespace
    {
        using Entries = std::map<std::string, std::filesystem::path, std::less<>>;
        struct Manifest
        {
            Entries folders;
            Entries files;
        };
        std::wstring_view Trim(std::wstring_view value)
        {
            const auto first = value.find_first_not_of(L" \t\r");
            return first == value.npos ? std::wstring_view{}
                                       : value.substr(first, value.find_last_not_of(L" \t\r") - first + 1);
        }
        std::string Identifier(std::wstring_view value)
        {
            if (value.empty() || !std::ranges::all_of(value,
                                                      [](wchar_t c)
                                                      {
                                                          return (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') ||
                                                                 (c >= L'0' && c <= L'9') || c == L'.' || c == L'_' ||
                                                                 c == L'-';
                                                      }))
                return {};
            std::string result;
            result.reserve(value.size());
            for (const auto c : value)
                result += static_cast<char>(c); // Identifier validation guarantees ASCII.
            return result;
        }
        bool Relative(const std::filesystem::path &path)
        {
            return !path.empty() && !path.has_root_path() &&
                   path.native().find_first_of(L"\r\n\t:") == std::wstring::npos &&
                   std::ranges::none_of(path, [](const auto &part) { return part == L".."; });
        }
        void Diagnose(std::vector<std::string> &diagnostics, std::size_t line, std::string message)
        {
            diagnostics.push_back("skin.ini:" + std::to_string(line) + ": " + std::move(message));
        }
        std::optional<std::wstring> Decode(std::string bytes, std::vector<std::string> &diagnostics)
        {
            if (bytes.starts_with("\xEF\xBB\xBF"))
                bytes.erase(0, 3);
            if (bytes.empty())
                return std::wstring{};
            if (bytes.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)()) ||
                bytes.find('\0') != bytes.npos)
            {
                Diagnose(diagnostics, 0, "Expected UTF-8 without NUL.");
                return {};
            }
            const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(),
                                                  static_cast<int>(bytes.size()), nullptr, 0);
            if (!count)
            {
                Diagnose(diagnostics, 0, "Expected valid UTF-8.");
                return {};
            }
            std::wstring decoded(static_cast<std::size_t>(count), L'\0');
            if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), static_cast<int>(bytes.size()),
                                    decoded.data(), count) != count)
            {
                Diagnose(diagnostics, 0, "Could not decode UTF-8.");
                return {};
            }
            return decoded;
        }
        Manifest Parse(std::wstring_view source, std::vector<std::string> &diagnostics)
        {
            Manifest manifest;
            std::string section;
            std::size_t lineNumber{};
            while (!source.empty())
            {
                const auto end = source.find(L'\n');
                const auto line = Trim(source.substr(0, end));
                source = end == source.npos ? std::wstring_view{} : source.substr(end + 1);
                ++lineNumber;
                if (line.empty() || line.starts_with(L';') || line.starts_with(L'#'))
                    continue;
                if (line.starts_with(L'['))
                {
                    section = line.ends_with(L']') ? Identifier(Trim(line.substr(1, line.size() - 2))) : "";
                    if (section != "Folders" && section != "Logo" && section != "InGame" && section != "MusicSelect" &&
                        section != "Result")
                    {
                        Diagnose(diagnostics, lineNumber, "Unknown scene section.");
                        section.clear();
                    }
                    continue;
                }
                const auto equals = line.find(L'=');
                const auto key = Identifier(Trim(line.substr(0, equals)));
                if (section.empty() || equals == line.npos || key.empty())
                {
                    Diagnose(diagnostics, lineNumber, "Expected a scene section and key=relative/path.");
                    continue;
                }
                auto value = Trim(line.substr(equals + 1));
                // Quoting preserves spaces in filenames. Backslashes are literal
                // path separators, not language-file escape sequences.
                if (value.starts_with(L'"') && value.size() >= 2 && value.ends_with(L'"'))
                    value = value.substr(1, value.size() - 2);
                const std::filesystem::path relative{value};
                if (!value.empty() && (!Relative(relative) || value.find(L'"') != value.npos))
                {
                    Diagnose(diagnostics, lineNumber, "Paths must stay relative without parent traversal.");
                    continue;
                }
                auto &entries = section == "Folders" ? manifest.folders : manifest.files;
                const auto identity = section == "Folders" ? key : section + "." + key;
                if (!entries.emplace(identity, relative).second)
                    Diagnose(diagnostics, lineNumber, "Duplicate image key: " + identity);
            }
            return manifest;
        }
        Manifest Read(const std::filesystem::path &root, std::vector<std::string> &diagnostics)
        {
            const auto path = root / L"skin.ini";
            std::error_code error;
            if (!std::filesystem::exists(path, error) && !error)
                return {};
            std::ifstream input(path, std::ios::binary);
            std::string bytes{std::istreambuf_iterator<char>{input}, {}};
            if (!input || input.bad())
            {
                Diagnose(diagnostics, 0, "Could not read the skin manifest.");
                return {};
            }
            const auto decoded = Decode(std::move(bytes), diagnostics);
            return decoded ? Parse(*decoded, diagnostics) : Manifest{};
        }
        std::filesystem::path Entry(const Entries &entries, const std::string &key, const Entries &fallback)
        {
            const auto found = entries.find(key);
            if (found != entries.end() && !found->second.empty())
                return found->second;
            const auto base = fallback.find(key);
            return base == fallback.end() ? std::filesystem::path{} : base->second;
        }
        bool FileExists(const std::filesystem::path &path)
        {
            std::error_code error;
            return std::filesystem::is_regular_file(path, error) && !error;
        }
    } // namespace
    SkinImages LoadSkinImages(const std::filesystem::path &skinRoot, const std::filesystem::path &defaultSkinRoot)
    {
        SkinImages images;
        const auto defaults = Read(defaultSkinRoot, images.diagnostics);
        const auto selected = Read(skinRoot, images.diagnostics);
        Entries keys = defaults.files;
        keys.insert(selected.files.begin(), selected.files.end());
        for (const auto &entry : keys)
        {
            const auto &key = entry.first;
            const auto scene = key.substr(0, key.find('.'));
            const auto folder = Entry(selected.folders, scene, defaults.folders);
            const auto file = Entry(selected.files, key, defaults.files);
            std::filesystem::path resolved;
            if (!folder.empty() && !file.empty())
            {
                const auto candidate = skinRoot / folder / file;
                if (FileExists(candidate))
                    resolved = candidate.lexically_normal();
                else if (selected.files.contains(key) && !selected.files.at(key).empty())
                    images.diagnostics.push_back(key + ": Image file is unavailable; using its default.");
            }
            if (resolved.empty())
            {
                const auto defaultFolder = Entry(defaults.folders, scene, {});
                const auto defaultFile = Entry(defaults.files, key, {});
                const auto candidate = defaultSkinRoot / defaultFolder / defaultFile;
                if (!defaultFolder.empty() && !defaultFile.empty() && FileExists(candidate))
                    resolved = candidate.lexically_normal();
            }
            images.paths.emplace(key, std::move(resolved));
        }
        return images;
    }
} // namespace mrg_client
