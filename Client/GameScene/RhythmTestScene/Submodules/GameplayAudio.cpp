#include "GameplaySessionController.h"
#include "GameplaySupport.h"
using namespace gameplay;

void GameplaySessionController::InitializeAudio(const mrg::EngineServices &services)
{
    std::string initializationError;
    if (!audioRouter_.Initialize(services.audio, initializationError))
    {
        initializationError_ = initializationError;
        return;
    }

    std::string hitSoundError;
    RegisterTaikoSounds(hitSoundError);

    // Decode chart resources once on session entry, never on a key press.
    // Indices resolving to the same file share one sample/channel.
    std::map<std::filesystem::path, std::vector<finger_drum::rhythm::SoundId>> chartSounds;
    for (const auto &[id, path] : session_->HitSoundFiles())
    {
        chartSounds[path].push_back(id);
    }
    for (const auto &[path, ids] : chartSounds)
    {
        std::string error;
        if (!audioRouter_.RegisterSoundAliases(ids, path, error) && hitSoundError.empty())
        {
            hitSoundError = ids.front() + ": " + error;
        }
    }

    std::string musicError;
    if (launchRequest_.IsValid())
    {
        musicRegistered_ = audioRouter_.RegisterSound(
            "Music.Track", launchRequest_.musicPath, mrg::audio::AudioLoadMode::Stream, musicError);
    }

    if (!hitSoundError.empty() || !musicError.empty())
    {
        initializationError_ = !musicError.empty() ? musicError : hitSoundError;
    }
}

void GameplaySessionController::RegisterTaikoSounds(std::string &errorMessage)
{
    struct SoundRegistration final
    {
        std::span<const finger_drum::rhythm::SoundId> ids;
        std::wstring_view file;
    };
    static const std::array<finger_drum::rhythm::SoundId, 3> DonIds{
        finger_drum::mode::taiko_sound::DonHit, finger_drum::mode::taiko_sound::LongNoteTick,
        finger_drum::mode::taiko_sound::DonFreeInput};
    static const std::array<finger_drum::rhythm::SoundId, 2> KatIds{
        finger_drum::mode::taiko_sound::KatHit, finger_drum::mode::taiko_sound::KatFreeInput};
    static const std::array<finger_drum::rhythm::SoundId, 1> BigDonIds{
        finger_drum::mode::taiko_sound::BigDonFirstHit};
    static const std::array<finger_drum::rhythm::SoundId, 1> BigKatIds{
        finger_drum::mode::taiko_sound::BigKatFirstHit};
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
        if (!audioRouter_.RegisterSoundAliases(
                registration.ids, TaikoHitSoundAssetPath(registration.file), registrationError) &&
            firstError.empty())
        {
            firstError = registration.ids.front() + ": " + registrationError;
        }
    }
    std::string balloonError;
    if (!audioRouter_.RegisterSound(finger_drum::mode::taiko_sound::BalloonPop,
                                    TaikoHitSoundAssetPath(L"pop.wav"),
                                    mrg::audio::AudioLoadMode::Sample, balloonError) &&
        firstError.empty())
    {
        firstError = "Taiko.Balloon.Pop: " + balloonError;
    }
    errorMessage = std::move(firstError);
}

void GameplaySessionController::ScheduleMusic()
{
    if (!musicRegistered_)
    {
        return;
    }
    finger_drum::rhythm::AudioCueRequest cue;
    cue.sound = "Music.Track";
    cue.bus = "Music";
    cue.timelineTime = finger_drum::rhythm::RhythmTime::zero();
    const std::array cues{cue};
    audioRouter_.Schedule(cues, timer_);
}
