#include "FingerDrumAssets.h"
#include "GameScene/RhythmTestScene/Submodules/GameplayPauseMenu.h"
#include "GameScene/RhythmTestScene/Submodules/GameplaySessionController.h"
#include "Taiko/Results/TaikoPlayResult.h"
#include "Taiko/TaikoMode.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <stdexcept>
#include <thread>

namespace
{
    namespace v = mrg::visual2d;
    namespace a = mrg::audio;
    namespace r = finger_drum::rhythm;
    namespace mode = finger_drum::mode;
    void Check(bool value, const char *message)
    {
        if (!value)
            throw std::runtime_error(message);
    }
    class NullRenderer final : public mrg::graphics::Visual2DRenderSystem
    {
      public:
        v::ImageHandle LoadImage(const std::filesystem::path &) override
        {
            return {};
        }
        v::Size GetImageSize(v::ImageHandle) const noexcept override
        {
            return {};
        }
        void SubmitScreen(const v::Visual2DCanvas &, const mrg::graphics::RenderContext &, v::Point,
                          std::uint32_t) override
        {
        }
        void SubmitPlane(const v::Visual2DCanvas &, const mrg::graphics::RenderContext &, const DirectX::XMFLOAT4X4 &,
                         v::Size, const DirectX::XMFLOAT4X4 &) override
        {
        }
        mrg::graphics::RenderTargetTextureHandle CreateCanvasRenderTarget(std::uint32_t, std::uint32_t) override
        {
            return {};
        }
        void RenderToTexture(const v::Visual2DCanvas &, const mrg::graphics::RenderTargetTextureHandle &,
                             const mrg::graphics::RenderContext &) override
        {
        }
    };
    const v::Visual2DNode *Find(const v::Visual2DNode &node, std::string_view name)
    {
        if (node.Name() == name)
            return &node;
        for (const auto &child : node.Children())
            if (const auto *found = Find(*child, name))
                return found;
        return nullptr;
    }
    void TestMenu()
    {
        NullRenderer renderer;
        v::ScreenVisual2DManager visuals;
        visuals.Initialize(renderer, {1280, 720});
        finger_drum::texts::TextCatalog texts;
        GameplayPauseMenu menu(visuals, texts);
        menu.Show(1280, 720, false, 12);
        Check(menu.HandleKey(VK_RETURN).action == GameplayPauseAction::None, "Preparation must block Resume.");
        menu.Show(1280, 720, true, 12);
        for (int i = 0; i < 20; ++i)
            static_cast<void>(menu.HandleKey(VK_RIGHT));
        auto choice = menu.HandleKey(VK_RETURN);
        Check(choice.action == GameplayPauseAction::Resume && choice.leadIn.count() == 3'000'000,
              "Resume must cap at three seconds.");
        for (int i = 0; i < 20; ++i)
            static_cast<void>(menu.HandleKey(VK_LEFT));
        Check(menu.HandleKey(VK_RETURN).leadIn.count() == 500'000, "Resume must retain the half-second minimum.");
        static_cast<void>(menu.HandleKey(VK_DOWN));
        for (int i = 0; i < 20; ++i)
            static_cast<void>(menu.HandleKey(VK_LEFT));
        Check(menu.HandleKey(VK_RETURN).action == GameplayPauseAction::Rewind &&
                  menu.HandleKey(VK_RETURN).measures == 1,
              "Rewind must use measure counts with a minimum of one.");
        auto *canvas = visuals.FindCanvas(1);
        if (!canvas)
            throw std::runtime_error("Pause Canvas missing.");
        const auto *retire = Find(canvas->Root(), "Pause.3");
        if (!retire)
            throw std::runtime_error("Retire row missing.");
        const auto b = retire->BoundsInCanvas();
        v::PointerInput p;
        p.available = true;
        p.position = {b.x + b.width / 2, b.y + b.height / 2};
        v::Visual2DInputRouter router;
        p.leftButtonDown = p.leftButtonPressed = true;
        router.Process(*canvas, p);
        p.leftButtonDown = p.leftButtonPressed = false;
        p.leftButtonReleased = true;
        router.Process(*canvas, p);
        router.Reset(*canvas);
        mrg::platform::InputState input;
        Check(menu.Update(input).action == GameplayPauseAction::Retire,
              "Mouse Retire must emit the same action as keyboard focus.");
        texts.SetLanguage(finger_drum::texts::Language::English);
        static_cast<void>(menu.Update(input));
        const auto packets = canvas->BuildDrawList();
        Check(std::ranges::any_of(packets,
                                  [](const auto &packet) { return packet.text && packet.text->value == L"Paused"; }),
              "Pause menu must use the current language.");
        menu.OnResize(720, 1280);
        menu.OnResize(2560, 1080);
        menu.Hide();
        Check(menu.HandleKey(VK_RETURN).action == GameplayPauseAction::None, "Hidden menus cannot accept keys.");
        menu.Shutdown();
        Check(visuals.CanvasCount() == 0, "Pause Canvas must be owned and released.");
    }
    std::vector<a::AudioPlaybackSettings> musicStarts;
    class Voice final : public a::IAudioVoiceBackend
    {
      public:
        bool IsPlaying() const noexcept override
        {
            return playing_;
        }
        bool Restart(const a::AudioPlaybackSettings &, a::IAudioBusBackend *, std::string &) override
        {
            return true;
        }
        bool Stop(std::string &) override
        {
            playing_ = false;
            return true;
        }
        bool SetPaused(bool, std::string &) override
        {
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
        bool SetDelay(std::uint64_t, std::uint64_t, std::string &) override
        {
            return true;
        }

      private:
        bool playing_{true};
    };
    class Clip final : public a::IAudioClipBackend
    {
      public:
        explicit Clip(bool music) : music_(music)
        {
        }
        a::AudioClipLoadState LoadState(std::string &) const override
        {
            return a::AudioClipLoadState::Ready;
        }
        bool GetDurationMicroseconds(std::uint64_t &duration, std::string &) const override
        {
            duration = 120'000'000;
            return true;
        }
        std::unique_ptr<a::IAudioVoiceBackend> Play(const a::AudioPlaybackSettings &settings, a::IAudioBusBackend *,
                                                    std::string &) override
        {
            if (music_)
                musicStarts.push_back(settings);
            return std::make_unique<Voice>();
        }

      private:
        bool music_;
    };
    std::unique_ptr<a::IAudioClipBackend> CreateClip(a::IAudioBackend &, const std::filesystem::path &path,
                                                     a::AudioLoadMode load, std::string &)
    {
        const bool music = path.filename() == "probe-music.mp3";
        if (music)
            Check(load == a::AudioLoadMode::StreamAsyncAccurate,
                  "Gameplay music must prepare accurate seek data once.");
        return std::make_unique<Clip>(music);
    }
    finger_drum::chart::PatternDocument Pattern()
    {
        namespace c = finger_drum::chart;
        c::PatternDocument p;
        p.mode = "Taiko";
        p.timing.push_back({{2, {}}, c::TimingDirectiveType::MeasureLength, 0, {3, 4}});
        p.timing.push_back({{3, {}}, c::TimingDirectiveType::Bpm, 240});
        p.timing.push_back({{4, {}}, c::TimingDirectiveType::MeasureLength, 0, {1, 1}});
        for (const std::int64_t i : {10, 11, 12})
            p.notes.push_back({.position = {i, {}}, .keyType = 1});
        return p;
    }
    class PauseClient final : public mrg::IGameClient
    {
      public:
        explicit PauseClient(bool &verified) : verified_(verified)
        {
        }
        mrg::EngineConfig GetEngineConfig() const override
        {
            mrg::EngineConfig c;
            c.showWindow = false;
            c.audio.preferredBackend = a::AudioOutputBackend::NoSound;
            c.audioClipBackendFactory = CreateClip;
            return c;
        }
        void Initialize(const mrg::EngineServices &services) override
        {
            services_.emplace(services);
            auto loaded = mode::TaikoMode{}.CreateSession(Pattern());
            Check(loaded.Succeeded(), "Transport fixture must load.");
            session_ = loaded.session.get();
            visuals_.Initialize(services.visual2DRendering, {1280, 720});
            controller_ = std::make_unique<GameplaySessionController>(
                finger_drum::GameplayLaunchRequest{"probe.ymp", {}, "probe-music.mp3", "Taiko"}, playback_, false,
                std::move(loaded.session));
            controller_->InitializeSession();
            controller_->Start(services, r::RhythmTime{12'240'000});
            menu_ = std::make_unique<GameplayPauseMenu>(visuals_, texts_);
        }
        bool Update(const mrg::UpdateContext &ctx) override
        {
            Check(ctx.totalSeconds < 10, "Pause transport timed out.");
            controller_->Update(ctx);
            if (!controller_->TimelineReady())
                return true;
            if (phase_ == 0 && controller_->Time().count() >= 12'250'000)
            {
                static_cast<void>(session_->ProcessInput('F', r::InputEdge::Pressed, controller_->Time()));
                controller_->OpenPauseMenu(ctx);
                paused_ = controller_->Time();
                phase_ = 1;
                rendered_ = false;
                menu_->Show(1280, 720, true, 13);
                return true;
            }
            if (phase_ == 1)
            {
                Check(controller_->Time() == paused_ && !session_->CanUseHp(),
                      "Pause must freeze the clock and latch practice.");
                if (++pausedUpdates_ < 20)
                    return true;
                Check(controller_->ResumeFromPause(ctx, r::RhythmDuration{500'000}), "Resume must succeed.");
                Check(controller_->Time() == paused_ - r::RhythmDuration{500'000} &&
                          session_->FinalizedNoteCount() == 1,
                      "Resume must move half a second without undoing the processed note.");
                Check(musicStarts.back().startPositionMicroseconds ==
                          static_cast<std::uint64_t>(controller_->Time().count()),
                      "Music source position must equal the resumed chart time.");
                controller_->OpenPauseMenu(ctx);
                const auto from = controller_->Time();
                Check(controller_->ResumeFromPause(ctx, r::RhythmDuration{3'000'000}) &&
                          controller_->Time() == from - r::RhythmDuration{3'000'000},
                      "The three-second Resume must move the same clock.");
                controller_->OpenPauseMenu(ctx);
                Check(controller_->RewindFromPause(ctx, 4) && controller_->Time() == r::RhythmTime{4'000'000} &&
                          session_->FinalizedNoteCount() == 0,
                      "Four source measures must ignore meter/BPM duration differences and restore grades.");
                Check(musicStarts.back().startPositionMicroseconds == 4'000'000,
                      "Rewind must seek music to the compiled measure start.");
                CheckConfirmedRewindExample(ctx);
                controller_->OpenPauseMenu(ctx);
                controller_->RetryFromPause(ctx);
                Check(session_->CanUseHp() && session_->FinalizedNoteCount() == 0,
                      "Retry must start a clean eligible attempt.");
                // Negative pre-roll must schedule source zero in the future,
                // and Rewind must never jump forwards after a long Resume.
                while (controller_->Time() > r::RhythmTime{3'000'000})
                {
                    controller_->OpenPauseMenu(ctx);
                    Check(controller_->ResumeFromPause(ctx, r::RhythmDuration{3'000'000}),
                          "Repeated Resume must succeed.");
                }
                controller_->OpenPauseMenu(ctx);
                const auto negativeFrom = controller_->Time();
                Check(controller_->ResumeFromPause(ctx, r::RhythmDuration{3'000'000}),
                      "Negative pre-roll Resume must succeed.");
                Check(controller_->Time() == negativeFrom - r::RhythmDuration{3'000'000} &&
                          musicStarts.back().startPositionMicroseconds == 0 &&
                          musicStarts.back().startDspClock > ctx.audio.DspClock(),
                      "Negative Resume must delay source zero to chart zero.");
                controller_->OpenPauseMenu(ctx);
                const auto earlier = controller_->Time();
                Check(controller_->RewindFromPause(ctx, 4) && controller_->Time() == earlier,
                      "Rewind before the first measure must not move the clock forwards.");
                controller_->OpenPauseMenu(ctx);
                controller_->RetryFromPause(ctx);
                controller_->OpenPauseMenu(ctx);
                controller_->RetireFromPause();
                auto result = controller_->TakeResult();
                const auto *taiko = dynamic_cast<const mode::TaikoPlayResult *>(result.get());
                if (!taiko)
                    throw std::runtime_error("Retirement result missing.");
                Check(taiko->summary.processedNotes == 3 && taiko->summary.unprocessedNotes == 0 &&
                          taiko->judgements[5] == 3 && !taiko->summary.IsCleared() && !taiko->summary.CanUseHp() &&
                          taiko->summary.controls.size() == 2,
                      "Retire must mark all remaining notes MISS and keep only the new attempt's controls.");
                phase_ = 2;
            }
            if (phase_ == 2)
            {
                // Render the actual modal once, then finish on the next update.
                if (rendered_)
                {
                    verified_ = true;
                    return false;
                }
            }
            return true;
        }
        void Render(const mrg::graphics::RenderContext &ctx) override
        {
            visuals_.Render(ctx);
            rendered_ = true;
        }
        void OnResize(std::uint32_t w, std::uint32_t h) override
        {
            visuals_.OnResize(w, h);
            if (menu_)
                menu_->OnResize(w, h);
        }
        void Shutdown() noexcept override
        {
            if (menu_)
                menu_->Shutdown();
            if (controller_)
                controller_->Shutdown();
            visuals_.Shutdown();
            playback_.StopAll();
        }

      private:
        void CheckConfirmedRewindExample(const mrg::UpdateContext &ctx)
        {
            auto loaded = mode::TaikoMode{}.CreateSession(Pattern());
            Check(loaded.Succeeded(), "Confirmed measure example must load.");
            const auto start = loaded.session->Timeline().Compile({9, {3, 4}});
            const auto expected = loaded.session->Timeline().Compile({5, {}});
            GameplaySessionController example(
                finger_drum::GameplayLaunchRequest{"probe.ymp", {}, "probe-music.mp3", "Taiko"}, playback_, false,
                std::move(loaded.session));
            example.InitializeSession();
            example.Start(*services_, start);
            example.Update(ctx);
            example.OpenPauseMenu(ctx);
            Check(example.RewindFromPause(ctx, 4) && example.Time() == expected,
                  "Measure 10 at 3/4 must rewind four measures to measure 6 at 0/1.");
        }
        bool &verified_;
        bool rendered_{};
        int phase_{}, pausedUpdates_{};
        r::RhythmTime paused_{};
        mode::PlaySession *session_{};
        std::optional<mrg::EngineServices> services_;
        a::AudioPlaybackManager playback_;
        v::ScreenVisual2DManager visuals_;
        finger_drum::texts::TextCatalog texts_;
        std::unique_ptr<GameplaySessionController> controller_;
        std::unique_ptr<GameplayPauseMenu> menu_;
    };
    void TestActualMusicSeek()
    {
        a::AudioSystem audio;
        a::AudioConfig config;
        config.preferredBackend = a::AudioOutputBackend::NoSound;
        std::string error;
        Check(audio.Initialize(config, nullptr, nullptr, error), error.c_str());
        a::AudioPlaybackManager playback;
        finger_drum::audio::GameplayAudioRouter router(playback);
        Check(router.Initialize(audio, error), error.c_str());
        const auto path =
            std::filesystem::path("FingerDrum.Assets/Assets/Songs/Music/Angeldream/angel dream hand shaking.mp3");
        Check(router.RegisterSound("Music.Track", path, a::AudioLoadMode::StreamAsyncAccurate, error), error.c_str());
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{10};
        while (!router.Ready() && std::chrono::steady_clock::now() < deadline)
        {
            audio.Update();
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
        }
        Check(router.Ready() && router.LastError().empty(), "Actual FMOD music must load.");
        auto clip = audio.LoadSound(path, a::AudioLoadMode::Stream, error);
        if (!clip)
            throw std::runtime_error(error);
        std::uint64_t duration{};
        Check(clip->GetDurationMicroseconds(duration, error), error.c_str());
        const r::RhythmTime nearEnd{static_cast<r::RhythmTime::rep>(duration - 100'000)};
        const auto clock = audio.CaptureClockSnapshot();
        r::RhythmTimer timer;
        timer.Start(clock.performanceCounterTicks, clock.performanceCounterFrequency, nearEnd);
        timer.AnchorDspClock(nearEnd, clock.dspClock, clock.sampleRate);
        Check(router.StartMusicAt(nearEnd, timer), "Actual FMOD near-end music seek must succeed.");
        const auto end = std::chrono::steady_clock::now() + std::chrono::seconds{2};
        while (playback.PlaybackCount() != 0 && std::chrono::steady_clock::now() < end)
        {
            audio.Update();
            playback.Update();
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
        }
        Check(playback.PlaybackCount() == 0,
              "A near-end seek must finish promptly instead of replaying the source from zero.");
        router.Shutdown();
        playback.StopAll();
        clip.reset();
        audio.Update();
        audio.Shutdown();
    }
} // namespace
int main()
{
    try
    {
        std::string error;
        Check(finger_drum::assets::InitializeBuiltInAssets(error), error.c_str());
        TestMenu();
        TestActualMusicSeek();
        bool verified{};
        Check(mrg::Run(std::make_unique<PauseClient>(verified)) == 0 && verified,
              "Native pause transport must complete.");
        std::puts("Pause menu, note restoration, eligibility, music source seeks and native rendering passed.");
        return 0;
    }
    catch (const std::exception &e)
    {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
