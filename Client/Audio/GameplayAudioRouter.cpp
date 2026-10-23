#include "GameplayAudioRouter.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

GameplayAudioRouter::GameplayAudioRouter(
    AudioPlaybackManager& playback) noexcept
    : playback_(playback)
{
}

GameplayAudioRouter::~GameplayAudioRouter()
{
    Shutdown();
}

bool GameplayAudioRouter::Initialize(
    AudioSystem& audioSystem,
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
        std::unique_ptr<AudioBus> bus =
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
    SoundId soundId,
    const std::filesystem::path& path,
    const AudioLoadMode loadMode,
    std::string& errorMessage, const bool restartWhilePlaying)
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

    std::unique_ptr<AudioClip> clip =
        audioSystem_->LoadSound(path, loadMode, errorMessage);
    if (clip == nullptr)
    {
        return false;
    }
    clips_.insert_or_assign(
        std::move(soundId),
        RegisteredSound{std::move(clip), restartWhilePlaying && (loadMode == AudioLoadMode::Sample ||
                                             loadMode == AudioLoadMode::SampleAsync)});
    errorMessage.clear();
    return true;
}

bool GameplayAudioRouter::RegisterSoundAliases(const std::span<const SoundId> soundIds,
                                               const std::filesystem::path &path, std::string &errorMessage,
                                               const AudioLoadMode loadMode)
{
    if (audioSystem_ == nullptr)
    {
        errorMessage = "The gameplay audio router is not initialized.";
        return false;
    }
    if (soundIds.empty() ||
        std::ranges::any_of(
            soundIds,
            [](const SoundId& soundId)
            {
                return soundId.empty();
            }))
    {
        errorMessage = "Gameplay sound aliases require non-empty IDs.";
        return false;
    }

    std::unique_ptr<AudioClip> loaded = audioSystem_->LoadSound(path, loadMode, errorMessage);
    if (loaded == nullptr)
    {
        return false;
    }
    std::shared_ptr<AudioClip> clip = std::move(loaded);
    for (const SoundId& soundId : soundIds)
    {
        clips_.insert_or_assign(
            soundId,
            RegisteredSound{clip, loadMode == AudioLoadMode::Sample ||
                loadMode == AudioLoadMode::SampleAsync});
    }
    errorMessage.clear();
    return true;
}

bool GameplayAudioRouter::ConfigurePlayback(const PlayOptions &options, std::string &error)
{
    options.Validate();
    musicPitch_ = static_cast<float>(options.preserveMusicPitch ? 1.0 : options.accelerate);
    hitSoundPitch_ = static_cast<float>(options.preserveHitSoundPitch ? 1.0 : options.accelerate);
    const auto music = clips_.find("Music.Track");
    if (music != clips_.end() && options.preserveMusicPitch && options.accelerate != 1.0)
        return music->second.clip->PrepareTimeStretch(options.accelerate, error);
    error.clear();
    return true;
}

AudioPreparationState GameplayAudioRouter::PrepareResources()
{
    auto ready = AudioPreparationState::Ready;
    for (const auto &[id, sound] : clips_)
    {
        std::string error;
        const auto state = sound.clip->LoadState(error);
        if (state == AudioClipLoadState::Loading)
            ready = AudioPreparationState::Loading;
        else if (state == AudioClipLoadState::Failed)
        {
            lastError_ = id + ": " + error;
            return AudioPreparationState::Failed;
        }
    }
    return ready;
}

