#pragma once

#include "MRG_Core.h"
#include "Texts/TextCatalog.h"

#include <filesystem>
#include <string>

namespace mrg_client
{
    struct OptionValues
    {
        finger_drum::texts::Language language{finger_drum::texts::Language::Korean};
        std::wstring skinSet{L"Default Skin"};
        std::string audioMiddleware{"FMOD"};
        mrg::audio::AudioOutputBackend audioOutput{mrg::audio::AudioOutputBackend::Automatic};
        int driverIndex{-1};
        std::string driverName;
    };

    // Owns the persistent values, independently of the panel and audio handles.
    // A successful atomic file replacement is required before publishing a change.
    class OptionSettings final
    {
      public:
        explicit OptionSettings(std::filesystem::path path);
        [[nodiscard]] static std::filesystem::path NextToExecutable();
        [[nodiscard]] const std::filesystem::path &Path() const noexcept { return path_; }
        [[nodiscard]] const OptionValues &Values() const noexcept { return values_; }
        [[nodiscard]] std::uint64_t Revision() const noexcept { return revision_; }
        [[nodiscard]] bool Load(std::string &error);
        [[nodiscard]] bool Save(const OptionValues &values, std::string &error);

      private:
        std::filesystem::path path_;
        OptionValues values_;
        std::uint64_t revision_{};
    };
}
