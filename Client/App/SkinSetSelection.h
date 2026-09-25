#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace mrg_client
{
    class SkinSetSelection final
    {
      public:
        [[nodiscard]] static SkinSetSelection &Instance() noexcept;

        void Initialize();
        [[nodiscard]] std::vector<std::wstring> AvailableNames() const;
        [[nodiscard]] const std::wstring &CurrentName() const noexcept;
        [[nodiscard]] std::uint64_t Revision() const noexcept;
        [[nodiscard]] bool Select(const std::wstring &name, std::string &error);
        [[nodiscard]] std::filesystem::path Resolve(const std::filesystem::path &relativePath) const;

      private:
        [[nodiscard]] static std::filesystem::path SettingsPath();
        [[nodiscard]] static std::filesystem::path SkinsRoot();

        std::wstring currentName_{L"Default Skin"};
        std::uint64_t revision_{};
    };
}
