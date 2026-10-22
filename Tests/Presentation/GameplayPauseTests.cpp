#include "YOUMUAssets.h"
#include "GameFlow/GameplaySessionController.h"
#include "GameScene/RhythmTestScene/Submodules/GameplayPauseMenu.h"
#include "GameScene/RhythmTestScene/RhythmTestScene.h"
#include "Taiko/Results/TaikoPlayResult.h"
#include "Taiko/TaikoMode.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <thread>

struct InputStateTestAccess
{
    static void Begin(InputState &input) { input.BeginUpdate(); }
    static void Press(InputState &input, std::uint16_t key, std::int64_t ticks) { input.SetKey(key, true, ticks); }
};

namespace
{

    void Check(bool value, const char *message)
    {
        if (!value)
        {
            std::fprintf(stderr, "CHECK FAILED: %s\n", message);
            throw std::runtime_error(message);
        }
    }
    class NullRenderer final : public Visual2DRenderSystem
    {
      public:
        ImageHandle LoadImage(const std::filesystem::path &) override
        {
            return {};
        }
        Size GetImageSize(ImageHandle) const noexcept override
        {
            return {};
        }
        void SubmitScreen(const Visual2DCanvas &, const RenderContext &, Point,
                          std::uint32_t) override
        {
        }
        void SubmitPlane(const Visual2DCanvas &, const RenderContext &, const DirectX::XMFLOAT4X4 &,
                         Size, const DirectX::XMFLOAT4X4 &) override
        {
        }
        RenderTargetTextureHandle CreateCanvasRenderTarget(std::uint32_t, std::uint32_t) override
        {
            return {};
        }
        void RenderToTexture(const Visual2DCanvas &, const RenderTargetTextureHandle &,
                             const RenderContext &) override
        {
        }
    };
    const Visual2DNode *Find(const Visual2DNode &node, std::string_view name)
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
        ScreenVisual2DManager visuals;
        visuals.Initialize(renderer, {1280, 720});
        TextCatalog texts;
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
            PointerInput pointer;
            pointer.available = true;
            pointer.position = {bounds.x + bounds.width / 2, bounds.y + bounds.height / 2};
            pointer.leftButtonDown = pointer.leftButtonPressed = true;
            Visual2DInputRouter clicks;
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
        PointerInput p;
        p.available = true;
        p.position = {b.x + b.width / 2, b.y + b.height / 2};
        Visual2DInputRouter router;
        p.leftButtonDown = p.leftButtonPressed = true;
        router.Process(*canvas, p);
        p.leftButtonDown = p.leftButtonPressed = false;
        p.leftButtonReleased = true;
        router.Process(*canvas, p);
        router.Reset(*canvas);
        InputState input;
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
        texts.SetLanguage(Language::English);
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
    std::vector<AudioPlaybackSettings> musicStarts;
    std::vector<std::uint64_t> musicSchedules;
    std::int64_t musicScheduleTicks{};
    std::size_t sampleStarts{}, clipLoads{}, musicResumes{};
    bool failNextMusicStart{};
    bool failSampleLoads{};
    int sampleDelayMs{};
    std::chrono::steady_clock::time_point samplesReadyAt{};
    struct ClipState
    {
        bool music{};
        std::chrono::steady_clock::time_point readyAt{};
    };
    void WaitForGameplay(IPlaySessionController &controller, const UpdateContext &ctx)
    {
        const auto target = controller.Time();
        const auto start = std::chrono::steady_clock::now();
        InputState empty;
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
    class Voice final : public IAudioVoiceBackend
    {
      public:
        explicit Voice(std::shared_ptr<ClipState> state) : state_(std::move(state))
        {
        }
        bool IsPlaying() const noexcept override
        {
            return playing_;
        }
        AudioPreparationState PreparationState(std::string &error) override
        {
            error.clear();
            return std::chrono::steady_clock::now() < state_->readyAt
                       ? AudioPreparationState::Loading : AudioPreparationState::Ready;
        }
        bool Restart(const AudioPlaybackSettings &settings, IAudioBusBackend *, std::string &error) override
        {
            if (state_->music)
            {
                if (failNextMusicStart)
                {
                    failNextMusicStart = false;
                    error = "Injected music preparation failure.";
                    return false;
                }
                Check(std::chrono::steady_clock::now() >= state_->readyAt && settings.startPaused &&
                          settings.startDspClock == 0,
                      "The target seek must wait for Play's internal seek and retain its paused voice.");
                musicStarts.back() = settings;
                state_->readyAt = std::chrono::steady_clock::now() + std::chrono::milliseconds{40};
            }
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
    class Clip final : public IAudioClipBackend
    {
      public:
        explicit Clip(bool music) : state_(std::make_shared<ClipState>())
        {
            state_->music = music;
            if (!music && sampleDelayMs)
                samplesReadyAt = state_->readyAt = std::chrono::steady_clock::now() + std::chrono::milliseconds{sampleDelayMs};
            ++clipLoads;
        }
        AudioClipLoadState LoadState(std::string &error) const override
        {
            if (!state_->music && failSampleLoads)
            {
                error = "Injected hit sound load failure.";
                return AudioClipLoadState::Failed;
            }
            error.clear();
            return std::chrono::steady_clock::now() < state_->readyAt ? AudioClipLoadState::Loading
                                                                      : AudioClipLoadState::Ready;
        }
        bool GetDurationMicroseconds(std::uint64_t &duration, std::string &) const override
        {
            duration = 120'000'000;
            return true;
        }
        std::unique_ptr<IAudioVoiceBackend> Play(const AudioPlaybackSettings &settings, IAudioBusBackend *,
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
                      "Music must stay inaudible until source preparation completes.");
                musicStarts.push_back(settings);
                state_->readyAt = std::chrono::steady_clock::now() + std::chrono::milliseconds{40};
            }
            else
            {
                Check(std::chrono::steady_clock::now() >= state_->readyAt,
                      "Input samples must be ready before their first playback request.");
                ++sampleStarts;
            }
            return std::make_unique<Voice>(state_);
        }

      private:
        std::shared_ptr<ClipState> state_;
    };

    std::unique_ptr<IAudioClipBackend> CreateClip(IAudioBackend &, const std::filesystem::path &path,
                                                     AudioLoadMode load, std::string &)
    {
        const bool music = path.filename() == "probe-music.mp3";
        if (music)
            Check(load == AudioLoadMode::StreamAsyncAccurate,
                  "Gameplay music must prepare accurate seek data once.");
        return std::make_unique<Clip>(music);
    }
    PatternDocument Pattern()
    {

        PatternDocument p;
        p.mode = "Taiko";
        p.timing.push_back({{2, {}}, TimingDirectiveType::MeasureLength, 0, {3, 4}});
        p.timing.push_back({{3, {}}, TimingDirectiveType::Bpm, 240});
        p.timing.push_back({{4, {}}, TimingDirectiveType::MeasureLength, 0, {1, 1}});
        for (const std::int64_t i : {10, 11, 12})
            p.notes.push_back({.position = {i, {}}, .keyType = 1});
        return p;
    }
    class ProbeResult final : public IPlayResult
    {
      public:
        PlayResultSummary summary;
        std::string_view ModeId() const noexcept override
        {
            return "Probe";
        }
        const PlayResultSummary &Summary() const noexcept override
        {
            return summary;
        }
    };
    class ProbeCollector final : public IPlayResultCollector
    {
      public:
        void RecordInput(const RhythmInputEvent &) override
        {
        }
        void RecordControl(PlayControlEvent event) override
        {
            summary_.controls.push_back(event);
        }
        void Consume(const NoteProcessResult &, bool) override
        {
        }
        void RecordRetirement(const NoteAccuracy &) override
        {
        }
        void SetInitialTime(RhythmTime time) noexcept override
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
        std::unique_ptr<IPlayResult> TakeResult(PlayEndReason reason, RhythmTime time) override
        {
            auto result = std::make_unique<ProbeResult>();
            result->summary = summary_;
            result->summary.endReason = reason;
            result->summary.endTime = time;
            return result;
        }

      private:
        PlayResultSummary summary_;
    };
    // A second mode exists only in this test. It supplies no Taiko note, sound
    // or result implementation, and uses exactly the same controller/interface.
    class ProbeMode final : public IPlayGameMode
    {
      public:
        ModeInputLayout InputLayout() const override
        {
            return {"Probe", {}};
        }
        std::string_view Id() const noexcept override
        {
            return "Probe";
        }
        ModeLoadResult LoadSession(const std::filesystem::path &,
                                         const std::optional<std::filesystem::path> &) const override
        {
            return CreateSession({}, {});
        }
        ModeLoadResult CreateSession(const PatternDocument &pattern,
                                           const EffectDocument &) const override
        {
            auto session = std::make_unique<PlaySession>();
            session->SetInputLayout(InputLayout());
            session->SetTimeline(MusicalTimeline(pattern));
            session->Gear().CreateLane();
            session->Gear().Finalize();
            session->SetResultCollector(std::make_unique<ProbeCollector>());
            return {std::move(session), {}};
        }
    };
    class ColdStartClient final : public IGameClient
    {
        class Rendering final : public Visual2DRenderSystem
        {
          public:
            Rendering(Visual2DRenderSystem &inner, bool &shown)
                : inner_(inner), shown_(shown), resumesBefore_(musicResumes) {}
            ImageHandle LoadImage(const std::filesystem::path &path) override { return inner_.LoadImage(path); }
            bool PrepareImages(std::span<const std::filesystem::path> paths, std::string &error) override
            { return inner_.PrepareImages(paths, error); }
            Size GetImageSize(ImageHandle image) const noexcept override { return inner_.GetImageSize(image); }
            void SubmitScreen(const Visual2DCanvas &canvas, const RenderContext &ctx, Point origin, std::uint32_t z) override
            {
                Check(std::chrono::steady_clock::now() >= samplesReadyAt && musicResumes > resumesBefore_,
                      "The first playable screen must wait for cold samples and the scheduled music start.");
                shown_ = true;
                inner_.SubmitScreen(canvas, ctx, origin, z);
            }
            void SubmitPlane(const Visual2DCanvas &canvas, const RenderContext &ctx, const DirectX::XMFLOAT4X4 &world,
                             Size size, const DirectX::XMFLOAT4X4 &view) override
            { inner_.SubmitPlane(canvas, ctx, world, size, view); }
            RenderTargetTextureHandle CreateCanvasRenderTarget(std::uint32_t w, std::uint32_t h) override
            { return inner_.CreateCanvasRenderTarget(w, h); }
            void RenderToTexture(const Visual2DCanvas &canvas, const RenderTargetTextureHandle &target,
                                 const RenderContext &ctx) override { inner_.RenderToTexture(canvas, target, ctx); }
          private:
            Visual2DRenderSystem &inner_;
            bool &shown_;
            std::size_t resumesBefore_{};
        };
      public:
        explicit ColdStartClient(bool &verified) : verified_(verified) {}
        EngineConfig GetEngineConfig() const override
        {
            EngineConfig c;
            c.showWindow = false;
            c.audio.preferredBackend = AudioOutputBackend::NoSound;
            c.audioClipBackendFactory = CreateClip;
            return c;
        }
        void Initialize(const EngineServices &services) override
        {
            rendering_ = std::make_unique<Rendering>(services.visual2DRendering, shown_);
            visuals_.Initialize(*rendering_, {1280, 720});
            auto store = std::make_shared<GameplayLaunchStore>();
            auto loaded = TaikoMode{}.CreateSession(Pattern());
            Check(loaded.Succeeded(), "Cold-start fixture must load.");
            store->SetValidated({"probe.ymp", {}, "probe-music.mp3", "Taiko"}, std::move(loaded.session));
            scene_ = std::make_unique<RhythmTestScene>(store, playback_, visuals_, false, texts_, result_);
            sampleDelayMs = 200;
            const EngineServices wrapped{services.meshRendering, services.textRendering, *rendering_, services.audio,
                services.windowWidth, services.windowHeight};
            scene_->Initialize(wrapped);
            sampleDelayMs = 0;
            scene_->BeginScene();
        }
        bool Update(const UpdateContext &ctx) override
        {
            Check(ctx.totalSeconds < 5, "Cold-start gameplay did not become visible.");
            InputStateTestAccess::Begin(input_);
            const auto before = sampleStarts;
            if (shown_ && !sent_)
            {
                const auto ticks = ctx.audio.CaptureClockSnapshot().performanceCounterTicks;
                InputStateTestAccess::Press(input_, 'F', ticks);
                InputStateTestAccess::Press(input_, 'D', ticks);
                sent_ = true;
            }
            scene_->Update({ctx.deltaSeconds, ctx.totalSeconds, ctx.updateIndex, input_, ctx.audio, ctx.performance}, scenes_);
            if (sent_)
            {
                Check(sampleStarts == before + 2, "The first visible-frame Don and Kat must both request a sample.");
                verified_ = true;
                return false;
            }
            playback_.Update();
            visuals_.Update(ctx.deltaSeconds);
            return true;
        }
        void Render(const RenderContext &ctx) override { scene_->Render(ctx); visuals_.Render(ctx); }
        void OnResize(std::uint32_t width, std::uint32_t height) override
        { visuals_.OnResize(width, height); if (scene_) scene_->OnResize(width, height); }
        void Shutdown() noexcept override
        {
            sampleDelayMs = 0;
            if (scene_) scene_->Shutdown();
            scene_.reset(); playback_.StopAll(); visuals_.Shutdown(); rendering_.reset();
        }
      private:
        bool &verified_;
        bool shown_{}, sent_{};
        InputState input_;
        ScreenVisual2DManager visuals_;
        AudioPlaybackManager playback_;
        SceneManager scenes_;
        TextCatalog texts_;
        std::unique_ptr<IPlayResult> result_;
        std::unique_ptr<Rendering> rendering_;
        std::unique_ptr<RhythmTestScene> scene_;
    };
    class PauseClient final : public IGameClient
    {
      public:
        explicit PauseClient(bool &verified) : verified_(verified)
        {
        }
        EngineConfig GetEngineConfig() const override
        {
            EngineConfig c;
            c.showWindow = false;
            c.audio.preferredBackend = AudioOutputBackend::NoSound;
            c.audioClipBackendFactory = CreateClip;
            return c;
        }
        void Initialize(const EngineServices &services) override
        {
            services_.emplace(services);
            auto loaded = TaikoMode{}.CreateSession(Pattern());
            Check(loaded.Succeeded(), "Transport fixture must load.");
            session_ = loaded.session.get();
            visuals_.Initialize(services.visual2DRendering, {1280, 720});
            controller_ = std::make_unique<GameplaySessionController>(
                GameplayLaunchRequest{"probe.ymp", {}, "probe-music.mp3", "Taiko"}, playback_, false,
                std::move(loaded.session));
            controller_->InitializeSession();
            const auto loadsBefore = clipLoads;
            Check(controller_->PrepareSession(), "A validated session must be immediately reusable.");
            static_cast<void>(controller_->PrepareAudioResources(services.audio));
            const auto preparedLoads = clipLoads;
            Check(preparedLoads > loadsBefore, "Audio loading must start before presentation becomes ready.");
            controller_->Start(services, RhythmTime{12'240'000});
            Check(clipLoads == preparedLoads, "Starting after presentation must reuse already prepared audio clips.");
            menu_ = std::make_unique<GameplayPauseMenu>(visuals_, texts_);
        }
        bool Update(const UpdateContext &ctx) override
        {
            Check(ctx.totalSeconds < 10, "Pause transport timed out.");
            controller_->Update(ctx);
            if (phase_ == 0 && controller_->CurrentState() != PlayState::Gameplay)
                return true;
            if (phase_ == 0 && controller_->Time().count() >= 12'250'000)
            {
                static_cast<void>(session_->ProcessInput('F', InputEdge::Pressed, controller_->Time()));
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
                const auto musicVoicesBeforeResume = musicStarts.size();
                Check(controller_->ResumeFromPause(ctx, RhythmDuration{500'000}), "Resume must succeed.");
                Check(controller_->Time() == paused_ - RhythmDuration{500'000} &&
                          session_->FinalizedNoteCount() == 1,
                      "Resume must move half a second without undoing the processed note.");
                Check(controller_->CurrentState() == PlayState::Pause && !controller_->PauseMenuOpen(),
                      "Resume must accept the action while the seek keeps gameplay paused.");
                Check(!controller_->ResumeFromPause(ctx, RhythmDuration{500'000}),
                      "A duplicate Resume must not subtract the lead-in again.");
                const auto samplesBefore = sampleStarts;
                WaitForGameplay(*controller_, ctx);
                Check(sampleStarts == samplesBefore && musicStarts.size() == musicVoicesBeforeResume,
                      "Resume must retain the music voice and must not replay hit sounds.");
                CheckClockAlignment(ctx, paused_ - RhythmDuration{500'000});
                Check(musicStarts.back().startPositionMicroseconds ==
                          static_cast<std::uint64_t>((paused_ - RhythmDuration{500'000}).count()),
                      "Music source position must equal the resumed chart time.");
                controller_->OpenPauseMenu(ctx);
                const auto from = controller_->Time();
                Check(controller_->ResumeFromPause(ctx, RhythmDuration{3'000'000}) &&
                          controller_->Time() == from - RhythmDuration{3'000'000},
                      "The three-second Resume must move the same clock.");
                WaitForGameplay(*controller_, ctx);
                controller_->OpenPauseMenu(ctx);
                Check(controller_->RewindFromPause(ctx, 4) && controller_->Time() == RhythmTime{4'000'000} &&
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
                while (controller_->Time() > RhythmTime{3'000'000})
                {
                    controller_->OpenPauseMenu(ctx);
                    Check(controller_->ResumeFromPause(ctx, RhythmDuration{3'000'000}),
                          "Repeated Resume must succeed.");
                    WaitForGameplay(*controller_, ctx);
                }
                controller_->OpenPauseMenu(ctx);
                const auto negativeFrom = controller_->Time();
                Check(controller_->ResumeFromPause(ctx, RhythmDuration{3'000'000}),
                      "Negative pre-roll Resume must succeed.");
                Check(controller_->Time() == negativeFrom - RhythmDuration{3'000'000} &&
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
                const auto *taiko = dynamic_cast<const TaikoPlayResult *>(result.get());
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
                    CheckResourceLoadFailure(ctx);
                    verified_ = true;
                    return false;
                }
            }
            return true;
        }
        void Render(const RenderContext &ctx) override
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
        void CheckTransientVoices(const UpdateContext &ctx)
        {
            GameplayAudioRouter router(playback_);
            std::string error;
            Check(router.Initialize(ctx.audio, error), "The audio resume fixture must initialize.");
            Check(router.RegisterSound("Probe.Hit", "probe-hit.wav", AudioLoadMode::SampleAsync, error),
                  "The transient sample must load.");
            Check(router.RegisterSound("Music.Track", "probe-music.mp3", AudioLoadMode::StreamAsyncAccurate, error),
                  "The background music must load.");
            const auto voicesBefore = playback_.PlaybackCount();
            const std::array<AudioCueRequest, 1> cue{{{"Probe.Hit"}}};
            router.PlayNow(cue);
            Check(playback_.PlaybackCount() == voicesBefore + 1, "The old hit sound must actually be playing.");
            const auto samplesBefore = sampleStarts;
            router.StopAllVoices();
            Check(playback_.PlaybackCount() == voicesBefore, "Pause must discard the old hit sound.");
            using Preparation = AudioPreparationState;
            auto ready = router.PrepareMusicAt(RhythmTime{500'000});
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{2};
            while (ready == Preparation::Loading && std::chrono::steady_clock::now() < deadline)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds{1});
                ready = router.PrepareMusicAt(RhythmTime{500'000});
            }
            Check(ready == Preparation::Ready, "Only background music must finish seek preparation.");
            const auto clock = ctx.audio.CaptureClockSnapshot();
            RhythmTimer timer;
            timer.Start(clock.performanceCounterTicks, clock.performanceCounterFrequency, RhythmTime{500'000});
            timer.AnchorDspClock(RhythmTime{500'000}, clock.dspClock, clock.sampleRate);
            Check(router.StartPreparedMusic(RhythmTime{500'000}, timer) && sampleStarts == samplesBefore &&
                      playback_.PlaybackCount() == voicesBefore + 1,
                  "Resume must recreate background music without replaying the discarded hit sound.");
            router.StopAllVoices();
            const auto startsBeforeEnd = musicStarts.size();
            Check(router.PrepareMusicAt(RhythmTime{120'000'000}) == Preparation::Ready &&
                      router.StartPreparedMusic(RhythmTime{120'000'000}, timer) &&
                      musicStarts.size() == startsBeforeEnd && playback_.PlaybackCount() == voicesBefore,
                  "A chart beyond the music's end must resume without creating an out-of-range voice.");
        }
        void CheckResourceLoadFailure(const UpdateContext &ctx)
        {
            auto loaded = TaikoMode{}.CreateSession(Pattern());
            Check(loaded.Succeeded(), "Failed-resource fixture must load its chart.");
            GameplaySessionController failed({"probe.ymp", {}, "probe-music.mp3", "Taiko"}, playback_, false,
                                             std::move(loaded.session));
            failed.InitializeSession();
            failSampleLoads = true;
            failed.Start(*services_, RhythmTime{});
            failed.Update(ctx);
            Check(failed.CurrentState() == PlayState::Pause && failed.PauseMenuOpen() && !failed.CanContinue() &&
                      failed.ErrorMessage().find("hit sound load failure") != std::string_view::npos,
                  "A failed sample must block gameplay and expose the pause menu with an error.");
            Check(!failed.ResumeFromPause(ctx, RhythmDuration{500'000}),
                  "Failed resources must not be bypassed with Resume.");
            failSampleLoads = false;
            failed.RetryFromPause(ctx);
            WaitForGameplay(failed, ctx);
            Check(failed.ErrorMessage().empty() && failed.Session().CanUseHp(),
                  "Retry must reload failed resources and begin a clean attempt after recovery.");
            failed.Shutdown();
        }

        void CheckClockAlignment(const UpdateContext &ctx, RhythmTime target)
        {
            const auto before = ctx.audio.CaptureClockSnapshot();
            InputState empty;
            controller_->Update({0, ctx.totalSeconds + 1, ctx.updateIndex, empty, ctx.audio, {}});
            const auto after = ctx.audio.CaptureClockSnapshot();
            const auto startTicks = musicScheduleTicks + before.performanceCounterFrequency / 50;
            const auto elapsed = [&](std::int64_t ticks)
            {
                return RhythmDuration{static_cast<std::int64_t>(static_cast<long double>(ticks - startTicks) *
                                                                   1'000'000 / before.performanceCounterFrequency)};
            };
            Check(controller_->Time() >= target + elapsed(before.performanceCounterTicks) - RhythmDuration{5'000} &&
                      controller_->Time() <= target + elapsed(after.performanceCounterTicks) + RhythmDuration{5'000},
                  "The DSP scheduling lead must also be reflected in the QPC gameplay epoch.");
        }
        void CheckSharedLifecycle(const UpdateContext &ctx)
        {
            auto created = ProbeMode{}.CreateSession({}, {});
            std::unique_ptr<IPlaySessionController> common = std::make_unique<GameplaySessionController>(
                GameplayLaunchRequest{"probe.ymp", {}, "probe-music.mp3", "Probe"}, playback_, false,
                std::move(created.session));
            common->InitializeSession();
            Check(common->CurrentState() == PlayState::Loading, "Every mode must start in Loading.");
            common->OpenPauseMenu(ctx);
            Check(common->CurrentState() == PlayState::Pause && !common->CanContinue(),
                  "ESC before asset loading must pause without enabling Resume.");
            common->Start(*services_, RhythmTime{1'000'000});
            common->Update(ctx);
            Check(common->CanContinue(), "Paused loading must finish without starting the timer.");
            failNextMusicStart = true;
            Check(common->ResumeFromPause(ctx, RhythmDuration{500'000}), "A seek request must be accepted.");
            Check(common->CurrentState() == PlayState::Pause && common->PauseMenuOpen() &&
                      common->Time() == RhythmTime{500'000} && !common->ErrorMessage().empty(),
                  "Audio failure must keep a frozen clock and expose the pause menu/error.");
            common->RetryFromPause(ctx);
            Check(common->CurrentState() == PlayState::Loading, "Retry must use Loading for every mode.");
            WaitForGameplay(*common, ctx);
            Check(common->ErrorMessage().empty(), "A successful Retry must clear the resolved music error.");
            InputState empty;
            common->Update({3, ctx.totalSeconds + 3, ctx.updateIndex, empty, ctx.audio, {}});
            Check(common->CurrentState() == PlayState::Ending && common->ShouldShowResult(),
                  "A different mode must complete through the common Ending state.");
            auto result = common->TakeResult();
            Check(result && result->ModeId() == "Probe" &&
                      result->Summary().endReason == PlayEndReason::Completed,
                  "Ending must preserve a mode-specific polymorphic result.");
            common->Update(ctx);
            Check(!common->ShouldShowResult() && !common->TakeResult(), "Ending must never generate a second result.");
        }
        void CheckConfirmedRewindExample(const UpdateContext &ctx)
        {
            auto loaded = TaikoMode{}.CreateSession(Pattern());
            Check(loaded.Succeeded(), "Confirmed measure example must load.");
            const auto start = loaded.session->Timeline().Compile({9, {3, 4}});
            const auto expected = loaded.session->Timeline().Compile({5, {}});
            GameplaySessionController example(
                GameplayLaunchRequest{"probe.ymp", {}, "probe-music.mp3", "Taiko"}, playback_, false,
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
        RhythmTime paused_{};
        PlaySession *session_{};
        std::optional<EngineServices> services_;
        AudioPlaybackManager playback_;
        ScreenVisual2DManager visuals_;
        TextCatalog texts_;
        std::unique_ptr<GameplaySessionController> controller_;
        std::unique_ptr<GameplayPauseMenu> menu_;
    };
    void TestActualMusicSeek()
    {
        AudioSystem audio;
        AudioConfig config;
        config.preferredBackend = AudioOutputBackend::NoSound;
        std::string error;
        Check(audio.Initialize(config, nullptr, nullptr, error), error.c_str());
        AudioPlaybackManager playback;
        GameplayAudioRouter router(playback);
        Check(router.Initialize(audio, error), error.c_str());
        const auto path =
            std::filesystem::path("YOUMU.Assets/Assets/Songs/Music/Angeldream/angel dream hand shaking.mp3");
        Check(router.RegisterSound("Music.Track", path, AudioLoadMode::StreamAsyncAccurate, error), error.c_str());
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{10};
        while (router.PrepareResources() == AudioPreparationState::Loading && std::chrono::steady_clock::now() < deadline)
        {
            audio.Update();
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
        }
        Check(router.PrepareResources() == AudioPreparationState::Ready && router.LastError().empty(), "Actual FMOD music must load.");
        auto clip = audio.LoadSound(path, AudioLoadMode::Stream, error);
        if (!clip)
            throw std::runtime_error(error);
        std::uint64_t duration{};
        Check(clip->GetDurationMicroseconds(duration, error), error.c_str());
        const RhythmTime nearEnd{static_cast<RhythmTime::rep>(duration - 100'000)};
        using Preparation = AudioPreparationState;
        for (const auto position : {RhythmTime{1'000'000}, RhythmTime{500'000}, RhythmTime{5'000'000},
                                    RhythmTime{1'000'000}, RhythmTime{500'000}, RhythmTime{5'000'000},
                                    RhythmTime{}, nearEnd})
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
            RhythmTimer timer;
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
        // Cancel an in-flight source seek and immediately close the service.
        // Retired native sounds must finish release before FMOD is closed.
        std::shared_ptr<AudioClip> pending = audio.LoadSound(path, AudioLoadMode::StreamAsyncAccurate, error);
        Check(pending != nullptr, error.c_str());
        const auto cancelDeadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
        while (pending->LoadState(error) == AudioClipLoadState::Loading &&
               std::chrono::steady_clock::now() < cancelDeadline)
        {
            audio.Update();
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
        }
        AudioPlaybackSettings pendingSettings;
        pendingSettings.startPaused = true;
        pendingSettings.startPositionMicroseconds = 20'000'000;
        Check(playback.Play(pending, pendingSettings, nullptr, error) != 0, error.c_str());
        playback.StopAll();
        audio.RetireClip(std::move(pending));
        audio.Shutdown();
    }
} // namespace
int main(int argc, char **argv)
{
    try
    {
        std::string error;
        Check(InitializeBuiltInAssets(error), error.c_str());
        if (argc > 1 && std::string_view{argv[1]} == "--cold-start")
        {
            bool verified{};
            Check(Run(std::make_unique<ColdStartClient>(verified)) == 0 && verified,
                  "Cold-start presentation must deliver the very first visible-frame Don and Kat inputs.");
            std::puts("Cold-start first-frame Don/Kat input and presentation readiness passed.");
            return 0;
        }
        TestMenu();
        TestActualMusicSeek();
        bool verified{};
        Check(Run(std::make_unique<PauseClient>(verified)) == 0 && verified,
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
