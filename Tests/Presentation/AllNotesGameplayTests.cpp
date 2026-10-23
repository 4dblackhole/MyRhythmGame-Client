#include "../Rhythm/Submodules/AllNotesReplay.h"
#include "App/AssetPaths.h"
#include "Audio/GameplayAudioRouter.h"
#include "GameScene/RhythmTestScene/Submodules/GameplayPresenter.h"
#include "PhantomCore.h"
#include <algorithm>
#include <chrono>
#include <thread>

using tests::Require;

namespace
{
    std::size_t NoteRootCount(const Visual2DNode &node)
    {
        std::size_t count = node.Name().starts_with("Lane.Note.") ? 1 : 0;
        for (const auto &child : node.Children())
            count += NoteRootCount(*child);
        return count;
    }
    Visual2DNode &ProgressFill(Visual2DNode &root)
    {
        for (const auto &child : root.Children())
            if (child->Name() == "Hud.GameProgress")
                for (const auto &part : child->Children())
                    if (part->Name() == "Progress.Fill")
                        return *part;
        throw std::runtime_error("Actual gameplay must create the progress fill.");
    }
    class ReplayGame final : public IGameClient
    {
      public:
        ReplayGame(bool &verified, std::size_t &frames, bool effects, std::string songFilter)
            : presenter_(visuals_, texts_), audio_(playback_), verified_(verified), frames_(frames), effects_(effects), songFilter_(std::move(songFilter))
        {
        }

        EngineConfig GetEngineConfig() const override
        {
            EngineConfig config;
            config.windowTitle = L"All-notes gameplay regression";
            config.showWindow = false;
            config.renderRateOverrideHz = 60;
            config.audio.allowNoSoundFallback = false;
            return config;
        }

        void Initialize(const EngineServices &services) override
        {
            visuals_.Initialize(services.visual2DRendering,
                                {static_cast<float>(services.windowWidth),
                                 static_cast<float>(services.windowHeight)});
            const auto root = UserSongs();
            const auto catalog = SongCatalog{}.Load(root);
            const SongCatalogEntry *song = nullptr;
            for (const auto &entry : catalog.songs)
                if ((!songFilter_.empty() && entry.metadataPath.generic_u8string().find(
                        std::u8string_view{reinterpret_cast<const char8_t*>(songFilter_.c_str())}) != std::u8string::npos) ||
                    (songFilter_.empty() && entry.metadataPath.filename() == "angel dream hand shaking.ymm"))
                    song = &entry;
            if (!song)
                throw std::runtime_error("Actual engine replay requires the selected catalog song.");
            auto loaded = TaikoMode{}.LoadSession(!songFilter_.empty() ? song->patterns.front().patternPath : root / (effects_
                ? "Patterns/angeldream/YME Effects Verification/angeldream [effects test].ymp"
                : "Patterns/angeldream/All Notes Verification/angeldream [all notes test].ymp"));
            Require(loaded.Succeeded(), "All-notes gameplay chart must load.");
            session_ = std::move(loaded.session);
            Require(!effects_ || session_->Gear().Lanes().front()->Notes().size() == 15,
                    "Effects replay must load the effect fixture, not the all-notes fixture.");
            std::cout << (effects_ ? "Replay fixture: YME effects\n" : "Replay fixture: all notes\n");
            replay_ = std::make_unique<tests::AllNotesReplay>(*session_, !effects_ && songFilter_.empty());
            if (!songFilter_.empty())
            {
                std::uint64_t sourceFrames{}, attack = UINT64_MAX;
                std::uint32_t sourceRate{};
                DecodeAudioPcm(song->audioPath, {}, [&](auto channels, auto rate, auto samples) {
                    sourceRate = rate;
                    for (std::size_t i = 0; i < samples.size(); ++i)
                        if (attack == UINT64_MAX && std::abs(samples[i]) > .0316227766F)
                            attack = sourceFrames + i / channels;
                    sourceFrames += samples.size() / channels;
                });
                const auto note = session_->Gear().Lanes().front()->Notes().front()->Timing();
                const double attackMs = static_cast<double>(attack) * 1000 / sourceRate;
                Require(std::abs(note.count() / 1000.0 - attackMs) < 1,
                        "Actual gameplay note timing must match the normalized PCM attack within 1 ms.");
                std::cout << "Canonical gameplay timing: first_note_ms=" << note.count() / 1000.0
                          << " pcm_attack_ms=" << attackMs << '\n';
            }
            const auto imageDeadline = std::chrono::steady_clock::now() + std::chrono::seconds{10};
            while (!presenter_.PrepareAssets(services) && std::chrono::steady_clock::now() < imageDeadline)
                std::this_thread::sleep_for(std::chrono::milliseconds{1});
            Require(presenter_.PrepareAssets(services), "Async skin image preparation must complete.");
            presenter_.Initialize(services, *session_);
            Require(NoteRootCount(visuals_.FindCanvas(1)->Root()) == 0,
                    "Session entry must not eagerly construct every note visual.");
            presenter_.SetVisible(true);
            RegisterAudio(services.audio, song->audioPath);
            const auto readyDeadline = std::chrono::steady_clock::now() + std::chrono::seconds{10};
            auto preparation = audio_.PrepareResources();
            while (preparation == AudioPreparationState::Loading &&
                   std::chrono::steady_clock::now() < readyDeadline)
            {
                services.audio.Update();
                std::this_thread::sleep_for(std::chrono::milliseconds{1});
                preparation = audio_.PrepareResources();
            }
            Require(preparation == AudioPreparationState::Ready && audio_.LastError().empty(),
                    "Async gameplay resources must become ready before clock anchoring.");
            const auto initial = presenter_.InitialTimelineTime();
            const auto clock = services.audio.CaptureClockSnapshot();
            timer_.Start(clock.performanceCounterTicks, clock.performanceCounterFrequency, initial);
            timer_.AnchorDspClock(initial, clock.dspClock, clock.sampleRate);
            const std::array music{AudioCueRequest{.sound = "Music.Track", .bus = "Music"}};
            audio_.Schedule(music, timer_);
            Require(audio_.LastError().empty(), "Music must schedule on the real audio backend.");
        }

