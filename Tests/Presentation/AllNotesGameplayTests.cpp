#include "../Rhythm/Submodules/AllNotesReplay.h"
#include "App/AssetPaths.h"
#include "Audio/GameplayAudioRouter.h"
#include "GameScene/RhythmTestScene/Submodules/GameplayPresenter.h"
#include "MRG_Core.h"

using namespace finger_drum;
using finger_drum::tests::Require;

namespace
{
    std::size_t NoteRootCount(const mrg::visual2d::Visual2DNode &node)
    {
        std::size_t count = node.Name().starts_with("Lane.Note.") ? 1 : 0;
        for (const auto &child : node.Children())
            count += NoteRootCount(*child);
        return count;
    }
    class ReplayGame final : public mrg::IGameClient
    {
      public:
        ReplayGame(bool &verified, std::size_t &frames)
            : presenter_(visuals_, texts_), audio_(playback_), verified_(verified), frames_(frames)
        {
        }

        mrg::EngineConfig GetEngineConfig() const override
        {
            mrg::EngineConfig config;
            config.windowTitle = L"All-notes gameplay regression";
            config.showWindow = false;
            config.renderRateOverrideHz = 60;
            config.audio.allowNoSoundFallback = false;
            return config;
        }

        void Initialize(const mrg::EngineServices &services) override
        {
            visuals_.Initialize(services.visual2DRendering,
                                {static_cast<float>(services.windowWidth),
                                 static_cast<float>(services.windowHeight)});
            const auto root = mrg_client::asset_paths::UserSongs();
            const auto catalog = chart::SongCatalog{}.Load(root);
            const chart::SongCatalogEntry *song = nullptr;
            for (const auto &entry : catalog.songs)
                if (entry.metadataPath.filename() == "angel dream hand shaking.ymm")
                    song = &entry;
            Require(song != nullptr, "Actual engine replay requires AngelDream music.");
            auto loaded = mode::TaikoMode{}.LoadSession(root / "Pattern/angeldream/angeldream [all notes test].ymp");
            Require(loaded.Succeeded(), "All-notes gameplay chart must load.");
            session_ = std::move(loaded.session);
            replay_ = std::make_unique<tests::AllNotesReplay>(*session_);
            presenter_.Initialize(services, *session_);
            Require(NoteRootCount(visuals_.FindCanvas(1)->Root()) == 0,
                    "Session entry must not eagerly construct every note visual.");
            presenter_.SetVisible(true);
            RegisterAudio(services.audio, song->audioPath);
            const auto initial = presenter_.InitialTimelineTime();
            const auto clock = services.audio.CaptureClockSnapshot();
            timer_.Start(clock.performanceCounterTicks, clock.performanceCounterFrequency, initial);
            timer_.AnchorDspClock(initial, clock.dspClock, clock.sampleRate);
            const std::array music{rhythm::AudioCueRequest{.sound = "Music.Track", .bus = "Music"}};
            audio_.Schedule(music, timer_);
            Require(audio_.LastError().empty(), "Music must schedule on the real audio backend.");
        }

        bool Update(const mrg::UpdateContext &context) override
        {
            visuals_.Update(context.deltaSeconds);
            const auto time = timer_.Now(context.audio.CaptureClockSnapshot().performanceCounterTicks);
            const auto result = replay_->Advance(*session_, time);
            presenter_.AdvanceEffects(context.deltaSeconds);
            presenter_.ApplyFeedback({.result = result});
            presenter_.UpdatePresentation(time);
            audio_.PlayNow(result.audioCues);
            audio_.ApplyAutomation(session_->EvaluateAutomation(time));
            audio_.Update();
            playback_.Update();
            Require(audio_.LastError().empty(), "Real hit/tick/pop playback must have no audio error.");
            Require(context.totalSeconds < 40, "Actual gameplay replay must finish within 40 seconds.");
            if (time >= replay_->End())
            {
                replay_->Verify(*session_);
                presenter_.UpdatePresentation(time + rhythm::RhythmDuration{1'000'000});
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

        void Render(const mrg::graphics::RenderContext &context) override
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
        void RegisterAudio(mrg::audio::AudioSystem &system, const std::filesystem::path &music)
        {
            std::string error;
            Require(audio_.Initialize(system, error), "Audio initialization: " + error);
            struct Sample
            {
                rhythm::SoundId id;
                const wchar_t *file;
            };
            const std::array samples{Sample{mode::taiko_sound::DonHit, L"don.wav"},
                                     Sample{mode::taiko_sound::LongNoteTick, L"don.wav"},
                                     Sample{mode::taiko_sound::DonFreeInput, L"don.wav"},
                                     Sample{mode::taiko_sound::KatHit, L"kat.wav"},
                                     Sample{mode::taiko_sound::KatFreeInput, L"kat.wav"},
                                     Sample{mode::taiko_sound::BigDonFirstHit, L"bigdon.wav"},
                                     Sample{mode::taiko_sound::BigKatFirstHit, L"bigkat.wav"},
                                     Sample{mode::taiko_sound::BalloonPop, L"pop.wav"}};
            // Preserve production aliases: one decoded sample/native channel per file.
            std::map<std::wstring, std::vector<rhythm::SoundId>> aliases;
            for (const auto &sample : samples)
                aliases[sample.file].push_back(sample.id);
            for (const auto &[file, ids] : aliases)
                Require(audio_.RegisterSoundAliases(ids, mrg_client::asset_paths::skin::TaikoHitSound(file), error),
                        "Sample registration: " + error);
            Require(audio_.RegisterSound("Music.Track", music, mrg::audio::AudioLoadMode::Stream, error),
                    "Music registration: " + error);
            std::cout << "Actual audio output: " << static_cast<int>(system.ActiveOutput()) << '\n';
        }

        mrg::audio::AudioPlaybackManager playback_;
        mrg::visual2d::ScreenVisual2DManager visuals_;
        texts::TextCatalog texts_;
        std::unique_ptr<mode::PlaySession> session_;
        std::unique_ptr<tests::AllNotesReplay> replay_;
        GameplayPresenter presenter_;
        audio::GameplayAudioRouter audio_;
        rhythm::RhythmTimer timer_;
        bool &verified_;
        std::size_t &frames_;
    };
} // namespace

int main()
{
    try
    {
        std::string error;
        Require(assets::InitializeBuiltInAssets(error), "Built-in assets: " + error);
        bool verified = false;
        std::size_t frames = 0;
        const int exitCode = mrg::Run(std::make_unique<ReplayGame>(verified, frames));
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
