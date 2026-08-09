#include "GameplayAudioRouter.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace finger_drum::audio
{
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
        voices_.clear();
        effects_.clear();
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
        clips_.insert_or_assign(std::move(soundId), std::move(clip));
        errorMessage.clear();
        return true;
    }

    bool GameplayAudioRouter::RegisterSoundAliases(
        const std::span<const rhythm::SoundId> soundIds,
        const std::filesystem::path& path,
        std::string& errorMessage)
    {
        for (const rhythm::SoundId& soundId : soundIds)
        {
            if (!RegisterSound(
                    soundId,
                    path,
                    mrg::audio::AudioLoadMode::Sample,
                    errorMessage))
            {
                return false;
            }
        }
        return true;
    }

    void GameplayAudioRouter::Route(
        const std::span<const rhythm::AudioCueRequest> cues,
        const rhythm::RhythmTimer& timer)
    {
        if (audioSystem_ == nullptr)
        {
            return;
        }
        const std::uint64_t currentDspClock = audioSystem_->DspClock();
        for (const rhythm::AudioCueRequest& cue : cues)
        {
            const auto clip = clips_.find(cue.sound);
            if (clip == clips_.end())
            {
                lastError_ = "No AudioClip is registered for SoundId: " +
                    cue.sound;
                continue;
            }

            mrg::audio::AudioPlaybackSettings settings;
            settings.volume = std::max(cue.volume, 0.0F);
            settings.pitch = std::max(cue.pitch, 0.01F);
            const std::uint64_t requestedClock =
                timer.ToDspClock(cue.timelineTime);
            settings.startDspClock = requestedClock > currentDspClock
                ? requestedClock
                : 0;
            std::string error;
            std::unique_ptr<mrg::audio::AudioVoice> voice = clip->second->Play(
                settings,
                FindBus(cue.bus),
                error);
            if (voice == nullptr)
            {
                lastError_ = std::move(error);
                continue;
            }
            voices_.push_back(std::move(voice));
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

    void GameplayAudioRouter::Update()
    {
        std::erase_if(
            voices_,
            [](const std::unique_ptr<mrg::audio::AudioVoice>& voice)
            {
                return voice == nullptr || !voice->IsPlaying();
            });
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
        if (value.type == chart::EffectCommandType::BusVolume)
        {
            if (mrg::audio::AudioBus* const bus = FindBus(target);
                bus != nullptr && !bus->SetVolume(
                    static_cast<float>(std::max(value.value, 0.0)),
                    error))
            {
                lastError_ = std::move(error);
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

        if (mrg::audio::AudioEffect* const effect = EnsureEffect(
                target,
                effectType);
            effect != nullptr && !effect->SetParameter(
                parameter,
                parameterValue,
                error))
        {
            lastError_ = std::move(error);
        }
    }
}
