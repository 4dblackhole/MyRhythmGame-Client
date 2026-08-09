#pragma once

#include "MRG_Core.h"

#include "Mode/PlayGameMode.h"
#include "Time/RhythmTimer.h"

#include <filesystem>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace finger_drum::audio
{
    class GameplayAudioRouter final
    {
    public:
        GameplayAudioRouter() = default;
        ~GameplayAudioRouter();

        GameplayAudioRouter(const GameplayAudioRouter&) = delete;
        GameplayAudioRouter& operator=(const GameplayAudioRouter&) = delete;

        [[nodiscard]] bool Initialize(
            mrg::audio::AudioSystem& audioSystem,
            std::string& errorMessage);
        void Shutdown() noexcept;
        [[nodiscard]] bool RegisterSound(
            rhythm::SoundId soundId,
            const std::filesystem::path& path,
            mrg::audio::AudioLoadMode loadMode,
            std::string& errorMessage);
        [[nodiscard]] bool RegisterSoundAliases(
            std::span<const rhythm::SoundId> soundIds,
            const std::filesystem::path& path,
            std::string& errorMessage);

        void Route(
            std::span<const rhythm::AudioCueRequest> cues,
            const rhythm::RhythmTimer& timer);
        void ApplyAutomation(
            std::span<const mode::AutomationValue> values);
        void Update();

        [[nodiscard]] std::string_view LastError() const noexcept;

    private:
        [[nodiscard]] mrg::audio::AudioBus* FindBus(
            std::string_view id) noexcept;
        [[nodiscard]] mrg::audio::AudioEffect* EnsureEffect(
            std::string_view busId,
            mrg::audio::AudioEffectType type);
        void ApplyAutomationValue(const mode::AutomationValue& value);

        mrg::audio::AudioSystem* audioSystem_{};
        std::map<std::string, std::unique_ptr<mrg::audio::AudioBus>, std::less<>>
            buses_;
        std::map<std::string, std::unique_ptr<mrg::audio::AudioClip>, std::less<>>
            clips_;
        std::map<std::string,
            std::map<mrg::audio::AudioEffectType,
                std::unique_ptr<mrg::audio::AudioEffect>>,
            std::less<>> effects_;
        std::vector<std::unique_ptr<mrg::audio::AudioVoice>> voices_;
        std::string lastError_;
    };
}