void GameplayAudioRouter::PlayNow(
    const std::span<const AudioCueRequest> cues)
{
    Route(cues, nullptr);
}
AudioPreparationState GameplayAudioRouter::PrepareMusicAt(RhythmTime position)
{
    using Preparation = AudioPreparationState;
    const auto found = clips_.find("Music.Track");
    if (found == clips_.end())
    {
        lastError_ = "No music clip is registered.";
        return Preparation::Failed;
    }
    std::string error;
    const auto state = found->second.clip->LoadState(error);
    if (state == AudioClipLoadState::Loading)
        return Preparation::Loading;
    if (state == AudioClipLoadState::Failed)
    {
        lastError_ = std::move(error);
        return Preparation::Failed;
    }
    std::uint64_t duration{};
    if (!found->second.clip->GetDurationMicroseconds(duration, error))
    {
        lastError_ = std::move(error);
        return Preparation::Failed;
    }
    const auto sourcePosition = static_cast<std::uint64_t>(std::max(position.count(), RhythmTime::rep{}));
    musicPastEnd_ = sourcePosition >= duration;
    if (musicPastEnd_)
        return Preparation::Ready;
    auto* voice = playback_.FindVoice(musicVoice_);
    if (voice && !voice->IsPlaying())
    {
        static_cast<void>(playback_.Stop(musicVoice_, error));
        std::erase(voices_, musicVoice_);
        musicVoice_ = InvalidAudioPlaybackId;
    }
    if (musicVoice_ != InvalidAudioPlaybackId)
    {
        if (!musicPositioned_)
        {
            AudioPlaybackSettings settings;
            settings.startPaused = true;
            settings.pitch = musicPitch_;
            settings.startPositionMicroseconds = sourcePosition;
            const auto bus = buses_.find("Music");
            if (!playback_.Restart(musicVoice_, settings, bus == buses_.end() ? nullptr : bus->second, error))
            {
                lastError_ = std::move(error);
                return Preparation::Failed;
            }
            musicPositioned_ = true;
        }
        voice = playback_.FindVoice(musicVoice_);
        if (!voice)
        {
            lastError_ = "The prepared music voice is no longer available.";
            return Preparation::Failed;
        }
        const auto prepared = voice->PreparationState(error);
        if (prepared == Preparation::Failed)
            lastError_ = std::move(error);
        return prepared;
    }
    AudioPlaybackSettings settings;
    settings.startPaused = true;
    settings.pitch = musicPitch_;
    settings.startPositionMicroseconds = sourcePosition;
    const auto bus = buses_.find("Music");
    musicVoice_ = playback_.Play(found->second.clip, settings, bus == buses_.end() ? nullptr : bus->second, error);
    if (musicVoice_ == InvalidAudioPlaybackId)
    {
        lastError_ = std::move(error);
        return Preparation::Failed;
    }
    voices_.push_back(musicVoice_);
    musicPositioned_ = true;
    return Preparation::Loading;
}

