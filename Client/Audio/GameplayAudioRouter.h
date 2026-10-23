#pragma once

#include "PhantomCore.h"

#include "Mode/PlayGameMode.h"
#include "Time/RhythmTimer.h"

#include <filesystem>
#include <map>
#include <memory>
#include <span>
#include <set>
#include <string>
#include <string_view>
#include <vector>

class GameplayAudioRouter final
{
public:
    explicit GameplayAudioRouter(
        AudioPlaybackManager& playback) noexcept;
    ~GameplayAudioRouter();

    GameplayAudioRouter(const GameplayAudioRouter&) = delete;
    GameplayAudioRouter& operator=(const GameplayAudioRouter&) = delete;

    [[nodiscard]] bool Initialize(
        AudioSystem& audioSystem,
        std::string& errorMessage);
    void Shutdown() noexcept;
    [[nodiscard]] bool RegisterSound(
        SoundId soundId,
        const std::filesystem::path& path,
        AudioLoadMode loadMode,
        std::string& errorMessage, bool restartWhilePlaying = true);
    [[nodiscard]] bool RegisterSoundAliases(std::span<const SoundId> soundIds,
                                            const std::filesystem::path &path, std::string &errorMessage,
                                            AudioLoadMode loadMode = AudioLoadMode::Sample);
    AudioPreparationState PrepareResources();
    bool ConfigurePlayback(const PlayOptions &options, std::string &error);

    // Input-driven cues start immediately. Timeline scheduling remains a
    // separate path for music and other deliberately future playback.
    void PlayNow(std::span<const AudioCueRequest> cues);
    void Schedule(
        std::span<const AudioCueRequest> cues,
        const RhythmTimer& timer);
    void ApplyAutomation(
        std::span<const AutomationValue> values);
    void PrepareAutomation(std::span<const CompiledEffectCommand> commands);
    void StopAllVoices() noexcept;
    // Preserve a live music voice; transient samples must never resume.
    void PauseMusicAndStopSamples();
    AudioPreparationState PrepareMusicAt(RhythmTime position);
    bool StartPreparedMusic(RhythmTime position, const RhythmTimer &timer);
    void ResetAutomation();
    void Update();

    [[nodiscard]] std::string_view LastError() const noexcept;

private:
    struct RegisteredSound
    {
        std::shared_ptr<AudioClip> clip;
        bool restartWhilePlaying{};
    };

    void Route(
        std::span<const AudioCueRequest> cues,
        const RhythmTimer* timer);
    [[nodiscard]] AudioBus* FindBus(
        std::string_view id) noexcept;
    [[nodiscard]] AudioEffect* EnsureEffect(
        std::string_view busId,
        AudioEffectType type);
    void ApplyAutomationValue(const AutomationValue& value);

    float musicPitch_{1.0F}, hitSoundPitch_{1.0F};
    AudioSystem* audioSystem_{};
    AudioPlaybackManager& playback_;
    std::map<std::string, std::shared_ptr<AudioBus>, std::less<>>
        buses_;
    std::map<std::string, RegisteredSound, std::less<>> clips_;
    std::map<std::string,
        std::map<AudioEffectType,
            std::unique_ptr<AudioEffect>>,
        std::less<>> effects_;
    // Only IDs for this play session. Voice ownership belongs to the Client.
    std::vector<AudioPlaybackId> voices_;
    AudioPlaybackId musicVoice_{InvalidAudioPlaybackId};
    bool musicPositioned_{};
    bool musicPastEnd_{};
    // Aliased SoundIds share this entry by their common AudioClip address.
    // Sample playback therefore has at most one active native channel per
    // loaded sound resource.
    std::map<const AudioClip*, AudioPlaybackId>
        sampleVoices_;
    std::string lastError_;
    std::set<std::pair<std::string, AudioEffectType>> bypassedEffects_;
    std::map<std::pair<EffectCommandType, std::string>, double> appliedValues_;
};
