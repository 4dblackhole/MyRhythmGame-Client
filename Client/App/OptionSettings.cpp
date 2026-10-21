#include "OptionSettings.h"
#include "YOUMUAssets.h"
#include "Mode/PlayGameModeFactory.h"
#include <algorithm>

#include <Windows.h>

#include <charconv>
#include <fstream>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace
{
    std::string_view Trim(std::string_view value)
    {
        const auto begin = value.find_first_not_of(" \t\r\n");
        if (begin == std::string_view::npos) return {};
        const auto end = value.find_last_not_of(" \t\r\n");
        return value.substr(begin, end - begin + 1);
    }

    std::wstring DecodeUtf8(const std::string_view value)
    {
        if (value.empty()) return {};
        const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
            value.data(), static_cast<int>(value.size()), nullptr, 0);
        if (length <= 0) throw std::runtime_error("Option.ini contains invalid UTF-8.");
        std::wstring result(static_cast<std::size_t>(length), L'\0');
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
            static_cast<int>(value.size()), result.data(), length);
        return result;
    }

    std::string EncodeUtf8(const std::wstring &value)
    {
        if (value.empty()) return {};
        const int length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
            value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
        if (length <= 0) throw std::runtime_error("The option value is invalid Unicode.");
        std::string result(static_cast<std::size_t>(length), '\0');
        WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
            static_cast<int>(value.size()), result.data(), length, nullptr, nullptr);
        return result;
    }

    std::string_view OutputName(const AudioOutputBackend output)
    {

        switch (output)
        {
        case AudioOutputBackend::Automatic: return "Automatic";
        case AudioOutputBackend::Wasapi: return "WASAPI";
        case AudioOutputBackend::Asio: return "ASIO";
        default: throw std::runtime_error("Invalid audio output option.");
        }
    }

    bool IsSingleLine(const std::string_view value)
    {
        return value.size() <= 4096 && value.find_first_of("\r\n\0", 0, 3) ==
            std::string_view::npos;
    }

    template <class Integer> Integer ReadInteger(std::string_view value)
    {
        if (value.starts_with('+'))
            value.remove_prefix(1);
        Integer number{};
        const auto parsed = std::from_chars(value.data(), value.data() + value.size(), number);
        if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size())
            throw std::runtime_error("The option must be an integer within its valid range.");
        return number;
    }

    bool IsIdentifier(std::string_view value)
    {
        return !value.empty() && value.size() <= 128 &&
               std::ranges::all_of(value,
                                   [](char c)
                                   {
                                       return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                                              (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.';
                                   });
    }

    void Validate(const OptionValues &values)
    {
        if (values.language != Language::Korean &&
            values.language != Language::English)
            throw std::runtime_error("Invalid language option.");
        if (values.driverIndex < -1 || !IsSingleLine(values.audioMiddleware) ||
            !IsSingleLine(values.driverName) || values.audioMiddleware.empty())
            throw std::runtime_error("Invalid audio option.");
        const auto skin = EncodeUtf8(values.skinSet);
        if (!IsSingleLine(EncodeUtf8(values.songsPath.wstring())))
            throw std::runtime_error("Songs path must fit a single UTF-8 INI line.");
        if (!values.songsPath.is_absolute() && values.songsPath.has_root_path())
            throw std::runtime_error("Songs path must be fully absolute or relative to the executable directory.");
        if (skin.empty() || !IsSingleLine(skin) || values.skinSet == L"." ||
            values.skinSet == L".." || values.skinSet.find_first_of(L"/\\") != std::wstring::npos)
            throw std::runtime_error("The skin option must name one folder.");
        if (values.audioMiddleware != "FMOD")
            throw std::runtime_error("Only the FMOD audio middleware is installed.");
        static_cast<void>(OutputName(values.audioOutput));
        if (!values.windowWidth || !values.windowHeight || values.windowWidth > 16384 ||
            values.windowHeight > 16384)
            throw std::runtime_error("Window dimensions must be within 1..16384 pixels.");
        for (const auto &[mode, bindings] : values.modeKeys)
        {
            if (!IsIdentifier(mode))
                throw std::runtime_error("Invalid mode identifier.");
            for (const auto &[id, keys] : bindings)
            {
                if (!IsIdentifier(id) || keys.empty() ||
                    std::ranges::any_of(keys, [](auto key) { return key < 8 || key > 254; }))
                    throw std::runtime_error("Invalid mode input keys.");
            }
        }
        for (auto layout : AvailableInputLayouts())
            if (const auto saved = values.modeKeys.find(layout.modeId); saved != values.modeKeys.end())
                static_cast<void>(ResolveInputLayout(std::move(layout), saved->second));
    }

    void ReadValue(OptionValues &values, const std::string_view section,
                   const std::string_view key, const std::string_view value)
    {
        if (section == "General")
        {
            if (key == "Language")
            {
                if (value == "ko") values.language = Language::Korean;
                else if (value == "en") values.language = Language::English;
                else throw std::runtime_error("Language must be ko or en.");
            }
            else if (key == "SkinSet") values.skinSet = DecodeUtf8(value);
            else if (key == "GlobalOffsetMs")
                values.globalOffsetMs = ReadInteger<std::int32_t>(value);
            else if (key == "SongsPath")
                values.songsPath = std::filesystem::path(DecodeUtf8(value));
        }
        else if (section == "Display")
        {
            if (key == "Width")
                values.windowWidth = ReadInteger<std::uint32_t>(value);
            else if (key == "Height")
                values.windowHeight = ReadInteger<std::uint32_t>(value);
        }
        else if (section.starts_with("Keys."))
        {
            std::vector<std::uint16_t> keys;
            auto remaining = value;
            for (;;)
            {
                const auto comma = remaining.find(',');
                keys.push_back(ReadInteger<std::uint16_t>(Trim(remaining.substr(0, comma))));
                if (comma == std::string_view::npos)
                    break;
                remaining.remove_prefix(comma + 1);
            }
            values.modeKeys[std::string(section.substr(5))][std::string(key)] = std::move(keys);
        }
        else if (section == "Audio")
        {
            if (key == "Middleware") values.audioMiddleware = value;
            else if (key == "Output")
            {

                if (value == "Automatic") values.audioOutput = AudioOutputBackend::Automatic;
                else if (value == "WASAPI") values.audioOutput = AudioOutputBackend::Wasapi;
                else if (value == "ASIO") values.audioOutput = AudioOutputBackend::Asio;
                else throw std::runtime_error("Output must be Automatic, WASAPI or ASIO.");
            }
            else if (key == "DriverIndex")
            {
                const auto result = std::from_chars(value.data(), value.data() + value.size(),
                                                    values.driverIndex);
                if (result.ec != std::errc{} || result.ptr != value.data() + value.size())
                    throw std::runtime_error("DriverIndex must be an integer.");
            }
            else if (key == "DriverName") values.driverName = value;
        }
    }

}

