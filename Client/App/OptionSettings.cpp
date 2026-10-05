#include "OptionSettings.h"
#include "FingerDrumAssets.h"

#include <Windows.h>
#include <ShlObj.h>

#include <charconv>
#include <fstream>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace mrg_client
{
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

        std::string_view OutputName(const mrg::audio::AudioOutputBackend output)
        {
            using mrg::audio::AudioOutputBackend;
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

        void Validate(const OptionValues &values)
        {
            if (values.language != finger_drum::texts::Language::Korean &&
                values.language != finger_drum::texts::Language::English)
                throw std::runtime_error("Invalid language option.");
            if (values.driverIndex < -1 || !IsSingleLine(values.audioMiddleware) ||
                !IsSingleLine(values.driverName) || values.audioMiddleware.empty())
                throw std::runtime_error("Invalid audio option.");
            const auto skin = EncodeUtf8(values.skinSet);
            if (skin.empty() || !IsSingleLine(skin) || values.skinSet == L"." ||
                values.skinSet == L".." || values.skinSet.find_first_of(L"/\\") != std::wstring::npos)
                throw std::runtime_error("The skin option must name one folder.");
            static_cast<void>(OutputName(values.audioOutput));
        }

        void ReadValue(OptionValues &values, const std::string_view section,
                       const std::string_view key, const std::string_view value)
        {
            if (section == "General")
            {
                if (key == "Language")
                {
                    if (value == "ko") values.language = finger_drum::texts::Language::Korean;
                    else if (value == "en") values.language = finger_drum::texts::Language::English;
                    else throw std::runtime_error("Language must be ko or en.");
                }
                else if (key == "SkinSet") values.skinSet = DecodeUtf8(value);
            }
            else if (section == "Audio")
            {
                if (key == "Middleware") values.audioMiddleware = value;
                else if (key == "Output")
                {
                    using mrg::audio::AudioOutputBackend;
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

        void ImportLegacySkin(OptionValues &values)
        {
            PWSTR folder = nullptr;
            if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &folder)))
                return;
            const auto path = std::filesystem::path(folder) / L"FingerDrum" / L"skin-set.txt";
            CoTaskMemFree(folder);
            std::ifstream file(path, std::ios::binary);
            std::string line;
            if (std::getline(file, line) && line.size() <= 4096 && !Trim(line).empty())
            {
                try
                {
                    OptionValues candidate = values;
                    candidate.skinSet = DecodeUtf8(Trim(line));
                    Validate(candidate);
                    values = std::move(candidate);
                }
                catch (const std::exception &) { /* Invalid legacy data keeps the default. */ }
            }
        }
    }

    OptionSettings::OptionSettings(std::filesystem::path path) : path_(std::move(path)) {}

    std::filesystem::path OptionSettings::NextToExecutable()
    {
        return finger_drum::assets::UserSongsRoot().parent_path().parent_path() / L"Option.ini";
    }

    bool OptionSettings::Load(std::string &error)
    {
        error.clear();
        try
        {
            OptionValues candidate;
            if (!std::filesystem::exists(path_))
            {
                ImportLegacySkin(candidate);
                return Save(candidate, error);
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
            ++revision_;
            return true;
        }
        catch (const std::exception &exception)
        {
            error = exception.what();
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
            file << "[General]\r\nLanguage="
                 << (values.language == finger_drum::texts::Language::English ? "en" : "ko")
                 << "\r\nSkinSet=" << EncodeUtf8(values.skinSet)
                 << "\r\n\r\n[Audio]\r\nMiddleware=" << values.audioMiddleware
                 << "\r\nOutput=" << OutputName(values.audioOutput)
                 << "\r\nDriverIndex=" << values.driverIndex
                 << "\r\nDriverName=" << values.driverName << "\r\n";
            file.close();
            if (!file || !MoveFileExW(temporary.c_str(), path_.c_str(),
                                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
                throw std::runtime_error("Unable to save Option.ini beside the executable.");
            values_ = std::move(candidate);
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
}