bool GameplayAudioRouter::StartPreparedMusic(RhythmTime position, const RhythmTimer &timer)
{
    if (musicPastEnd_ || musicVoice_ == InvalidAudioPlaybackId)
        return true; // No source audio remains beyond the song's end.
    auto *voice = playback_.FindVoice(musicVoice_);
    std::string error;
    if (!voice)
    {
        lastError_ = "The prepared music voice is no longer available.";
        return false;
    }
    if (!voice->SetDelay(timer.ToDspClock(std::max(position, RhythmTime{})), 0, error) ||
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
    const std::span<const AudioCueRequest> cues,
    const RhythmTimer& timer)
{
    Route(cues, &timer);
}

void GameplayAudioRouter::Route(
    const std::span<const AudioCueRequest> cues,
    const RhythmTimer* const timer)
{
    if (audioSystem_ == nullptr)
    {
        return;
    }
    const std::uint64_t currentDspClock =
        timer != nullptr ? audioSystem_->DspClock() : 0;
    for (const AudioCueRequest& cue : cues)
    {
        const auto sound = clips_.find(cue.sound);
        if (sound == clips_.end())
        {
            lastError_ = "No AudioClip is registered for SoundId: " +
                cue.sound;
            continue;
        }

        AudioPlaybackSettings settings;
        settings.volume = std::max(cue.volume, 0.0F);
        settings.pitch = std::max(cue.pitch * (cue.sound.starts_with("Gameplay.Metronome.") ? 1.0F : hitSoundPitch_), 0.01F);
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
        const std::shared_ptr<AudioBus> playbackBus =
            bus == buses_.end() ? nullptr : bus->second;

        const AudioClip* const clipAddress =
            sound->second.clip.get();
        if (sound->second.restartWhilePlaying)
        {
            const auto active = sampleVoices_.find(clipAddress);
            if (active != sampleVoices_.end())
            {
                AudioVoice* const voice =
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

                const AudioPlaybackId finishedId =
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

        const AudioPlaybackId voice = playback_.Play(
            sound->second.clip,
            settings,
            playbackBus,
            error);
        if (voice == InvalidAudioPlaybackId)
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
    const std::span<const AutomationValue> values)
{
    for (const AutomationValue& value : values)
    {
        ApplyAutomationValue(value);
    }
}

void GameplayAudioRouter::PauseMusicAndStopSamples()
{
    for (const auto id : voices_)
    {
        std::string error;
        auto* voice = playback_.FindVoice(id);
        if (!voice)
            continue;
        if (id == musicVoice_ && voice->IsPlaying())
        {
            if (!voice->SetPaused(true, error))
            {
                if (voice->IsPlaying())
                    lastError_ = std::move(error);
                static_cast<void>(playback_.Stop(id, error));
                musicVoice_ = InvalidAudioPlaybackId;
            }
        }
        else
            static_cast<void>(playback_.Stop(id, error));
    }
    std::erase_if(voices_, [this](auto id) { return id != musicVoice_ || !playback_.FindVoice(id); });
    if (!playback_.FindVoice(musicVoice_))
        musicVoice_ = InvalidAudioPlaybackId;
    musicPositioned_ = false;
    sampleVoices_.clear();
    appliedValues_.clear();
}

void GameplayAudioRouter::StopAllVoices() noexcept
{
    // Stop only this session; menu/UI playback may share the manager.
    for (const AudioPlaybackId id : voices_)
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
    musicVoice_ = InvalidAudioPlaybackId;
    musicPositioned_ = false;
    musicPastEnd_ = false;
    sampleVoices_.clear();
    appliedValues_.clear();
}

void GameplayAudioRouter::Update()
{
    std::erase_if(
        voices_,
        [this](const AudioPlaybackId id)
        {
            const AudioVoice* voice = playback_.FindVoice(id);
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

AudioBus* GameplayAudioRouter::FindBus(
    const std::string_view id) noexcept
{
    const auto bus = buses_.find(id);
    return bus != buses_.end() ? bus->second.get() : nullptr;
}

AudioEffect* GameplayAudioRouter::EnsureEffect(
    const std::string_view busId,
    const AudioEffectType type)
{
    AudioBus* const bus = FindBus(busId);
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
    std::unique_ptr<AudioEffect> effect =
        bus->AddEffect(type, error);
    if (effect == nullptr)
    {
        lastError_ = std::move(error);
        return nullptr;
    }
    AudioEffect* const result = effect.get();
    busEffects.emplace(type, std::move(effect));
    return result;
}

void GameplayAudioRouter::ApplyAutomationValue(
    const AutomationValue& value)
{
    std::string error;
    const std::string_view target = value.target.empty()
        ? std::string_view{"HitSound"}
        : std::string_view{value.target};
    const auto key = std::pair{value.type, std::string(target)};
    if (const auto applied = appliedValues_.find(key);
        applied != appliedValues_.end() && applied->second == value.value)
        return;
    if (value.type == EffectCommandType::BusVolume)
    {
        if (AudioBus *const bus = FindBus(target); bus != nullptr)
        {
            if (!bus->SetVolume(static_cast<float>(std::max(value.value, 0.0)), error))
                lastError_ = std::move(error);
            else
                appliedValues_.insert_or_assign(key, value.value);
        }
        return;
    }

    AudioEffectType effectType{};
    AudioEffectParameter parameter{};
    float parameterValue = static_cast<float>(value.value);
    switch (value.type)
    {
    case EffectCommandType::ReverbSend:
        effectType = AudioEffectType::Reverb;
        parameter = AudioEffectParameter::WetLevelDb;
        parameterValue = static_cast<float>(
            -80.0 + std::clamp(value.value, 0.0, 1.0) * 80.0);
        break;
    case EffectCommandType::LowPassCutoff:
        effectType = AudioEffectType::LowPass;
        parameter = AudioEffectParameter::CutoffHz;
        break;
    case EffectCommandType::HighPassCutoff:
        effectType = AudioEffectType::HighPass;
        parameter = AudioEffectParameter::CutoffHz;
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

void GameplayAudioRouter::PrepareAutomation(std::span<const CompiledEffectCommand> commands)
{
    for (const auto &compiled : commands)
    {
        AudioEffectType type;
        switch (compiled.command.type)
        {
        case EffectCommandType::ReverbSend: type = AudioEffectType::Reverb; break;
        case EffectCommandType::LowPassCutoff: type = AudioEffectType::LowPass; break;
        case EffectCommandType::HighPassCutoff: type = AudioEffectType::HighPass; break;
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
