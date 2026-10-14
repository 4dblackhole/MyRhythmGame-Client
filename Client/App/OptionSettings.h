#pragma once

#include "MRG_Core.h"
#include "Texts/TextCatalog.h"

#include <filesystem>
#include <map>
#include <string>
#include <vector>

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
        std::uint32_t windowWidth{1280}, windowHeight{720};
        std::int32_t globalOffsetMs{};
        std::map<std::string, std::map<std::string, std::vector<std::uint16_t>>> modeKeys;
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
        [[nodiscard]] const std::string &LoadWarning() const noexcept { return loadWarning_; }
        // Failure publishes defaults and a warning without overwriting the original INI.
        [[nodiscard]] bool Load(std::string &error);
        [[nodiscard]] bool Save(const OptionValues &values, std::string &error);

      private:
        std::filesystem::path path_;
        OptionValues values_;
        std::string loadWarning_;
        std::uint64_t revision_{};
    };
}
