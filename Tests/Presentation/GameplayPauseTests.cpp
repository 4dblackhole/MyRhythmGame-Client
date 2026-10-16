#include "FingerDrumAssets.h"
#include "GameFlow/GameplaySessionController.h"
#include "GameScene/RhythmTestScene/Submodules/GameplayPauseMenu.h"
#include "Taiko/Results/TaikoPlayResult.h"
#include "Taiko/TaikoMode.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
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
        const auto *firstRow = Find(canvas->Root(), "Pause.Row.0");
        const auto *secondRow = Find(canvas->Root(), "Pause.Row.1");
        if (!firstRow || !secondRow)
            throw std::runtime_error("The Penpot action rows must exist.");
        const auto firstBounds = firstRow->BoundsInCanvas(), secondBounds = secondRow->BoundsInCanvas();
        Check(std::abs(firstBounds.width - 480) < .001F && std::abs(firstBounds.height - 48) < .001F &&
                  std::abs(firstBounds.y - secondBounds.y - 88.0F * 2 / 3) < .001F,
              "Pause rows must match Penpot's 720x72px dimensions and 16px gaps at 1080p.");
        const auto clickSelector = [&](std::string_view name)
        {
            const auto *button = Find(canvas->Root(), name);
            if (!button)
                throw std::runtime_error("Selector button must exist.");
            const auto bounds = button->BoundsInCanvas();
            v::PointerInput pointer;
            pointer.available = true;
            pointer.position = {bounds.x + bounds.width / 2, bounds.y + bounds.height / 2};
            pointer.leftButtonDown = pointer.leftButtonPressed = true;
            v::Visual2DInputRouter clicks;
            clicks.Process(*canvas, pointer);
            menu.Show(1280, 720, true, 12);
            pointer.leftButtonDown = pointer.leftButtonPressed = false;
            pointer.leftButtonReleased = true;
            clicks.Process(*canvas, pointer);
            clicks.Reset(*canvas);
            return menu.Update({});
        };
        auto adjusted = clickSelector("Pause.0.Next");
        Check(adjusted.action == GameplayPauseAction::None && adjusted.leadIn.count() == 1'000'000,
              "A Resume selector click must adjust the value without resuming the game.");
        adjusted = clickSelector("Pause.1.Next");
        Check(adjusted.action == GameplayPauseAction::None && adjusted.measures == 2,
              "A Rewind selector click must adjust the value without rewinding the game.");
        adjusted = clickSelector("Pause.1.Previous");
        Check(adjusted.action == GameplayPauseAction::None && adjusted.measures == 1,
              "The previous selector must apply the same lower bound as keyboard adjustment.");
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
        for (const auto [row, action] :
             {std::pair{0, GameplayPauseAction::Resume}, std::pair{2, GameplayPauseAction::Retry}})
        {
            const auto *button = Find(canvas->Root(), "Pause." + std::to_string(row));
            if (!button)
                throw std::runtime_error("Pause action hit target must exist.");
            const auto bounds = button->BoundsInCanvas();
            p = {};
            p.available = true;
            p.position = {bounds.x + bounds.width / 2, bounds.y + bounds.height / 2};
            p.leftButtonDown = p.leftButtonPressed = true;
            router.Process(*canvas, p);
            // Repeated Show while held must preserve the click capture.
            menu.Show(1280, 720, true, 12);
            p.leftButtonDown = p.leftButtonPressed = false;
            p.leftButtonReleased = true;
            router.Process(*canvas, p);
            Check(menu.Update(input).action == action, "A button release must immediately emit its action.");
            router.Reset(*canvas);
        }
        texts.SetLanguage(finger_drum::texts::Language::English);
        static_cast<void>(menu.Update(input));
        const auto packets = canvas->BuildDrawList();
        Check(std::ranges::any_of(packets,
                                  [](const auto &packet) { return packet.text && packet.text->value == L"Resume"; }),
              "Pause menu must use the current language.");
        menu.OnResize(720, 1280);
        const auto *panel = Find(canvas->Root(), "Pause.Panel");
        Check(panel != nullptr && panel->BoundsInCanvas().width <= canvas->LogicalSize().width - 31.9F,
              "The complete menu, including selectors, must fit narrow viewports.");
        menu.OnResize(2560, 1080);
        menu.Hide();
        Check(menu.HandleKey(VK_RETURN).action == GameplayPauseAction::None, "Hidden menus cannot accept keys.");
        menu.Shutdown();
        Check(visuals.CanvasCount() == 0, "Pause Canvas must be owned and released.");
    }
    std::vector<a::AudioPlaybackSettings> musicStarts;
    std::vector<std::uint64_t> musicSchedules;
    std::int64_t musicScheduleTicks{};
    std::size_t sampleStarts{}, clipLoads{}, musicResumes{};
    bool failNextMusicStart{};
    struct ClipState
    {
        bool music{};
        std::chrono::steady_clock::time_point readyAt{};
    };
    void WaitForGameplay(IPlaySessionController &controller, const mrg::UpdateContext &ctx)
    {
        const auto target = controller.Time();
        const auto start = std::chrono::steady_clock::now();
        mrg::platform::InputState empty;
        while (controller.CurrentState() != PlayState::Gameplay)
        {
            const auto elapsed = std::chrono::steady_clock::now() - start;
            Check(elapsed < std::chrono::seconds{2}, "Prepared music did not enter Gameplay.");
            Check(controller.Time() == target, "Preparing audio must not advance the gameplay clock.");
            ctx.audio.Update();
            controller.Update({0.001,
                               ctx.totalSeconds + std::chrono::duration<double>(elapsed).count(),
                               ctx.updateIndex,
                               empty,
                               ctx.audio,
                               {}});
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
        }
    }
    class Voice final : public a::IAudioVoiceBackend
    {
      public:
        explicit Voice(std::shared_ptr<ClipState> state) : state_(std::move(state))
        {
        }
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
        bool SetPaused(bool paused, std::string &) override
        {
            if (!paused && state_->music)
            {
                Check(std::chrono::steady_clock::now() >= state_->readyAt,
                      "Music must finish its async seek before being unpaused.");
                ++musicResumes;
            }
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
            if (state_->music)
            {
                musicSchedules.push_back(start);
                LARGE_INTEGER counter{};
                QueryPerformanceCounter(&counter);
                musicScheduleTicks = counter.QuadPart;
            }
            return true;
        }

      private:
        bool playing_{true};
        std::shared_ptr<ClipState> state_;
    };
    class Clip final : public a::IAudioClipBackend
    {
      public:
        explicit Clip(bool music) : state_(std::make_shared<ClipState>())
        {
            state_->music = music;
            ++clipLoads;
        }
        a::AudioClipLoadState LoadState(std::string &) const override
        {
            return std::chrono::steady_clock::now() < state_->readyAt ? a::AudioClipLoadState::Loading
                                                                      : a::AudioClipLoadState::Ready;
        }
        bool GetDurationMicroseconds(std::uint64_t &duration, std::string &) const override
        {
            duration = 120'000'000;
            return true;
        }
        std::unique_ptr<a::IAudioVoiceBackend> Play(const a::AudioPlaybackSettings &settings, a::IAudioBusBackend *,
                                                    std::string &error) override
        {
            if (state_->music)
            {
                if (failNextMusicStart)
                {
                    failNextMusicStart = false;
                    error = "Injected music preparation failure.";
                    return nullptr;
                }
                Check(settings.startPaused && settings.startDspClock == 0,
                      "Music seeks must create a paused voice without a stale DSP deadline.");
                musicStarts.push_back(settings);
                state_->readyAt = std::chrono::steady_clock::now() + std::chrono::milliseconds{40};
            }
            else
                ++sampleStarts;
            return std::make_unique<Voice>(state_);
        }

      private:
        std::shared_ptr<ClipState> state_;
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
    class ProbeResult final : public mode::IPlayResult
    {
      public:
        mode::PlayResultSummary summary;
        std::string_view ModeId() const noexcept override
        {
            return "Probe";
        }
        const mode::PlayResultSummary &Summary() const noexcept override
        {
            return summary;
        }
    };
    class ProbeCollector final : public mode::IPlayResultCollector
    {
      public:
        void RecordInput(const r::RhythmInputEvent &) override
        {
        }
        void RecordControl(mode::PlayControlEvent event) override
        {
            summary_.controls.push_back(event);
        }
        void Consume(const r::NoteProcessResult &, bool) override
        {
        }
        void RecordRetirement(const r::NoteAccuracy &) override
        {
        }
        void SetInitialTime(r::RhythmTime time) noexcept override
        {
            summary_.initialTime = time;
        }
        void Reset() noexcept override
        {
            summary_ = {};
        }
        void ResetJudgements() noexcept override
        {
        }
        std::unique_ptr<mode::IPlayResult> TakeResult(mode::PlayEndReason reason, r::RhythmTime time) override
        {
            auto result = std::make_unique<ProbeResult>();
            result->summary = summary_;
            result->summary.endReason = reason;
            result->summary.endTime = time;
            return result;
        }

      private:
        mode::PlayResultSummary summary_;
    };
    // A second mode exists only in this test. It supplies no Taiko note, sound
    // or result implementation, and uses exactly the same controller/interface.
    class ProbeMode final : public mode::IPlayGameMode
    {
      public:
        mode::ModeInputLayout InputLayout() const override
        {
            return {"Probe", {}};
        }
        std::string_view Id() const noexcept override
        {
            return "Probe";
        }
        mode::ModeLoadResult LoadSession(const std::filesystem::path &,
                                         const std::optional<std::filesystem::path> &) const override
        {
            return CreateSession({}, {});
        }
        mode::ModeLoadResult CreateSession(const finger_drum::chart::PatternDocument &pattern,
                                           const finger_drum::chart::EffectDocument &) const override
        {
            auto session = std::make_unique<mode::PlaySession>();
            session->SetInputLayout(InputLayout());
            session->SetTimeline(finger_drum::chart::MusicalTimeline(pattern));
            session->Gear().CreateLane();
            session->Gear().Finalize();
            session->SetResultCollector(std::make_unique<ProbeCollector>());
            return {std::move(session), {}};
        }
    };
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
            if (phase_ == 0 && controller_->CurrentState() != PlayState::Gameplay)
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
                Check(controller_->CurrentState() == PlayState::Pause && !controller_->PauseMenuOpen(),
                      "Resume must accept the action while the seek keeps gameplay paused.");
                Check(!controller_->ResumeFromPause(ctx, r::RhythmDuration{500'000}),
                      "A duplicate Resume must not subtract the lead-in again.");
                const auto samplesBefore = sampleStarts;
                WaitForGameplay(*controller_, ctx);
                Check(sampleStarts == samplesBefore, "Resume must not replay hit sounds.");
                CheckClockAlignment(ctx, paused_ - r::RhythmDuration{500'000});
                Check(musicStarts.back().startPositionMicroseconds ==
                          static_cast<std::uint64_t>((paused_ - r::RhythmDuration{500'000}).count()),
                      "Music source position must equal the resumed chart time.");
                controller_->OpenPauseMenu(ctx);
                const auto from = controller_->Time();
                Check(controller_->ResumeFromPause(ctx, r::RhythmDuration{3'000'000}) &&
                          controller_->Time() == from - r::RhythmDuration{3'000'000},
                      "The three-second Resume must move the same clock.");
                WaitForGameplay(*controller_, ctx);
                controller_->OpenPauseMenu(ctx);
                Check(controller_->RewindFromPause(ctx, 4) && controller_->Time() == r::RhythmTime{4'000'000} &&
                          session_->FinalizedNoteCount() == 0,
                      "Four source measures must ignore meter/BPM duration differences and restore grades.");
                WaitForGameplay(*controller_, ctx);
                Check(musicStarts.back().startPositionMicroseconds == 4'000'000,
                      "Rewind must seek music to the compiled measure start.");
                CheckConfirmedRewindExample(ctx);
                CheckSharedLifecycle(ctx);
                CheckTransientVoices(ctx);
                controller_->OpenPauseMenu(ctx);
                controller_->RetryFromPause(ctx);
                Check(session_->CanUseHp() && session_->FinalizedNoteCount() == 0,
                      "Retry must start a clean eligible attempt.");
                Check(controller_->CurrentState() == PlayState::Loading, "Retry must re-enter Loading.");
                const auto loadsBefore = clipLoads;
                WaitForGameplay(*controller_, ctx);
                Check(clipLoads == loadsBefore, "Retry must reuse the loaded clips.");
                // Negative pre-roll must schedule source zero in the future,
                // and Rewind must never jump forwards after a long Resume.
                while (controller_->Time() > r::RhythmTime{3'000'000})
                {
                    controller_->OpenPauseMenu(ctx);
                    Check(controller_->ResumeFromPause(ctx, r::RhythmDuration{3'000'000}),
                          "Repeated Resume must succeed.");
                    WaitForGameplay(*controller_, ctx);
                }
                controller_->OpenPauseMenu(ctx);
                const auto negativeFrom = controller_->Time();
                Check(controller_->ResumeFromPause(ctx, r::RhythmDuration{3'000'000}),
                      "Negative pre-roll Resume must succeed.");
                Check(controller_->Time() == negativeFrom - r::RhythmDuration{3'000'000} &&
                          musicStarts.back().startPositionMicroseconds == 0 &&
                          controller_->CurrentState() == PlayState::Pause,
                      "Negative Resume must preserve source zero while preparing.");
                WaitForGameplay(*controller_, ctx);
                Check(musicSchedules.back() > ctx.audio.DspClock(),
                      "Negative Resume must schedule source zero at future chart zero.");
                controller_->OpenPauseMenu(ctx);
                const auto earlier = controller_->Time();
                Check(controller_->RewindFromPause(ctx, 4) && controller_->Time() == earlier,
                      "Rewind before the first measure must not move the clock forwards.");
                controller_->OpenPauseMenu(ctx);
                controller_->RetryFromPause(ctx);
                WaitForGameplay(*controller_, ctx);
                controller_->OpenPauseMenu(ctx);
                controller_->RetireFromPause();
                Check(controller_->CurrentState() == PlayState::Ending && controller_->ShouldShowResult(),
                      "Retire must enter Ending and prepare one owned result.");
                auto result = controller_->TakeResult();
                Check(!controller_->TakeResult() && !controller_->ShouldShowResult(),
                      "Result ownership must be transferred only once.");
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
        void CheckTransientVoices(const mrg::UpdateContext &ctx)
        {
            finger_drum::audio::GameplayAudioRouter router(playback_);
            std::string error;
            Check(router.Initialize(ctx.audio, error), "The audio resume fixture must initialize.");
            Check(router.RegisterSound("Probe.Hit", "probe-hit.wav", a::AudioLoadMode::SampleAsync, error),
                  "The transient sample must load.");
            Check(router.RegisterSound("Music.Track", "probe-music.mp3", a::AudioLoadMode::StreamAsyncAccurate, error),
                  "The background music must load.");
            const auto voicesBefore = playback_.PlaybackCount();
            const std::array<r::AudioCueRequest, 1> cue{{{"Probe.Hit"}}};
            router.PlayNow(cue);
            Check(playback_.PlaybackCount() == voicesBefore + 1, "The old hit sound must actually be playing.");
            const auto samplesBefore = sampleStarts;
            router.StopAllVoices();
            Check(playback_.PlaybackCount() == voicesBefore, "Pause must discard the old hit sound.");
            using Preparation = finger_drum::audio::GameplayAudioRouter::MusicPreparation;
            auto ready = router.PrepareMusicAt(r::RhythmTime{500'000});
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{2};
            while (ready == Preparation::Loading && std::chrono::steady_clock::now() < deadline)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds{1});
                ready = router.PrepareMusicAt(r::RhythmTime{500'000});
            }
            Check(ready == Preparation::Ready, "Only background music must finish seek preparation.");
            const auto clock = ctx.audio.CaptureClockSnapshot();
            r::RhythmTimer timer;
            timer.Start(clock.performanceCounterTicks, clock.performanceCounterFrequency, r::RhythmTime{500'000});
            timer.AnchorDspClock(r::RhythmTime{500'000}, clock.dspClock, clock.sampleRate);
            Check(router.StartPreparedMusic(r::RhythmTime{500'000}, timer) && sampleStarts == samplesBefore &&
                      playback_.PlaybackCount() == voicesBefore + 1,
                  "Resume must recreate background music without replaying the discarded hit sound.");
            router.StopAllVoices();
            const auto startsBeforeEnd = musicStarts.size();
            Check(router.PrepareMusicAt(r::RhythmTime{120'000'000}) == Preparation::Ready &&
                      router.StartPreparedMusic(r::RhythmTime{120'000'000}, timer) &&
                      musicStarts.size() == startsBeforeEnd && playback_.PlaybackCount() == voicesBefore,
                  "A chart beyond the music's end must resume without creating an out-of-range voice.");
        }
        void CheckClockAlignment(const mrg::UpdateContext &ctx, r::RhythmTime target)
        {
            const auto before = ctx.audio.CaptureClockSnapshot();
            mrg::platform::InputState empty;
            controller_->Update({0, ctx.totalSeconds + 1, ctx.updateIndex, empty, ctx.audio, {}});
            const auto after = ctx.audio.CaptureClockSnapshot();
            const auto startTicks = musicScheduleTicks + before.performanceCounterFrequency / 50;
            const auto elapsed = [&](std::int64_t ticks)
            {
                return r::RhythmDuration{static_cast<std::int64_t>(static_cast<long double>(ticks - startTicks) *
                                                                   1'000'000 / before.performanceCounterFrequency)};
            };
            Check(controller_->Time() >= target + elapsed(before.performanceCounterTicks) - r::RhythmDuration{5'000} &&
                      controller_->Time() <= target + elapsed(after.performanceCounterTicks) + r::RhythmDuration{5'000},
                  "The DSP scheduling lead must also be reflected in the QPC gameplay epoch.");
        }
        void CheckSharedLifecycle(const mrg::UpdateContext &ctx)
        {
            auto created = ProbeMode{}.CreateSession({}, {});
            std::unique_ptr<IPlaySessionController> common = std::make_unique<GameplaySessionController>(
                finger_drum::GameplayLaunchRequest{"probe.ymp", {}, "probe-music.mp3", "Probe"}, playback_, false,
                std::move(created.session));
            common->InitializeSession();
            Check(common->CurrentState() == PlayState::Loading, "Every mode must start in Loading.");
            common->OpenPauseMenu(ctx);
            Check(common->CurrentState() == PlayState::Pause && !common->CanContinue(),
                  "ESC before asset loading must pause without enabling Resume.");
            common->Start(*services_, r::RhythmTime{1'000'000});
            common->Update(ctx);
            Check(common->CanContinue(), "Paused loading must finish without starting the timer.");
            failNextMusicStart = true;
            Check(common->ResumeFromPause(ctx, r::RhythmDuration{500'000}), "A seek request must be accepted.");
            Check(common->CurrentState() == PlayState::Pause && common->PauseMenuOpen() &&
                      common->Time() == r::RhythmTime{500'000} && !common->ErrorMessage().empty(),
                  "Audio failure must keep a frozen clock and expose the pause menu/error.");
            common->RetryFromPause(ctx);
            Check(common->CurrentState() == PlayState::Loading, "Retry must use Loading for every mode.");
            WaitForGameplay(*common, ctx);
            Check(common->ErrorMessage().empty(), "A successful Retry must clear the resolved music error.");
            mrg::platform::InputState empty;
            common->Update({3, ctx.totalSeconds + 3, ctx.updateIndex, empty, ctx.audio, {}});
            Check(common->CurrentState() == PlayState::Ending && common->ShouldShowResult(),
                  "A different mode must complete through the common Ending state.");
            auto result = common->TakeResult();
            Check(result && result->ModeId() == "Probe" &&
                      result->Summary().endReason == mode::PlayEndReason::Completed,
                  "Ending must preserve a mode-specific polymorphic result.");
            common->Update(ctx);
            Check(!common->ShouldShowResult() && !common->TakeResult(), "Ending must never generate a second result.");
        }
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
            WaitForGameplay(example, ctx);
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
        using Preparation = finger_drum::audio::GameplayAudioRouter::MusicPreparation;
        for (const auto position : {r::RhythmTime{1'000'000}, r::RhythmTime{}, nearEnd})
        {
            router.StopAllVoices();
            auto prepared = router.PrepareMusicAt(position);
            while (prepared == Preparation::Loading && std::chrono::steady_clock::now() < deadline)
            {
                audio.Update();
                playback.Update();
                std::this_thread::sleep_for(std::chrono::milliseconds{1});
                prepared = router.PrepareMusicAt(position);
            }
            Check(prepared == Preparation::Ready, "Actual MP3 seek must finish before scheduling playback.");
            auto clock = audio.CaptureClockSnapshot();
            clock.dspClock += static_cast<std::uint64_t>(clock.sampleRate) / 50;
            r::RhythmTimer timer;
            timer.Start(clock.performanceCounterTicks, clock.performanceCounterFrequency, position);
            timer.AnchorDspClock(position, clock.dspClock, clock.sampleRate);
            Check(router.StartPreparedMusic(position, timer), "Actual FMOD repeated seek must succeed.");
        }
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
