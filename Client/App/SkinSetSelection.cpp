#include "App/SkinSetSelection.h"
#include "FingerDrumAssets.h"

#include <algorithm>
#include <utility>

namespace mrg_client
{
    SkinSetSelection &SkinSetSelection::Instance() noexcept
    {
        static SkinSetSelection selection;
        return selection;
    }

    std::filesystem::path SkinSetSelection::SkinsRoot()
    {
        return finger_drum::assets::UserSongsRoot().parent_path() / L"skins";
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

    void SkinSetSelection::Initialize(const std::wstring &savedName)
    {
        const auto names = AvailableNames();
        currentName_ = std::ranges::find(names, savedName) != names.end()
            ? savedName : L"Default Skin";
        ++revision_;
    }

    const std::wstring &SkinSetSelection::CurrentName() const noexcept { return currentName_; }
    std::uint64_t SkinSetSelection::Revision() const noexcept { return revision_; }

    bool SkinSetSelection::Select(const std::wstring &name, std::string &error)
    {
        error.clear();
        const auto names = AvailableNames();
        if (std::ranges::find(names, name) == names.end())
        {
            error = "The selected skin set folder is unavailable.";
            return false;
        }
        if (name != currentName_)
        {
            currentName_ = name;
            ++revision_;
        }
        return true;
    }

    std::filesystem::path SkinSetSelection::Resolve(const std::filesystem::path &relativePath) const
    {
        return finger_drum::assets::ResolveSkinAsset(SkinsRoot() / currentName_, relativePath);
    }
}
