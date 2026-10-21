#include "App/AssetPaths.h"
#include "GameFlow/GameplaySessionController.h"
#include "Taiko/Submodules/TaikoSoundIds.h"
#include <array>
#include <map>
namespace
{
    void RegisterTaikoSounds(GameplayAudioRouter &router, std::string &errorMessage);
}

void GameplaySessionController::InitializeAudio(AudioSystem &audio)
{
    initializationError_.clear();
    std::string initializationError;
    if (!audioRouter_.Initialize(audio, initializationError))
    {
        initializationError_ = initializationError;
        return;
    }

    std::string hitSoundError;
    if (session_->InputLayout().modeId == "Taiko")
        RegisterTaikoSounds(audioRouter_, hitSoundError);
    audioRouter_.PrepareAutomation(session_->Effects());

    // Decode chart resources once on session entry, never on a key press.
    // Indices resolving to the same file share one sample/channel.
    std::map<std::filesystem::path, std::vector<SoundId>> chartSounds;
    for (const auto &[id, path] : session_->HitSoundFiles())
    {
        chartSounds[path].push_back(id);
    }
    for (const auto &[path, ids] : chartSounds)
    {
        std::string error;
        if (!audioRouter_.RegisterSoundAliases(ids, path, error, AudioLoadMode::SampleAsync) &&
            hitSoundError.empty())
        {
            hitSoundError = ids.front() + ": " + error;
        }
    }

    std::string musicError;
    if (launchRequest_.IsValid())
    {
        musicRegistered_ = audioRouter_.RegisterSound("Music.Track", launchRequest_.musicPath,
                                                      AudioLoadMode::StreamAsyncAccurate, musicError);
    }

    if (!hitSoundError.empty() || !musicError.empty())
    {
        initializationError_ = !musicError.empty() ? musicError : hitSoundError;
    }
}

namespace
{
    void RegisterTaikoSounds(GameplayAudioRouter &router, std::string &errorMessage)
    {
        struct SoundRegistration final
        {
            std::span<const SoundId> ids;
            std::wstring_view file;
        };
        static const std::array<SoundId, 3> DonIds{DonHit,
                                                                        LongNoteTick,
                                                                        DonFreeInput};
        static const std::array<SoundId, 2> KatIds{KatHit,
                                                                        KatFreeInput};
        static const std::array<SoundId, 1> BigDonIds{
            BigDonFirstHit};
        static const std::array<SoundId, 1> BigKatIds{
            BigKatFirstHit};
        const std::array<SoundRegistration, 4> registrations{{
            {DonIds, L"don.wav"},
            {KatIds, L"kat.wav"},
            {BigDonIds, L"bigdon.wav"},
            {BigKatIds, L"bigkat.wav"},
        }};
        std::string firstError;
        for (const SoundRegistration &registration : registrations)
        {
            std::string registrationError;
            if (!router.RegisterSoundAliases(registration.ids,
                                             TaikoHitSound(registration.file),
                                             registrationError, AudioLoadMode::SampleAsync) &&
                firstError.empty())
            {
                firstError = registration.ids.front() + ": " + registrationError;
            }
        }
        std::string balloonError;
        if (!router.RegisterSound(BalloonPop,
                                  TaikoHitSound(L"pop.wav"),
                                  AudioLoadMode::SampleAsync, balloonError) &&
            firstError.empty())
        {
            firstError = "Taiko.Balloon.Pop: " + balloonError;
        }
        errorMessage = std::move(firstError);
    }
} // namespace
