#pragma once

#include "App/SkinManifest.h"
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace mrg_client
{
    class SkinSetSelection final
    {
      public:
        [[nodiscard]] static SkinSetSelection &Instance() noexcept;

        void Initialize(const std::wstring &savedName = L"Default Skin");
        [[nodiscard]] std::vector<std::wstring> AvailableNames() const;
        [[nodiscard]] const std::wstring &CurrentName() const noexcept;
        [[nodiscard]] std::uint64_t Revision() const noexcept;
        [[nodiscard]] bool Select(const std::wstring &name, std::string &error);
        [[nodiscard]] std::filesystem::path Resolve(const std::filesystem::path &relativePath) const;
        [[nodiscard]] std::filesystem::path Image(std::string_view scene, std::string_view key) const;

      private:
        [[nodiscard]] static std::filesystem::path SkinsRoot();

        std::wstring currentName_{L"Default Skin"};
        std::uint64_t revision_{};
        mutable std::optional<SkinImages> images_;
    };
}