        bool Update(const UpdateContext &context) override
        {
            visuals_.Update(context.deltaSeconds);
            const auto time = timer_.Now(context.audio.CaptureClockSnapshot().performanceCounterTicks);
            const auto result = replay_->Advance(*session_, time);
            presenter_.AdvanceEffects(context.deltaSeconds);
            const bool keyPressed = std::ranges::any_of(result.events,
                [](const NoteEvent &event) { return event.inputAction.has_value(); });
            sawInput_ = sawInput_ || keyPressed;
            presenter_.ApplyFeedback({.keyPressed = keyPressed, .result = result});
            presenter_.UpdatePresentation(time);
            const auto *progressSprite = ProgressFill(visuals_.FindCanvas(1)->Root())
                                             .GetComponent<SpriteVisualComponent>();
            if (!progressSprite)
                throw std::runtime_error("Actual progress fill requires a Sprite component.");
            const float progress = progressSprite->UvScale().x;
            Require(progress >= 0 && progress <= 1, "Rendered progress must remain within the track.");
            sawPartialProgress_ = sawPartialProgress_ || (progress > 0 && progress < 1);
            audio_.PlayNow(result.audioCues);
            audio_.ApplyAutomation(session_->EvaluateAutomation(time));
            audio_.Update();
            playback_.Update();
            Require(audio_.LastError().empty(), "Real hit/tick/pop playback must have no audio error.");
            Require(context.totalSeconds < 40, "Actual gameplay replay must finish within 40 seconds.");
            if (!songFilter_.empty() && time >= RhythmTime{4'000'000})
            {
                Require(sawInput_ && sawPartialProgress_, "The selected song must render, judge and play its opening notes.");
                verified_ = true;
                return false;
            }
            if (time >= replay_->End())
            {
                replay_->Verify(*session_);
                Require(sawPartialProgress_ && progress == 1,
                        "Real playback must advance the progress bar and finish at the last note/tail.");
                presenter_.UpdatePresentation(time + RhythmDuration{1'000'000});
                Require(NoteRootCount(visuals_.FindCanvas(1)->Root()) == 0,
                        "Expired note subtrees must not accumulate during play.");
                session_->Reset();
                presenter_.ApplyFeedback({.reset = true});
                presenter_.UpdatePresentation(session_->Gear().Lanes().front()->Notes().front()->Timing());
                Require(NoteRootCount(visuals_.FindCanvas(1)->Root()) > 0,
                        "Reset/backward presentation must recreate visible notes from cached images.");
                verified_ = true;
                return false;
            }
            return true;
        }

