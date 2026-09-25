#include "App/SkinSetSelection.h"
#include "FingerDrumAssets.h"

#include <Windows.h>
#include <ShlObj.h>

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <string>

namespace mrg_client
{
    namespace
    {
        [[nodiscard]] std::string ToUtf8(const std::wstring &value)
        {
            const int length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
            if (length <= 0) throw std::runtime_error("Invalid skin set name.");
            std::string result(static_cast<std::size_t>(length), '\0');
            WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
                static_cast<int>(value.size()), result.data(), length, nullptr, nullptr);
            return result;
        }

        [[nodiscard]] std::wstring FromUtf8(const std::string &value)
        {
            if (value.empty()) return {};
            const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                value.data(), static_cast<int>(value.size()), nullptr, 0);
            if (length <= 0) return {};
            std::wstring result(static_cast<std::size_t>(length), L'\0');
            MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                static_cast<int>(value.size()), result.data(), length);
            return result;
        }
    }

    SkinSetSelection &SkinSetSelection::Instance() noexcept
    {
        static SkinSetSelection selection;
        return selection;
    }

    std::filesystem::path SkinSetSelection::SkinsRoot()
    {
        return finger_drum::assets::UserSongsRoot().parent_path() / L"skins";
    }

    std::filesystem::path SkinSetSelection::SettingsPath()
    {
        PWSTR value = nullptr;
        const HRESULT result = SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE,
                                                     nullptr, &value);
        if (FAILED(result) || value == nullptr)
        {
            if (value != nullptr) CoTaskMemFree(value);
            throw std::runtime_error("Unable to locate local application data for skin settings.");
        }
        const std::filesystem::path path =
            std::filesystem::path(value) / L"FingerDrum" / L"skin-set.txt";
        CoTaskMemFree(value);
        return path;
    }

    std::vector<std::wstring> SkinSetSelection::AvailableNames() const
    {
        std::vector<std::wstring> names{L"Default Skin"};
        std::error_code error;
        const std::filesystem::path root = SkinsRoot();
        for (std::filesystem::directory_iterator entry(root, error), end;
             !error && entry != end; entry.increment(error))
        {
            if (!entry->is_directory(error) || entry->is_symlink(error))
            {
                error.clear();
                continue;
            }
            std::wstring name = entry->path().filename().wstring();
            if (name != L"Default Skin" && name.find_first_of(L"\r\n") == std::wstring::npos)
                names.push_back(std::move(name));
        }
        std::ranges::sort(names.begin() + 1, names.end());
        return names;
    }

    void SkinSetSelection::Initialize()
    {
        currentName_ = L"Default Skin";
        std::ifstream file(SettingsPath(), std::ios::binary);
        if (!file) return;
        std::string line;
        if (!std::getline(file, line) || line.size() > 4096) return;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const std::wstring saved = FromUtf8(line);
        const auto names = AvailableNames();
        if (std::ranges::find(names, saved) != names.end()) currentName_ = saved;
    }

    const std::wstring &SkinSetSelection::CurrentName() const noexcept
    {
        return currentName_;
    }

    std::uint64_t SkinSetSelection::Revision() const noexcept
    {
        return revision_;
    }

    bool SkinSetSelection::Select(const std::wstring &name, std::string &error)
    {
        error.clear();
        const auto names = AvailableNames();
        if (std::ranges::find(names, name) == names.end())
        {
            error = "The selected skin set folder is unavailable.";
            return false;
        }
        if (name == currentName_) return true;

        try
        {
            const std::filesystem::path path = SettingsPath();
            std::filesystem::create_directories(path.parent_path());
            std::filesystem::path temporary = path;
            temporary += L".tmp";
            const std::string encoded = ToUtf8(name);
            std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
            file.write(encoded.data(), static_cast<std::streamsize>(encoded.size()));
            file.close();
            if (!file || !MoveFileExW(temporary.c_str(), path.c_str(),
                                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            {
                std::error_code ignored;
                std::filesystem::remove(temporary, ignored);
                error = "Unable to save the selected skin set.";
                return false;
            }
        }
        catch (const std::exception &exception)
        {
            error = exception.what();
            return false;
        }
        currentName_ = name;
        ++revision_;
        return true;
    }

    std::filesystem::path SkinSetSelection::Resolve(
        const std::filesystem::path &relativePath) const
    {
        return finger_drum::assets::ResolveSkinAsset(
            SkinsRoot() / currentName_, relativePath);
    }
}
