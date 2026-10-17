#include "EditorPlayback.h"
#include "EditorTime.h"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace
{
    namespace a = mrg::audio;
    using Time = finger_drum::rhythm::RhythmTime;
    constexpr Time Lead{20'000};
    void Require(bool success, const std::string &error)
    {
        if (!success)
            throw std::runtime_error(error.empty() ? "Editor audio playback failed." : error);
    }
} // namespace

void EditorPlayback::Toggle()
{
    if (wanted_)
        Pause();
    else
    {
        if (!audio_ || !audio_->IsInitialized())
            throw std::runtime_error("The editor audio system is not initialized.");
        error_.clear();
        wanted_ = true;
    }
}

void EditorPlayback::UpdatePosition()
{
    if (running_ && audio_)
        position_ = std::max(startPosition_, clock_.FromDspClock(audio_->DspClock()));
}

void EditorPlayback::StopVoices() noexcept
{
    voices_.StopAll();
    musicVoice_ = 0;
    running_ = primed_ = musicPositioned_ = false;
    for (auto &[path, track] : tracks_)
    {
        track.voice = 0;
        track.scheduledDspClock.reset();
    }
}

void EditorPlayback::Pause() noexcept
{
    UpdatePosition();
    try
    {
        PauseVoices();
    }
    catch (const std::exception &error)
    {
        StopVoices();
        error_ = error.what();
    }
    wanted_ = false;
}

void EditorPlayback::PauseVoices()
{
    std::string error;
    if (auto *voice = voices_.FindVoice(musicVoice_); voice && voice->IsPlaying())
        if (!voice->SetPaused(true, error) && voice->IsPlaying())
            Require(false, error);
    for (auto &[path, track] : tracks_)
    {
        if (voices_.FindVoice(track.voice))
            Require(voices_.Stop(track.voice, error), error);
        track.voice = 0;
        track.scheduledDspClock.reset();
    }
    running_ = primed_ = musicPositioned_ = false;
}

void EditorPlayback::Seek(const Time time)
{
    if (time <= Time::min() + Lead || time >= Time::max() - Lead)
        throw std::invalid_argument("Editor playback position leaves no scheduling margin.");
    PauseVoices();
    position_ = time;
    // Re-anchor and re-prime from the new position on the next Update.
    // Preserve the playing/paused intent even during an async stream seek.
}

void EditorPlayback::ClearSources() noexcept
{
    StopVoices();
    const auto retire = [this](std::shared_ptr<a::AudioClip> clip)
    {
        if (!clip || !audio_)
            return;
        try
        {
            audio_->RetireClip(std::move(clip));
        }
        catch (...)
        {
        }
    };
    retire(std::move(music_));
    for (auto &[path, track] : tracks_)
        retire(std::move(track.clip));
    tracks_.clear();
    soundPaths_.clear();
    filesRevision_.reset();
    cueRevision_.reset();
    musicDuration_.reset();
}

void EditorPlayback::LoadSources(const std::map<std::string, std::filesystem::path> &files,
                                 const std::uint64_t revision)
{
    UpdatePosition();
    ClearSources();
    const auto music = files.find("Music");
    if (music == files.end())
        throw std::runtime_error("The editor mode did not provide a music file.");
    std::string error;
    music_ = audio_->LoadSound(music->second, a::AudioLoadMode::StreamAsyncAccurate, error);
    Require(music_ != nullptr, error);
    // Alias IDs for one sample share a clip and a bounded native voice.
    for (const auto &[sound, path] : files)
    {
        if (sound == "Music")
            continue;
        soundPaths_[sound] = path;
        auto &track = tracks_[path];
        if (!track.clip)
        {
            track.clip = audio_->LoadSound(path, a::AudioLoadMode::Sample, error);
            Require(track.clip != nullptr, error);
        }
    }
    filesRevision_ = revision;
}

void EditorPlayback::UpdateCues(const std::vector<AudioMarker> &markers, const std::uint64_t revision)
{
    for (auto &[path, track] : tracks_)
    {
        if (voices_.FindVoice(track.voice))
        {
            std::string error;
            Require(voices_.Stop(track.voice, error), error);
        }
        track.voice = 0;
        track.scheduledDspClock.reset();
        track.times.clear();
    }
    for (const auto &marker : markers)
    {
        const auto path = soundPaths_.find(marker.sound);
        if (path == soundPaths_.end())
            throw std::runtime_error("An editor hit sound has no file: " + marker.sound);
        tracks_.at(path->second).times.push_back(editor_time::Microseconds(marker.seconds * 1000.0));
    }
    for (auto &[path, track] : tracks_)
    {
        std::ranges::sort(track.times);
        track.cursor = static_cast<std::size_t>(std::lower_bound(track.times.begin(), track.times.end(), position_) -
                                                track.times.begin());
    }
    cueRevision_ = revision;
}

void EditorPlayback::PrimeMusic()
{
    std::string error;
    const auto state = music_->LoadState(error);
    if (state == a::AudioClipLoadState::Failed)
        throw std::runtime_error(error);
    if (state != a::AudioClipLoadState::Ready && state != a::AudioClipLoadState::Playing)
        return;
    std::uint64_t duration = 0;
    Require(music_->GetDurationMicroseconds(duration, error), error);
    if (duration == 0 || duration > static_cast<std::uint64_t>(Time::max().count()))
        throw std::runtime_error("The music duration is outside the editor time range.");
    musicDuration_ = Time{static_cast<Time::rep>(duration)};
    if (position_ < *musicDuration_)
    {
        auto *voice = voices_.FindVoice(musicVoice_);
        if (!voice || !voice->IsPlaying())
        {
            if (voice)
                Require(voices_.Stop(musicVoice_, error), error);
            a::AudioPlaybackSettings settings;
            settings.startPaused = true;
            settings.startPositionMicroseconds = static_cast<std::uint64_t>(std::max(position_, Time{}).count());
            musicVoice_ = voices_.Play(music_, settings, nullptr, error);
            Require(musicVoice_ != 0, error);
            musicPositioned_ = true;
        }
    }
    primed_ = true;
}

bool EditorPlayback::BeginPreparedPlayback()
{
    std::string error;
    const auto state = music_->LoadState(error);
    if (state == a::AudioClipLoadState::Failed)
        throw std::runtime_error(error);
    if (state != a::AudioClipLoadState::Ready && state != a::AudioClipLoadState::Playing)
        return false;
    const bool hasMusic = musicVoice_ && position_ < musicDuration_.value_or(Time{});
    if (hasMusic)
    {
        if (!musicPositioned_)
        {
            a::AudioPlaybackSettings settings;
            settings.startPaused = true;
            settings.startPositionMicroseconds = static_cast<std::uint64_t>(std::max(position_, Time{}).count());
            Require(voices_.Restart(musicVoice_, settings, nullptr, error), error);
            musicPositioned_ = true;
            return false; // Anchor on a later poll, after the requested seek.
        }
        auto *voice = voices_.FindVoice(musicVoice_);
        if (!voice)
            throw std::runtime_error("The prepared music voice has stopped.");
        const auto prepared = voice->PreparationState(error);
        Require(prepared != a::AudioPreparationState::Failed, error);
        if (prepared == a::AudioPreparationState::Loading)
            return false;
    }
    const auto snapshot = audio_->CaptureClockSnapshot();
    if (snapshot.sampleRate <= 0)
        throw std::runtime_error("The audio mixer has no sample clock.");
    const auto leadSamples = static_cast<std::uint64_t>(snapshot.sampleRate / 50);
    clock_.Start(snapshot.performanceCounterTicks, snapshot.performanceCounterFrequency, position_);
    clock_.AnchorDspClock(position_, snapshot.dspClock + leadSamples, snapshot.sampleRate);
    if (hasMusic)
    {
        auto *voice = voices_.FindVoice(musicVoice_);
        if (!voice)
            throw std::runtime_error("The prepared music voice has stopped.");
        Require(voice->SetDelay(clock_.ToDspClock(std::max(position_, Time{})), 0, error), error);
        Require(voice->SetPaused(false, error), error);
    }
    startPosition_ = position_;
    for (auto &[path, track] : tracks_)
        track.cursor = static_cast<std::size_t>(std::lower_bound(track.times.begin(), track.times.end(), position_) -
                                                track.times.begin());
    running_ = true;
    return true;
}

void EditorPlayback::ScheduleHitSounds()
{
    for (auto &[path, track] : tracks_)
    {
        if (track.scheduledDspClock && *track.scheduledDspClock > audio_->DspClock())
            continue; // Never overwrite a sample that has not started yet.
        if (track.cursor == track.times.size())
            continue;
        // A suspended UI/modal interval cannot be replayed retroactively.
        // Binary-search past expired cues instead of producing a catch-up burst.
        const auto first = track.times.begin() + static_cast<std::ptrdiff_t>(track.cursor);
        track.cursor = static_cast<std::size_t>(std::lower_bound(first, track.times.end(), position_ - Lead) -
                                                track.times.begin());
        if (track.cursor == track.times.size() || track.times[track.cursor] > position_ + Lead)
            continue;
        const auto time = track.times[track.cursor++];
        a::AudioPlaybackSettings settings;
        settings.startDspClock = clock_.ToDspClock(time);
        std::string error;
        if (voices_.FindVoice(track.voice))
            Require(voices_.Restart(track.voice, settings, nullptr, error), error);
        else
        {
            track.voice = voices_.Play(track.clip, settings, nullptr, error);
            Require(track.voice != 0, error);
        }
        track.scheduledDspClock = settings.startDspClock;
    }
}

void EditorPlayback::Update(const std::map<std::string, std::filesystem::path> &files,
                            const std::uint64_t filesRevision, const std::vector<AudioMarker> &markers,
                            const std::uint64_t documentRevision, const Time chartEnd)
{
    if (!wanted_)
        return;
    try
    {
        if (filesRevision_ != filesRevision)
            LoadSources(files, filesRevision);
        if (cueRevision_ != documentRevision)
            UpdateCues(markers, documentRevision);
        if (!primed_)
            PrimeMusic();
        if (!primed_ || (!running_ && !BeginPreparedPlayback()))
            return;
        end_ = std::max(musicDuration_.value_or(Time{}), chartEnd);
        UpdatePosition();
        if (position_ >= end_)
        {
            Pause();
            position_ = end_;
        }
        else
            ScheduleHitSounds();
        voices_.Update();
    }
    catch (const std::exception &error)
    {
        Pause();
        error_ = error.what();
    }
}

void EditorPlayback::Shutdown() noexcept
{
    wanted_ = false;
    ClearSources();
    audio_ = nullptr;
    clock_.Stop();
}
