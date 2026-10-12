#include "EditorPlaybackTests.h"
#include "EditorScene/Submodules/EditorPlayback.h"
#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <thread>

namespace
{
    namespace a = mrg::audio;
    using Time = finger_drum::rhythm::RhythmTime;
    void Check(bool value, const char *message)
    {
        if (!value)
            throw std::runtime_error(message);
    }
    // A controllable DSP clock makes cue deadlines, pre-roll and stalled UI
    // intervals deterministic without depending on a physical audio driver.
    std::uint64_t dspClock{};
    class ClockBackend final : public a::IAudioBackend
    {
      public:
        bool Initialize(const a::AudioConfig &, std::string &) override
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
        a::AudioOutputBackend RequestedOutput() const noexcept override
        {
            return a::AudioOutputBackend::NoSound;
        }
        a::AudioOutputBackend ActiveOutput() const noexcept override
        {
            return RequestedOutput();
        }
        bool SetOutputBackend(a::AudioOutputBackend, std::string &) override
        {
            return false;
        }
        int DriverCount() const noexcept override
        {
            return 0;
        }
        const std::vector<a::AudioDeviceInfo> &OutputDrivers() const noexcept override
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
        std::unique_ptr<a::IAudioBusBackend> CreateBus(std::string_view, a::IAudioBusBackend *, std::string &) override
        {
            return {};
        }

      private:
        std::vector<a::AudioDeviceInfo> devices;
    };
    std::unique_ptr<a::IAudioBackend> CreateClockBackend()
    {
        return std::make_unique<ClockBackend>();
    }
    struct Probe
    {
        bool playing{true};
        bool paused{};
        std::filesystem::path path;
        a::AudioPlaybackSettings settings;
        std::vector<std::uint64_t> deadlines;
    };
    std::vector<std::shared_ptr<Probe>> probes;
    std::size_t loads{};
    bool failDuration{};
    a::AudioClipLoadState loadState{a::AudioClipLoadState::Ready};
    class Voice final : public a::IAudioVoiceBackend
    {
      public:
        explicit Voice(std::shared_ptr<Probe> value) : probe(std::move(value))
        {
        }
        bool IsPlaying() const noexcept override
        {
            return probe->playing;
        }
        bool Restart(const a::AudioPlaybackSettings &settings, a::IAudioBusBackend *, std::string &) override
        {
            probe->settings = settings;
            probe->deadlines.push_back(settings.startDspClock);
            return true;
        }
        bool Stop(std::string &) override
        {
            probe->playing = false;
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
        bool SetPitch(float, std::string &) override
        {
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
    class Clip final : public a::IAudioClipBackend
    {
      public:
        explicit Clip(std::filesystem::path value) : path(std::move(value))
        {
        }
        a::AudioClipLoadState LoadState(std::string &) const override
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
        std::unique_ptr<a::IAudioVoiceBackend> Play(const a::AudioPlaybackSettings &settings, a::IAudioBusBackend *,
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
    std::unique_ptr<a::IAudioClipBackend> CreateClip(a::IAudioBackend &, const std::filesystem::path &path,
                                                     a::AudioLoadMode mode, std::string &)
    {
        Check((path == "music") == (mode == a::AudioLoadMode::StreamAsyncAccurate),
              "Music must use accurate async loading; hit samples must be cached.");
        ++loads;
        return std::make_unique<Clip>(path);
    }
} // namespace

void TestEditorPlayback()
{
    dspClock = 0;
    probes.clear();
    loads = 0;
    a::AudioSystem audio;
    std::string error;
    Check(audio.Initialize({}, CreateClockBackend, CreateClip, error), "Probe audio initialization failed.");
    EditorPlayback playback;
    playback.Initialize(audio);
    const std::map<std::string, std::filesystem::path> files{{"Music", "music"}, {"Don", "don"}, {"Alias", "don"}};
    const std::vector<AudioMarker> cues{{0, "Don"}, {.005, "Alias"}, {.010, "Don"}, {1, "Don"}};
    const auto update = [&] { playback.Update(files, 1, cues, 1, Time{3'000'000}); };
    playback.Toggle();
    loadState = a::AudioClipLoadState::Loading;
    update();
    Check(playback.Active() && probes.empty() && loads == 2 && playback.Position() == Time{},
          "Loading must not advance time or allocate a voice; aliases must share a clip.");
    loadState = a::AudioClipLoadState::Ready;
    update();
    Check(probes.size() == 2 && probes[0]->settings.startDspClock == 960 && !probes[0]->paused &&
              probes[1]->deadlines == std::vector<std::uint64_t>{960},
          "Music and first hit must share the DSP anchor.");
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
    Check(!playback.Active() && playback.Position() == Time{500'000} && !probes[0]->playing,
          "Pause must freeze the cursor and stop only editor voices.");
    playback.Toggle();
    update();
    Check(probes.back()->path == "music" && probes.back()->settings.startPositionMicroseconds == 500'000 && loads == 2,
          "Resume must seek the music without loading sources again.");
    playback.Seek(Time{-100'000});
    update();
    Check(playback.Active() && probes.back()->settings.startPositionMicroseconds == 0 &&
              probes.back()->settings.startDspClock == dspClock + 5760,
          "Negative pre-roll must schedule the song at timeline zero.");
    playback.Seek(Time{900'000});
    update();
    const auto anchor = dspClock + 960;
    dspClock = anchor + 48000; // A suspended UI skips a past one-second cue.
    update();
    Check(probes.back()->path == "music", "Returning after a stall must not burst expired hit sounds.");
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
    a::AudioSystem audio;
    a::AudioConfig config;
    config.preferredBackend = a::AudioOutputBackend::NoSound;
    std::string error;
    Check(audio.Initialize(config, nullptr, nullptr, error), error.c_str());
    auto clip = audio.LoadSound(musicPath, a::AudioLoadMode::StreamAsyncAccurate, error);
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
    waitFor([&] { return clip->LoadState(error) == a::AudioClipLoadState::Ready; });
    std::uint64_t duration = 0;
    Check(clip->GetDurationMicroseconds(duration, error) && duration > 1'000'000, "Actual music duration missing.");
    a::AudioPlaybackSettings settings;
    settings.startPaused = true;
    settings.startPositionMicroseconds = duration - 100'000;
    auto voice = clip->Play(settings, nullptr, error);
    if (!voice)
        throw std::runtime_error(error);
    waitFor([&] { return clip->LoadState(error) != a::AudioClipLoadState::Loading; });
    Check(voice->SetDelay(audio.DspClock() + 4800, 0, error) && voice->SetPaused(false, error), error.c_str());
    waitFor([&] { return !voice->IsPlaying(); }); // Near-end seek must end rather than play the entire song.
    voice.reset();
    settings.startPositionMicroseconds = duration + 1'000'000;
    Check(!clip->Play(settings, nullptr, error) && !error.empty(), "FMOD must reject a position beyond the source.");
    audio.RetireClip(std::move(clip));
    audio.Update();

    EditorPlayback playback;
    playback.Initialize(audio);
    playback.Seek(Time{static_cast<Time::rep>(duration - 300'000)});
    playback.Toggle();
    const std::map<std::string, std::filesystem::path> files{{"Music", musicPath}};
    waitFor(
        [&]
        {
            playback.Update(files, 1, {}, 1, Time{});
            if (!playback.Error().empty())
                throw std::runtime_error(playback.Error());
            return !playback.Active();
        });
    Check(playback.Position() == Time{static_cast<Time::rep>(duration)},
          "FMOD editor transport must stop at music end.");
    playback.Shutdown();
    audio.Update();
}
