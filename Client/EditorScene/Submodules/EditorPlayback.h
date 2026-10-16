#pragma once
#include "MRG_Core.h"
#include "Modes/IEditorAudioSource.h"
#include "Time/RhythmTimer.h"
#include <optional>

// Editor-only transport/audio owner. Sources and cues come from the mode;
// no chart parsing, document mutation, or screen nodes live here.
class EditorPlayback final
{
  public:
    ~EditorPlayback()
    {
        Shutdown();
    }
    void Initialize(mrg::audio::AudioSystem &audio) noexcept
    {
        audio_ = &audio;
    }
    void Toggle();
    void Pause() noexcept;
    void Seek(finger_drum::rhythm::RhythmTime time);
    void Update(const std::map<std::string, std::filesystem::path> &files, std::uint64_t filesRevision,
                const std::vector<AudioMarker> &markers, std::uint64_t documentRevision,
                finger_drum::rhythm::RhythmTime chartEnd);
    void Shutdown() noexcept;
    [[nodiscard]] bool Active() const noexcept
    {
        return wanted_;
    }
    [[nodiscard]] finger_drum::rhythm::RhythmTime Position() const noexcept
    {
        return position_;
    }
    [[nodiscard]] std::optional<finger_drum::rhythm::RhythmTime> MusicDuration() const noexcept
    {
        return musicDuration_;
    }
    [[nodiscard]] const std::string &Error() const noexcept
    {
        return error_;
    }

  private:
    struct SoundTrack
    {
        std::shared_ptr<mrg::audio::AudioClip> clip;
        std::vector<finger_drum::rhythm::RhythmTime> times;
        std::size_t cursor{};
        mrg::audio::AudioPlaybackId voice{};
        std::optional<std::uint64_t> scheduledDspClock;
    };
    void LoadSources(const std::map<std::string, std::filesystem::path> &files, std::uint64_t revision);
    void UpdateCues(const std::vector<AudioMarker> &markers, std::uint64_t revision);
    void PrimeMusic();
    bool BeginPreparedPlayback();
    void UpdatePosition();
    void ScheduleHitSounds();
    void StopVoices() noexcept;
    void ClearSources() noexcept;

    mrg::audio::AudioSystem *audio_{};
    mrg::audio::AudioPlaybackManager voices_;
    std::shared_ptr<mrg::audio::AudioClip> music_;
    mrg::audio::AudioPlaybackId musicVoice_{};
    std::map<std::filesystem::path, SoundTrack> tracks_;
    std::map<std::string, std::filesystem::path> soundPaths_;
    std::optional<std::uint64_t> filesRevision_, cueRevision_;
    std::optional<finger_drum::rhythm::RhythmTime> musicDuration_;
    finger_drum::rhythm::RhythmTimer clock_;
    finger_drum::rhythm::RhythmTime position_{}, startPosition_{}, end_{};
    bool wanted_{}, running_{}, primed_{}, musicPositioned_{};
    std::string error_;
};