        void Render(const RenderContext &context) override
        {
            visuals_.Render(context);
            ++frames_;
        }
        void OnResize(std::uint32_t width, std::uint32_t height) override
        {
            visuals_.OnResize(width, height);
            presenter_.OnResize(width, height);
        }
        void Shutdown() noexcept override
        {
            presenter_.Shutdown();
            audio_.Shutdown();
            replay_.reset();
            session_.reset();
            visuals_.Shutdown();
            playback_.StopAll();
        }

      private:
        void RegisterAudio(AudioSystem &system, const std::filesystem::path &music)
        {
            std::string error;
            Require(audio_.Initialize(system, error), "Audio initialization: " + error);
            struct Sample
            {
                SoundId id;
                const wchar_t *file;
            };
            const std::array samples{Sample{DonHit, L"don.wav"},
                                     Sample{LongNoteTick, L"don.wav"},
                                     Sample{DonFreeInput, L"don.wav"},
                                     Sample{KatHit, L"kat.wav"},
                                     Sample{KatFreeInput, L"kat.wav"},
                                     Sample{BigDonFirstHit, L"bigdon.wav"},
                                     Sample{BigKatFirstHit, L"bigkat.wav"},
                                     Sample{BalloonPop, L"pop.wav"}};
            // Preserve production aliases: one decoded sample/native channel per file.
            std::map<std::wstring, std::vector<SoundId>> aliases;
            for (const auto &sample : samples)
                aliases[sample.file].push_back(sample.id);
            for (const auto &[file, ids] : aliases)
                Require(audio_.RegisterSoundAliases(ids, TaikoHitSound(file), error,
                                                    AudioLoadMode::SampleAsync),
                        "Sample registration: " + error);
            for (const auto &[id, path] : session_->HitSoundFiles())
                Require(audio_.RegisterSound(id, path, AudioLoadMode::SampleAsync, error),
                        "YME sample registration: " + error);
            audio_.PrepareAutomation(session_->Effects());
            Require(audio_.RegisterSound("Music.Track", music, AudioLoadMode::StreamAsync, error),
                    "Music registration: " + error);
            std::cout << "Actual audio output: " << static_cast<int>(system.ActiveOutput()) << '\n';
        }

        AudioPlaybackManager playback_;
        ScreenVisual2DManager visuals_;
        TextCatalog texts_;
        std::unique_ptr<PlaySession> session_;
        std::unique_ptr<tests::AllNotesReplay> replay_;
        GameplayPresenter presenter_;
        GameplayAudioRouter audio_;
        RhythmTimer timer_;
        bool &verified_;
        std::size_t &frames_;
        bool effects_{};
        bool sawPartialProgress_{};
        bool sawInput_{};
        std::string songFilter_;
    };
} // namespace

int main(int argc, char** argv)
{
    try
    {
        std::string error;
        Require(InitializeBuiltInAssets(error), "Built-in assets: " + error);
        bool verified = false;
        std::size_t frames = 0;
#if defined(TEST_YME_EFFECTS)
        constexpr bool effects = true;
#else
        constexpr bool effects = false;
#endif
        const int exitCode = Run(std::make_unique<ReplayGame>(verified, frames, effects, argc > 1 ? argv[1] : ""));
        Require(exitCode == 0 && verified && frames > 100,
                "Actual engine must render and complete the entire gameplay replay.");
        std::cout << "D3D12/FMOD gameplay replay passed: " << frames << " rendered frames.\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception &error)
    {
        std::cerr << "Gameplay replay failure: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
