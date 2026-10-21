#pragma once
#include "PhantomCore.h"
#include "Modes/IEditorAudioSource.h"
#include "Time/RhythmTimer.h"
#include <optional>

// Editor-only transport/audio owner. Sources and cues come from the mode;
// no chart parsing, document mutation, or screen nodes live here.
class EditorPlayback final
{
  public:
    static constexpr double MinimumRate = .25, MaximumRate = 2.0;
    void SetRate(double rate);
    double Rate() const noexcept
    {
        return rate_;
    }
    ~EditorPlayback()
    {
        Shutdown();
    }
    void Initialize(AudioSystem &audio) noexcept
    {
        audio_ = &audio;
    }
    void Toggle();
    void Pause() noexcept;
    void Seek(RhythmTime time);
    void Update(const std::map<std::string, std::filesystem::path> &files, std::uint64_t filesRevision,
                const std::vector<AudioMarker> &markers, std::uint64_t documentRevision,
                RhythmTime chartEnd);
    void Shutdown() noexcept;
    [[nodiscard]] bool Active() const noexcept
    {
        return wanted_;
    }
    [[nodiscard]] RhythmTime Position() const noexcept
    {
        return position_;
    }
    [[nodiscard]] std::optional<RhythmTime> MusicDuration() const noexcept
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
        std::shared_ptr<AudioClip> clip;
        std::vector<RhythmTime> times;
        std::size_t cursor{};
        AudioPlaybackId voice{};
        std::optional<std::uint64_t> scheduledDspClock;
    };
    void LoadSources(const std::map<std::string, std::filesystem::path> &files, std::uint64_t revision);
    void UpdateCues(const std::vector<AudioMarker> &markers, std::uint64_t revision);
    void PrimeMusic();
    bool BeginPreparedPlayback();
    void UpdatePosition();
    [[nodiscard]] RhythmTime PositionAtDsp(std::uint64_t dspClock) const noexcept;
    [[nodiscard]] std::uint64_t DspClockAt(RhythmTime time) const noexcept;
    void AdjustTrackRates(double rate, std::uint64_t dspClock, std::string &error);
    void ScheduleHitSounds();
    void StopVoices() noexcept;
    void PauseVoices();
    void ClearSources() noexcept;

    AudioSystem *audio_{};
    AudioPlaybackManager voices_;
    std::shared_ptr<AudioClip> music_;
    AudioPlaybackId musicVoice_{};
    std::map<std::filesystem::path, SoundTrack> tracks_;
    std::map<std::string, std::filesystem::path> soundPaths_;
    std::optional<std::uint64_t> filesRevision_, cueRevision_;
    std::optional<RhythmTime> musicDuration_;
    RhythmTimer clock_;
    RhythmTime position_{}, startPosition_{}, end_{};
    std::uint64_t dspAnchorClock_{};
    double rate_{1};
    bool wanted_{}, running_{}, primed_{}, musicPositioned_{};
    std::string error_;
};
