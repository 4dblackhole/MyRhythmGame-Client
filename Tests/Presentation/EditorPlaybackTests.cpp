#include "EditorPlaybackTests.h"
#include "EditorScene/Submodules/EditorPlayback.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <limits>
#include <stdexcept>
#include <thread>

namespace
{

    using Time = RhythmTime;
    void Check(bool value, const char *message)
    {
        if (!value)
            throw std::runtime_error(message);
    }
    // A controllable DSP clock makes cue deadlines, pre-roll and stalled UI
    // intervals deterministic without depending on a physical audio driver.
    std::uint64_t dspClock{};
    class ClockBackend final : public IAudioBackend
    {
      public:
        bool Initialize(const AudioConfig &, std::string &) override
        {
            return true;
        }
        void Update() override
        {
        }
        void Shutdown() noexcept override
        {
        }
        std::string_view Name() const noexcept override
        {
            return "Clock probe";
        }
        AudioOutputBackend RequestedOutput() const noexcept override
        {
            return AudioOutputBackend::NoSound;
        }
        AudioOutputBackend ActiveOutput() const noexcept override
        {
            return RequestedOutput();
        }
        bool SetOutputBackend(AudioOutputBackend, std::string &) override
        {
            return false;
        }
        int DriverCount() const noexcept override
        {
            return 0;
        }
        const std::vector<AudioDeviceInfo> &OutputDrivers() const noexcept override
        {
            return devices;
        }
        int ActiveDriverIndex() const noexcept override
        {
            return -1;
        }
        bool SetOutputDriver(int, std::string &) override
        {
            return false;
        }
        int RequestedSampleRate() const noexcept override
        {
            return 48000;
        }
        int SampleRate() const noexcept override
        {
            return 48000;
        }
        std::uint32_t DspBufferLength() const noexcept override
        {
            return 256;
        }
        int DspBufferCount() const noexcept override
        {
            return 4;
        }
        double EstimatedDspLatencyMilliseconds() const noexcept override
        {
            return 0;
        }
        bool SetSampleRate(int, std::string &) override
        {
            return false;
        }
        bool SetDspBufferSize(std::uint32_t, int, std::string &) override
        {
            return false;
        }
        std::uint64_t DspClock() const noexcept override
        {
            return dspClock;
        }
        std::unique_ptr<IAudioBusBackend> CreateBus(std::string_view, IAudioBusBackend *, std::string &) override
        {
            return {};
        }

      private:
        std::vector<AudioDeviceInfo> devices;
    };
    std::unique_ptr<IAudioBackend> CreateClockBackend()
    {
        return std::make_unique<ClockBackend>();
    }
    struct Probe
    {
        bool playing{true};
        bool paused{};
        std::filesystem::path path;
        AudioPlaybackSettings settings;
        std::vector<std::uint64_t> deadlines;
        std::size_t restarts{};
    };
    std::vector<std::shared_ptr<Probe>> probes;
    std::size_t loads{};
    bool failDuration{};
    bool failPitch{};
    std::uint64_t clockOnStop{};
    AudioClipLoadState loadState{AudioClipLoadState::Ready};
    class Voice final : public IAudioVoiceBackend
    {
      public:
        explicit Voice(std::shared_ptr<Probe> value) : probe(std::move(value))
        {
        }
        bool IsPlaying() const noexcept override
        {
            return probe->playing;
        }
        bool Restart(const AudioPlaybackSettings &settings, IAudioBusBackend *, std::string &) override
        {
            ++probe->restarts;
            probe->settings = settings;
            probe->deadlines.push_back(settings.startDspClock);
            return true;
        }
        bool Stop(std::string &) override
        {
            probe->playing = false;
            if (clockOnStop)
                dspClock = clockOnStop;
            return true;
        }
        bool SetPaused(bool value, std::string &) override
        {
            probe->paused = value;
            return true;
        }
        bool SetVolume(float, std::string &) override
        {
            return true;
        }
        bool SetPitch(float pitch, std::string &error) override
        {
            if (failPitch)
            {
                error = "rate probe failure";
                return false;
            }
            probe->settings.pitch = pitch;
            return true;
        }
        bool SetDelay(std::uint64_t start, std::uint64_t, std::string &) override
        {
            probe->settings.startDspClock = start;
            return true;
        }

