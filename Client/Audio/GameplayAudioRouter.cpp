#include "GameplayAudioRouter.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace finger_drum::audio
{
    GameplayAudioRouter::GameplayAudioRouter(
        mrg::audio::AudioPlaybackManager& playback) noexcept
        : playback_(playback)
    {
    }

    GameplayAudioRouter::~GameplayAudioRouter()
    {
        Shutdown();
    }

    bool GameplayAudioRouter::Initialize(
        mrg::audio::AudioSystem& audioSystem,
        std::string& errorMessage)
    {
        Shutdown();
        audioSystem_ = &audioSystem;

        constexpr std::array<std::string_view, 5> BusNames{
            "Music",
            "HitSound",
            "TickSound",
            "UserInputFeedback",
            "UI"};
        for (const std::string_view name : BusNames)
        {
            std::unique_ptr<mrg::audio::AudioBus> bus =
                audioSystem.CreateBus(name, nullptr, errorMessage);
            if (bus == nullptr)
            {
                Shutdown();
                return false;
            }
            buses_.emplace(std::string(name), std::move(bus));
        }

        errorMessage.clear();
        lastError_.clear();
        return true;
    }

    void GameplayAudioRouter::Shutdown() noexcept
    {
        StopAllVoices();
        effects_.clear();
        bypassedEffects_.clear();
        appliedValues_.clear();
        if (audioSystem_)
            for (auto &[id, sound] : clips_)
            {
                static_cast<void>(id);
                // Aliases share one clip. Retire only the last owned alias.
                if (sound.clip.use_count() > 1)
                {
                    sound.clip.reset();
                    continue;
                }
                try
                {
                    audioSystem_->RetireClip(std::move(sound.clip));
                }
                catch (...)
                { /* Shutdown releases remaining handles while the audio service is alive. */
                }
            }
        clips_.clear();
        buses_.clear();
        audioSystem_ = nullptr;
        lastError_.clear();
    }

    bool GameplayAudioRouter::RegisterSound(
        rhythm::SoundId soundId,
        const std::filesystem::path& path,
        const mrg::audio::AudioLoadMode loadMode,
        std::string& errorMessage)
    {
        if (audioSystem_ == nullptr)
        {
            errorMessage = "The gameplay audio router is not initialized.";
            return false;
        }
        if (soundId.empty())
        {
            errorMessage = "A gameplay sound ID cannot be empty.";
            return false;
        }

        std::unique_ptr<mrg::audio::AudioClip> clip =
            audioSystem_->LoadSound(path, loadMode, errorMessage);
        if (clip == nullptr)
        {
            return false;
        }
        clips_.insert_or_assign(
            std::move(soundId),
            RegisteredSound{std::move(clip), loadMode == mrg::audio::AudioLoadMode::Sample ||
                                                 loadMode == mrg::audio::AudioLoadMode::SampleAsync});
        errorMessage.clear();
        return true;
    }

    bool GameplayAudioRouter::RegisterSoundAliases(const std::span<const rhythm::SoundId> soundIds,
                                                   const std::filesystem::path &path, std::string &errorMessage,
                                                   const mrg::audio::AudioLoadMode loadMode)
    {
        if (audioSystem_ == nullptr)
        {
            errorMessage = "The gameplay audio router is not initialized.";
            return false;
        }
        if (soundIds.empty() ||
            std::ranges::any_of(
                soundIds,
                [](const rhythm::SoundId& soundId)
                {
                    return soundId.empty();
                }))
        {
            errorMessage = "Gameplay sound aliases require non-empty IDs.";
            return false;
        }

        std::unique_ptr<mrg::audio::AudioClip> loaded = audioSystem_->LoadSound(path, loadMode, errorMessage);
        if (loaded == nullptr)
        {
            return false;
        }
        std::shared_ptr<mrg::audio::AudioClip> clip = std::move(loaded);
        for (const rhythm::SoundId& soundId : soundIds)
        {
            clips_.insert_or_assign(
                soundId,
                RegisteredSound{clip, loadMode == mrg::audio::AudioLoadMode::Sample ||
                    loadMode == mrg::audio::AudioLoadMode::SampleAsync});
        }
        errorMessage.clear();
        return true;
    }

    bool GameplayAudioRouter::Ready()
    {
        bool ready = true;
        for (const auto &[id, sound] : clips_)
        {
            std::string error;
            const auto state = sound.clip->LoadState(error);
            if (state == mrg::audio::AudioClipLoadState::Loading)
                ready = false;
            else if (state == mrg::audio::AudioClipLoadState::Failed && lastError_.empty())
                lastError_ = id + ": " + error;
        }
        return ready;
    }

    void GameplayAudioRouter::PlayNow(
        const std::span<const rhythm::AudioCueRequest> cues)
    {
        Route(cues, nullptr);
    }
    GameplayAudioRouter::MusicPreparation GameplayAudioRouter::PrepareMusicAt(rhythm::RhythmTime position)
    {
        const auto found = clips_.find("Music.Track");
        if (found == clips_.end())
        {
            lastError_ = "No music clip is registered.";
            return MusicPreparation::Failed;
        }
        std::string error;
        const auto state = found->second.clip->LoadState(error);
        if (state == mrg::audio::AudioClipLoadState::Loading)
            return MusicPreparation::Loading;
        if (state == mrg::audio::AudioClipLoadState::Failed)
        {
            lastError_ = std::move(error);
            return MusicPreparation::Failed;
        }
        if (musicVoice_ != mrg::audio::InvalidAudioPlaybackId)
        {
            if (!musicPositioned_ && position > rhythm::RhythmTime{})
            {
                mrg::audio::AudioPlaybackSettings settings;
                settings.startPaused = true;
                settings.startPositionMicroseconds = static_cast<std::uint64_t>(position.count());
                const auto bus = buses_.find("Music");
                if (!playback_.Restart(musicVoice_, settings, bus == buses_.end() ? nullptr : bus->second, error))
                {
                    lastError_ = std::move(error);
                    return MusicPreparation::Failed;
                }
                musicPositioned_ = true;
                return MusicPreparation::Loading; // Wait for this source seek as well.
            }
            return MusicPreparation::Ready;
        }
        std::uint64_t duration{};
        if (!found->second.clip->GetDurationMicroseconds(duration, error))
        {
            lastError_ = std::move(error);
            return MusicPreparation::Failed;
        }
        const auto sourcePosition = static_cast<std::uint64_t>(std::max(position.count(), rhythm::RhythmTime::rep{}));
        if (sourcePosition >= duration)
            return MusicPreparation::Ready; // The chart may extend beyond the music file.
        mrg::audio::AudioPlaybackSettings settings;
        settings.startPaused = true;
        // A reused nonblocking stream may seek to zero inside Play. Request the
        // target only on a later Ready/Playing poll, using the same paused voice.
        const auto bus = buses_.find("Music");
        musicVoice_ = playback_.Play(found->second.clip, settings, bus == buses_.end() ? nullptr : bus->second, error);
        if (musicVoice_ == mrg::audio::InvalidAudioPlaybackId)
        {
            lastError_ = std::move(error);
            return MusicPreparation::Failed;
        }
        voices_.push_back(musicVoice_);
        musicPositioned_ = false;
        // A nonblocking source seek can become Loading after Play. Never
        // unpause it until a subsequent poll observes Ready/Playing.
        return MusicPreparation::Loading;
    }

    bool GameplayAudioRouter::StartPreparedMusic(rhythm::RhythmTime position, const rhythm::RhythmTimer &timer)
    {
        if (musicVoice_ == mrg::audio::InvalidAudioPlaybackId)
            return true; // No source audio remains beyond the song's end.
        auto *voice = playback_.FindVoice(musicVoice_);
        std::string error;
        if (!voice)
        {
            lastError_ = "The prepared music voice is no longer available.";
            return false;
        }
        if (!voice->SetDelay(timer.ToDspClock(std::max(position, rhythm::RhythmTime{})), 0, error) ||
            !voice->SetPaused(false, error))
        {
            lastError_ = std::move(error);
            return false;
        }
        lastError_.clear();
        return true;
    }
    void GameplayAudioRouter::ResetAutomation()
    {
        appliedValues_.clear();
        for (const auto name : {"HitSound", "TickSound", "UserInputFeedback"})
            if (auto *bus = FindBus(name))
            {
                std::string error;
                if (!bus->SetVolume(1, error))
                    lastError_ = std::move(error);
            }
        for (auto &[bus, effects] : effects_)
            for (auto &[type, effect] : effects)
            {
                std::string error;
                if (effect->SetBypass(true, error))
                    bypassedEffects_.emplace(bus, type);
                else
                    lastError_ = std::move(error);
            }
    }

    void GameplayAudioRouter::Schedule(
        const std::span<const rhythm::AudioCueRequest> cues,
        const rhythm::RhythmTimer& timer)
    {
        Route(cues, &timer);
    }

    void GameplayAudioRouter::Route(
        const std::span<const rhythm::AudioCueRequest> cues,
        const rhythm::RhythmTimer* const timer)
    {
        if (audioSystem_ == nullptr)
        {
            return;
        }
        const std::uint64_t currentDspClock =
            timer != nullptr ? audioSystem_->DspClock() : 0;
        for (const rhythm::AudioCueRequest& cue : cues)
        {
            const auto sound = clips_.find(cue.sound);
            if (sound == clips_.end())
            {
                lastError_ = "No AudioClip is registered for SoundId: " +
                    cue.sound;
                continue;
            }

            mrg::audio::AudioPlaybackSettings settings;
            settings.volume = std::max(cue.volume, 0.0F);
            settings.pitch = std::max(cue.pitch, 0.01F);
            if (timer != nullptr)
            {
                const std::uint64_t requestedClock =
                    timer->ToDspClock(cue.timelineTime);
                settings.startDspClock = requestedClock > currentDspClock
                    ? requestedClock
                    : 0;
            }
            std::string error;
            const auto bus = buses_.find(cue.bus);
            const std::shared_ptr<mrg::audio::AudioBus> playbackBus =
                bus == buses_.end() ? nullptr : bus->second;

            const mrg::audio::AudioClip* const clipAddress =
                sound->second.clip.get();
            if (sound->second.restartWhilePlaying)
            {
                const auto active = sampleVoices_.find(clipAddress);
                if (active != sampleVoices_.end())
                {
                    mrg::audio::AudioVoice* const voice =
                        playback_.FindVoice(active->second);
                    if (voice != nullptr && voice->IsPlaying())
                    {
                        if (!playback_.Restart(
                                active->second,
                                settings,
                                playbackBus,
                                error))
                        {
                            lastError_ = std::move(error);
                        }
                        continue;
                    }

                    const mrg::audio::AudioPlaybackId finishedId =
                        active->second;
                    sampleVoices_.erase(active);
                    std::erase(voices_, finishedId);
                    if (voice != nullptr)
                    {
                        std::string ignoredError;
                        static_cast<void>(playback_.Stop(
                            finishedId, ignoredError));
                    }
                }
            }

            const mrg::audio::AudioPlaybackId voice = playback_.Play(
                sound->second.clip,
                settings,
                playbackBus,
                error);
            if (voice == mrg::audio::InvalidAudioPlaybackId)
            {
                lastError_ = std::move(error);
                continue;
            }
            voices_.push_back(voice);
            if (sound->second.restartWhilePlaying)
            {
                sampleVoices_.insert_or_assign(clipAddress, voice);
            }
        }
    }

    void GameplayAudioRouter::ApplyAutomation(
        const std::span<const mode::AutomationValue> values)
    {
        for (const mode::AutomationValue& value : values)
        {
            ApplyAutomationValue(value);
        }
    }

    void GameplayAudioRouter::StopAllVoices() noexcept
    {
        // Stop only this session; menu/UI playback may share the manager.
        for (const mrg::audio::AudioPlaybackId id : voices_)
        {
            try
            {
                std::string ignoredError;
                if (playback_.FindVoice(id) != nullptr)
                {
                    static_cast<void>(playback_.Stop(id, ignoredError));
                }
            }
            catch (...)
            {
                // The global manager retains any remaining voice until shutdown.
            }
        }
        voices_.clear();
        musicVoice_ = mrg::audio::InvalidAudioPlaybackId;
        musicPositioned_ = false;
        sampleVoices_.clear();
        appliedValues_.clear();
    }

    void GameplayAudioRouter::Update()
    {
        std::erase_if(
            voices_,
            [this](const mrg::audio::AudioPlaybackId id)
            {
                const mrg::audio::AudioVoice* voice = playback_.FindVoice(id);
                // The common playback manager polls native channels once.
                return voice == nullptr;
            });
        std::erase_if(sampleVoices_,
                      [this](const auto &entry) { return playback_.FindVoice(entry.second) == nullptr; });
    }

    std::string_view GameplayAudioRouter::LastError() const noexcept
    {
        return lastError_;
    }

    mrg::audio::AudioBus* GameplayAudioRouter::FindBus(
        const std::string_view id) noexcept
    {
        const auto bus = buses_.find(id);
        return bus != buses_.end() ? bus->second.get() : nullptr;
    }

    mrg::audio::AudioEffect* GameplayAudioRouter::EnsureEffect(
        const std::string_view busId,
        const mrg::audio::AudioEffectType type)
    {
        mrg::audio::AudioBus* const bus = FindBus(busId);
        if (bus == nullptr)
        {
            lastError_ = "Automation references an unknown audio bus: " +
                std::string(busId);
            return nullptr;
        }

        auto& busEffects = effects_[std::string(busId)];
        const auto existing = busEffects.find(type);
        if (existing != busEffects.end())
        {
            return existing->second.get();
        }
        std::string error;
        std::unique_ptr<mrg::audio::AudioEffect> effect =
            bus->AddEffect(type, error);
        if (effect == nullptr)
        {
            lastError_ = std::move(error);
            return nullptr;
        }
        mrg::audio::AudioEffect* const result = effect.get();
        busEffects.emplace(type, std::move(effect));
        return result;
    }

    void GameplayAudioRouter::ApplyAutomationValue(
        const mode::AutomationValue& value)
    {
        std::string error;
        const std::string_view target = value.target.empty()
            ? std::string_view{"HitSound"}
            : std::string_view{value.target};
        const auto key = std::pair{value.type, std::string(target)};
        if (const auto applied = appliedValues_.find(key);
            applied != appliedValues_.end() && applied->second == value.value)
            return;
        if (value.type == chart::EffectCommandType::BusVolume)
        {
            if (mrg::audio::AudioBus *const bus = FindBus(target); bus != nullptr)
            {
                if (!bus->SetVolume(static_cast<float>(std::max(value.value, 0.0)), error))
                    lastError_ = std::move(error);
                else
                    appliedValues_.insert_or_assign(key, value.value);
            }
            return;
        }

        mrg::audio::AudioEffectType effectType{};
        mrg::audio::AudioEffectParameter parameter{};
        float parameterValue = static_cast<float>(value.value);
        switch (value.type)
        {
        case chart::EffectCommandType::ReverbSend:
            effectType = mrg::audio::AudioEffectType::Reverb;
            parameter = mrg::audio::AudioEffectParameter::WetLevelDb;
            parameterValue = static_cast<float>(
                -80.0 + std::clamp(value.value, 0.0, 1.0) * 80.0);
            break;
        case chart::EffectCommandType::LowPassCutoff:
            effectType = mrg::audio::AudioEffectType::LowPass;
            parameter = mrg::audio::AudioEffectParameter::CutoffHz;
            break;
        case chart::EffectCommandType::HighPassCutoff:
            effectType = mrg::audio::AudioEffectType::HighPass;
            parameter = mrg::audio::AudioEffectParameter::CutoffHz;
            break;
        default:
            return;
        }

        auto *effect = EnsureEffect(target, effectType);
        if (!effect)
            return;
        const auto pending = bypassedEffects_.find({std::string(target), effectType});
        if (pending != bypassedEffects_.end())
        {
            if (!effect->SetBypass(false, error))
            {
                lastError_ = std::move(error);
                return;
            }
            bypassedEffects_.erase(pending);
        }
        if (!effect->SetParameter(parameter, parameterValue, error))
        {
            lastError_ = std::move(error);
        }
        else
            appliedValues_.insert_or_assign(key, value.value);
    }

    void GameplayAudioRouter::PrepareAutomation(std::span<const chart::CompiledEffectCommand> commands)
    {
        for (const auto &compiled : commands)
        {
            mrg::audio::AudioEffectType type;
            switch (compiled.command.type)
            {
            case chart::EffectCommandType::ReverbSend: type = mrg::audio::AudioEffectType::Reverb; break;
            case chart::EffectCommandType::LowPassCutoff: type = mrg::audio::AudioEffectType::LowPass; break;
            case chart::EffectCommandType::HighPassCutoff: type = mrg::audio::AudioEffectType::HighPass; break;
            default: continue;
            }
            const auto target = compiled.command.target.empty() ? std::string{"HitSound"} : compiled.command.target;
            if (const auto bus = effects_.find(target); bus != effects_.end() && bus->second.contains(type))
                continue;
            if (auto *effect = EnsureEffect(target, type))
            {
                std::string error;
                if (effect->SetBypass(true, error))
                    bypassedEffects_.emplace(target, type);
                else
                    lastError_ = std::move(error);
            }
        }
    }
}
