#include "App/SkinSetSelection.h"
#include "FingerDrumAssets.h"

#include <Windows.h>
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
        return finger_drum::assets::UserAssetRoot() / L"Skins";
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
        images_.reset();
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
            images_.reset();
        }
        return true;
    }

    std::filesystem::path SkinSetSelection::Resolve(const std::filesystem::path &relativePath) const
    {
        return finger_drum::assets::ResolveSkinAsset(SkinsRoot() / currentName_, relativePath);
    }

    std::filesystem::path SkinSetSelection::Image(std::string_view scene, std::string_view key) const
    {
        if (!images_)
        {
            const auto defaults = finger_drum::assets::BuiltInAssetRoot() / L"assets/skins/Default Skin";
            images_ = LoadSkinImages(SkinsRoot() / currentName_, defaults);
            for (const auto &diagnostic : images_->diagnostics)
                OutputDebugStringA((diagnostic + "\n").c_str());
        }
        const auto found = images_->paths.find(SkinImageKey(scene, key));
        return found == images_->paths.end() ? std::filesystem::path{} : found->second;
    }
}