      private:
        std::shared_ptr<Probe> probe;
    };
    class Clip final : public IAudioClipBackend
    {
      public:
        explicit Clip(std::filesystem::path value) : path(std::move(value))
        {
        }
        AudioClipLoadState LoadState(std::string &) const override
        {
            return loadState;
        }
        bool GetDurationMicroseconds(std::uint64_t &duration, std::string &error) const override
        {
            duration = 2'000'000;
            if (failDuration)
                error = "duration probe failure";
            return !failDuration;
        }
        std::unique_ptr<IAudioVoiceBackend> Play(const AudioPlaybackSettings &settings, IAudioBusBackend *,
                                                    std::string &) override
        {
            auto probe = std::make_shared<Probe>();
            probe->path = path;
            probe->settings = settings;
            probe->paused = settings.startPaused;
            probe->deadlines.push_back(settings.startDspClock);
            probes.push_back(probe);
            return std::make_unique<Voice>(probe);
        }

      private:
        std::filesystem::path path;
    };
    std::unique_ptr<IAudioClipBackend> CreateClip(IAudioBackend &, const std::filesystem::path &path,
                                                     AudioLoadMode mode, std::string &)
    {
        Check((path == "music") == (mode == AudioLoadMode::StreamAsyncAccurate),
              "Music must use accurate async loading; hit samples must be cached.");
        ++loads;
        return std::make_unique<Clip>(path);
    }
    void TestPlaybackRates()
    {
        dspClock = 0;
        probes.clear();
        loads = 0;
        AudioSystem audio;
        std::string error;
        Check(audio.Initialize({}, CreateClockBackend, CreateClip, error), "Rate probe audio initialization failed.");
        EditorPlayback playback;
        playback.Initialize(audio);
        const std::map<std::string, std::filesystem::path> files{{"Music", "music"}, {"Don", "don"}};
        const std::vector<AudioMarker> cues{{0, "Don"}, {.01, "Don"}, {1, "Don"}};
        const auto update = [&] { playback.Update(files, 1, cues, 1, Time{3'000'000}); };
        playback.Seek(Time{-100'000});
        playback.SetRate(.5);
        playback.Toggle();
        update();
        const auto music = probes.front();
        Check(music->settings.startDspClock == 10560 && music->settings.pitch == .5F,
              "Half speed must double negative pre-roll and change the native sample rate together.");
        dspClock = 10080; // Five source milliseconds before the song starts.
        update();
        Check(playback.Position() == Time{-5'000} && probes.back()->settings.startDspClock == 10560,
              "A half-speed cue must share the delayed music deadline.");
        const auto queued = probes.back();
        playback.SetRate(2);
        Check(playback.Position() == Time{-5'000} && music->settings.startDspClock == 10200 &&
                  music->settings.pitch == 2 && music->restarts == 0 && !queued->playing,
              "Live rate changes must keep source position, retime pre-roll and cancel only the pending cue.");
        update();
        Check(probes.back()->settings.startDspClock == 10200 && probes.back()->settings.pitch == 2 && loads == 2,
              "The cancelled cue must be rescheduled under the new rate without reloading files.");
        dspClock = 10200;
        update();
        Check(probes.back()->settings.startDspClock == 10440,
              "A ten-source-millisecond cue gap must become five real milliseconds at double speed.");
        dspClock = 10440;
        update();
        const auto audible = probes.back();
        const auto playedCount = audible->deadlines.size();
        playback.SetRate(.25);
        update();
        Check(audible->playing && audible->settings.pitch == .25F && audible->deadlines.size() == playedCount &&
                  music->restarts == 0 && playback.Position() == Time{10'000},
              "Changing rate during an audible sample must preserve its tail and never repeat its attack.");
        dspClock += 48000;
        update();
        Check(playback.Position() == Time{260'000}, "One real second must advance 250 source ms at quarter speed.");
        playback.Pause();
        playback.SetRate(1.5);
        playback.Toggle();
        update();
        update();
        Check(music->settings.startPositionMicroseconds == 260'000 && music->settings.pitch == 1.5F && loads == 2,
              "Pause/resume must preserve the selected rate and absolute source seek position.");
        const auto futureStart = music->settings.startDspClock;
        playback.SetRate(2);
        Check(playback.Position() == Time{260'000} && music->settings.startDspClock == futureStart,
              "A rate change before the initial DSP lead expires must not advance the source cursor.");
        for (const double invalid : {0.0, 2.01, std::numeric_limits<double>::infinity(),
                                     std::numeric_limits<double>::quiet_NaN()})
        {
            bool rejected = false;
            try { playback.SetRate(invalid); }
            catch (const std::invalid_argument &) { rejected = true; }
            Check(rejected && playback.Rate() == 2 && playback.Active(),
                  "Invalid rates must not modify the active transport.");
        }
        failPitch = true;
        bool reported = false;
        try { playback.SetRate(1); }
        catch (const std::runtime_error &failure) { reported = std::string_view(failure.what()) == "rate probe failure"; }
        failPitch = false;
        Check(reported && !playback.Active() && playback.Rate() == 2,
              "A native frequency failure must stop playback and report the error without publishing a new rate.");
        playback.Shutdown();
        audio.Update();
    }

    void TestRateChangeAcrossCueDeadline()
    {
        dspClock = 0;
        probes.clear();
        AudioSystem audio;
        std::string error;
        Check(audio.Initialize({}, CreateClockBackend, CreateClip, error), "Deadline probe initialization failed.");
        EditorPlayback playback;
        playback.Initialize(audio);
        const std::map<std::string, std::filesystem::path> files{{"Music", "music"}, {"Don", "don"}};
        const std::vector<AudioMarker> cues{{0, "Don"}, {.01, "Don"}};
        const auto update = [&] { playback.Update(files, 1, cues, 1, Time{3'000'000}); };
        playback.Seek(Time{-100'000});
        playback.Toggle();
        update();
        dspClock = 5280; // Anchor 960 + 90 ms; the zero-time hit is queued for 5760.
        update();
        Check(probes.back()->settings.startDspClock == 5760, "Deadline probe did not queue the expected hit.");
        clockOnStop = 5761; // Simulate a mixer update during native Stop.
        playback.SetRate(2);
        clockOnStop = 0;
        update();
        Check(probes.back()->settings.startDspClock == 5760,
              "A cue crossed during a native control call must not replay; queue the next ten-ms cue instead.");
        playback.Shutdown();
        audio.Update();
    }
} // namespace

void TestEditorPlayback()
{
    TestPlaybackRates();
    TestRateChangeAcrossCueDeadline();
    dspClock = 0;
    probes.clear();
    loads = 0;
    AudioSystem audio;
    std::string error;
    Check(audio.Initialize({}, CreateClockBackend, CreateClip, error), "Probe audio initialization failed.");
    EditorPlayback playback;
    playback.Initialize(audio);
    const std::map<std::string, std::filesystem::path> files{{"Music", "music"}, {"Don", "don"}, {"Alias", "don"}};
    const std::vector<AudioMarker> cues{{0, "Don"}, {.005, "Alias"}, {.010, "Don"}, {1, "Don"}};
    const auto update = [&] { playback.Update(files, 1, cues, 1, Time{3'000'000}); };
    playback.Toggle();
    loadState = AudioClipLoadState::Loading;
    update();
    Check(playback.Active() && probes.empty() && loads == 2 && playback.Position() == Time{},
          "Loading must not advance time or allocate a voice; aliases must share a clip.");
    loadState = AudioClipLoadState::Ready;
    update();
    Check(probes.size() == 2 && probes[0]->settings.startDspClock == 960 && !probes[0]->paused &&
              probes[1]->deadlines == std::vector<std::uint64_t>{960},
          "Music and first hit must share the DSP anchor.");
    const auto musicProbe = probes[0];
    for (int i = 0; i < 1000; ++i)
        update();
    Check(probes.size() == 2 && probes[1]->deadlines.size() == 1 && loads == 2,
          "Before the lead deadline, dense ticks must not overwrite the first queued hit or reload files.");
    dspClock = 960;
    update();
    dspClock = 1200;
    update();
    Check(probes[1]->deadlines == std::vector<std::uint64_t>{960, 1200, 1440},
          "Dense ticks must reuse one native voice at their exact deadlines.");
    dspClock = 24960;
    update();
    Check(playback.Position() == Time{500'000}, "Playback time must derive from DSP samples.");
    playback.Pause();
    dspClock += 48000;
    update();
    Check(!playback.Active() && playback.Position() == Time{500'000} && musicProbe->playing && musicProbe->paused &&
              !probes[1]->playing,
          "Pause must retain paused music while stopping transient samples.");
    playback.Toggle();
    update();
    Check(probes.size() == 2 && musicProbe->settings.startPositionMicroseconds == 500'000 && loads == 2,
          "Resume must seek the same music voice without loading sources again.");
    Check(musicProbe->paused && musicProbe->settings.startDspClock == 0,
          "A requested source seek must remain paused without anchoring a stale DSP deadline.");
    loadState = AudioClipLoadState::Loading;
    dspClock += 48000;
    update();
    Check(playback.Active() && playback.Position() == Time{500'000} && musicProbe->paused,
          "Waiting for an async source seek must freeze the timeline and leave the prepared voice paused.");
    loadState = AudioClipLoadState::Playing;
    update();
    Check(!musicProbe->paused && musicProbe->settings.startDspClock == dspClock + 960,
          "A completed seek must anchor playback to the current DSP clock.");
    loadState = AudioClipLoadState::Ready;
    playback.Seek(Time{-100'000});
    update();
    update();
    Check(playback.Active() && musicProbe->settings.startPositionMicroseconds == 0 &&
              musicProbe->settings.startDspClock == dspClock + 5760,
          "Negative pre-roll must schedule the song at timeline zero.");
    playback.Seek(Time{900'000});
    update();
    update(); // The requested source seek must finish before the new DSP anchor.
    const auto anchor = dspClock + 960;
    dspClock = anchor + 48000; // A suspended UI skips a past one-second cue.
    update();
    Check(probes.size() == 2, "Returning after a stall must not burst expired hit sounds.");
    dspClock += 96000;
    update();
    Check(!playback.Active() && playback.Position() == Time{3'000'000},
          "Playback must stop at the later of music end and final long-note tail.");
    playback.Seek(Time{});
    bool rejected = false;
    try
    {
        playback.Seek(Time::min());
    }
    catch (const std::invalid_argument &)
    {
        rejected = true;
    }
    Check(rejected && playback.Position() == Time{}, "Invalid seeking must preserve the playback position.");
    playback.Toggle();
    failDuration = true;
    update();
    Check(!playback.Active() && playback.Error() == "duration probe failure",
          "Backend failures must stop with an error.");
    failDuration = false;
    playback.Shutdown();
    audio.Update();
    Check(std::ranges::none_of(probes, [](const auto &probe) { return probe->playing; }),
          "Shutdown must leave no editor voice running.");
}

void TestEditorPlaybackWithFmod(const std::filesystem::path &musicPath)
{
    AudioSystem audio;
    AudioConfig config;
    config.preferredBackend = AudioOutputBackend::NoSound;
    std::string error;
    Check(audio.Initialize(config, nullptr, nullptr, error), error.c_str());
    auto clip = audio.LoadSound(musicPath, AudioLoadMode::StreamAsyncAccurate, error);
    if (!clip)
        throw std::runtime_error(error);
    const auto waitFor = [&](const auto &ready)
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{10};
        while (!ready() && std::chrono::steady_clock::now() < deadline)
        {
            audio.Update();
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
        }
        Check(ready(), "Actual FMOD playback did not reach the expected state.");
    };
    waitFor([&] { return clip->LoadState(error) == AudioClipLoadState::Ready; });
    std::uint64_t duration = 0;
    Check(clip->GetDurationMicroseconds(duration, error) && duration > 1'000'000, "Actual music duration missing.");
    AudioPlaybackSettings settings;
    settings.startPaused = true;
    settings.startPositionMicroseconds = duration - 100'000;
    auto voice = clip->Play(settings, nullptr, error);
    if (!voice)
        throw std::runtime_error(error);
    waitFor([&] { return voice->PreparationState(error) == AudioPreparationState::Ready; });
    Check(voice->SetDelay(audio.DspClock() + 4800, 0, error) && voice->SetPaused(false, error), error.c_str());
    waitFor([&] { return !voice->IsPlaying(); }); // Near-end seek must end rather than play the entire song.
    voice.reset();
    waitFor([&] { return clip->LoadState(error) == AudioClipLoadState::Ready; });
    settings.startPositionMicroseconds = duration - 1;
    voice = clip->Play(settings, nullptr, error);
    Check(voice != nullptr, "A position one microsecond before the source end must be accepted.");
    waitFor([&] { return voice->PreparationState(error) == AudioPreparationState::Ready; });
    Check(voice->SetPaused(false, error), error.c_str());
    waitFor([&] { return !voice->IsPlaying(); });
    voice.reset();
    waitFor([&] { return clip->LoadState(error) == AudioClipLoadState::Ready; });
    settings.startPositionMicroseconds = duration + 1'000'000;
    Check(!clip->Play(settings, nullptr, error) && !error.empty(), "FMOD must reject a position beyond the source.");
    audio.RetireClip(std::move(clip));
    audio.Update();

    EditorPlayback playback;
    playback.Initialize(audio);
    const std::map<std::string, std::filesystem::path> files{{"Music", musicPath}};
    const auto update = [&]
    {
        playback.Update(files, 1, {}, 1, Time{});
        if (!playback.Error().empty())
            throw std::runtime_error(playback.Error());
    };
    for (int attempt = 0; attempt < 12; ++attempt)
    {
        constexpr std::array rates{.25, .5, 1.0, 1.5, 2.0};
        playback.SetRate(rates[static_cast<std::size_t>(attempt) % rates.size()]);
        // Cover ordinary Space pause/resume and seeks made while paused.
        if (attempt % 3 == 1)
            playback.Seek(Time{1'000'000});
        if (attempt % 3 == 2)
            playback.Seek(Time{-100'000});
        const auto start = playback.Position();
        playback.Toggle();
        waitFor([&] { update(); return playback.Position() >= start + Time{100'000}; });
        Check(playback.Active(), "Repeated actual MP3 playback must remain active after preparing the seek.");
        const auto beforeRateChange = playback.Position();
        playback.SetRate(playback.Rate() == 2 ? .5 : 2);
        Check(playback.Active() && playback.Position() >= beforeRateChange &&
                  playback.Position() - beforeRateChange < Time{100'000},
              "Actual FMOD live rate changes must preserve position without a new source seek.");
        playback.Toggle();
        const auto paused = playback.Position();
        update();
        Check(!playback.Active() && playback.Position() == paused, "Space pause must freeze the actual music cursor.");
    }
    playback.Seek(Time{static_cast<Time::rep>(duration - 300'000)});
    playback.Toggle();
    waitFor(
        [&]
        {
            update();
            return !playback.Active();
        });
    Check(playback.Position() == Time{static_cast<Time::rep>(duration)},
          "FMOD editor transport must stop at music end.");
    playback.Shutdown();
    audio.Update();
}