OptionSettings::OptionSettings(std::filesystem::path path) : path_(std::move(path)) {}

std::filesystem::path OptionSettings::SongsRoot() const
{
    if (values_.songsPath.empty())
        return UserSongsRoot();
    return (values_.songsPath.is_absolute() ? values_.songsPath
                                            : AssetExecutableDirectory() / values_.songsPath)
        .lexically_normal();
}

std::filesystem::path OptionSettings::NextToExecutable()
{
    return AssetExecutableDirectory() / L"Option.ini";
}

bool OptionSettings::Load(std::string &error)
{
    error.clear();
    try
    {
        OptionValues candidate;
        if (!std::filesystem::exists(path_))
        {
            if (!Save(candidate, error))
                throw std::runtime_error(error);
            return true;
        }
        std::ifstream file(path_, std::ios::binary);
        if (!file) throw std::runtime_error("Unable to read Option.ini.");
        std::string section, line;
        bool firstLine = true;
        while (std::getline(file, line))
        {
            if (line.size() > 8192) throw std::runtime_error("Option.ini line is too long.");
            std::string_view text = line;
            if (firstLine && text.starts_with("\xEF\xBB\xBF")) text.remove_prefix(3);
            firstLine = false;
            text = Trim(text);
            if (text.empty() || text.front() == ';' || text.front() == '#') continue;
            if (text.front() == '[' && text.back() == ']')
            {
                section = Trim(text.substr(1, text.size() - 2));
                continue;
            }
            const auto separator = text.find('=');
            if (separator == std::string_view::npos)
                throw std::runtime_error("Option.ini entry is missing '='.");
            ReadValue(candidate, section, Trim(text.substr(0, separator)),
                      Trim(text.substr(separator + 1)));
        }
        if (!file.eof()) throw std::runtime_error("Unable to finish reading Option.ini.");
        Validate(candidate);
        values_ = std::move(candidate);
        loadWarning_.clear();
        ++revision_;
        return true;
    }
    catch (const std::exception &exception)
    {
        error = exception.what();
        loadWarning_ = error;
        values_ = OptionValues{};
        ++revision_;
        return false;
    }
}

bool OptionSettings::Save(const OptionValues &values, std::string &error)
{
    error.clear();
    auto temporary = path_;
    temporary += L".tmp";
    try
    {
        Validate(values);
        // Allocate the publication candidate before replacing the persisted file.
        OptionValues candidate = values;
        std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
        file << "[General]\r\nLanguage=" << (values.language == Language::English ? "en" : "ko")
             << "\r\nSkinSet=" << EncodeUtf8(values.skinSet) << "\r\nGlobalOffsetMs=" << values.globalOffsetMs
             << "\r\nSongsPath=" << EncodeUtf8(values.songsPath.wstring())
             << "\r\n\r\n[Display]\r\nWidth=" << values.windowWidth << "\r\nHeight=" << values.windowHeight
             << "\r\n\r\n[Audio]\r\nMiddleware=" << values.audioMiddleware
             << "\r\nOutput=" << OutputName(values.audioOutput) << "\r\nDriverIndex=" << values.driverIndex
             << "\r\nDriverName=" << values.driverName << "\r\n";
        for (const auto &[mode, bindings] : values.modeKeys)
        {
            file << "\r\n[Keys." << mode << "]\r\n";
            for (const auto &[id, keys] : bindings)
            {
                file << id << '=';
                for (std::size_t i = 0; i < keys.size(); ++i)
                    file << (i ? "," : "") << keys[i];
                file << "\r\n";
            }
        }
        file.close();
        if (!file || !MoveFileExW(temporary.c_str(), path_.c_str(),
                                 MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Unable to save Option.ini beside the executable.");
        values_ = std::move(candidate);
        loadWarning_.clear();
        ++revision_;
        return true;
    }
    catch (const std::exception &exception)
    {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        error = exception.what();
        return false;
    }
}
