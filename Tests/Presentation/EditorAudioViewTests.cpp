#include "App/AssetPaths.h"
#include "Catalog/SongCatalog.h"
#include "Editing/ChartEditor.h"
#include "EditorModeTests.h"
#include "EditorMenuTests.h"
#include "EditorScene/Submodules/EditorView.h"
#include "EditorScene/Submodules/Modes/EditorModeFactory.h"
#include "GameScene/MusicSelectScene/Submodules/SongPreviewController.h"
#include <chrono>
#include <iostream>
#include <thread>

namespace
{
    namespace v = mrg::visual2d;
    std::size_t previewLoadCalls{};
    std::unique_ptr<mrg::audio::IAudioClipBackend> CountClipLoads(mrg::audio::IAudioBackend &backend,
        const std::filesystem::path &path, mrg::audio::AudioLoadMode mode, std::string &error)
    {
        ++previewLoadCalls;
        return mrg::audio::CreateFmodAudioClipBackend(backend, path, mode, error);
    }
    void Check(bool condition, const char *message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }
    class RecordingRenderer final : public mrg::graphics::Visual2DRenderSystem
    {
      public:
        std::vector<v::DrawPacket> packets;
        v::ImageHandle LoadImage(const std::filesystem::path &) override
        {
            return {};
        }
        v::Size GetImageSize(v::ImageHandle) const noexcept override
        {
            return {};
        }
        void SubmitScreen(const v::Visual2DCanvas &canvas, const mrg::graphics::RenderContext &, v::Point,
                          std::uint32_t) override
        {
            packets.clear();
            const auto batches = canvas.BuildDrawList();
            v::ForEachDrawPrimitive(batches, [this](const v::DrawPacket &packet) { packets.push_back(packet); });
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
    v::Visual2DNode *FindNode(v::Visual2DNode &node, const std::string_view name)
    {
        if (node.Name() == name)
            return &node;
        for (const auto &child : node.Children())
            if (auto *found = FindNode(*child, name))
                return found;
        return nullptr;
    }
} // namespace

int main()
{
    try
    {
        std::string assetError;
        Check(finger_drum::assets::InitializeBuiltInAssets(assetError), assetError.c_str());
        TestEditorMenu();
        namespace chart = finger_drum::chart;
        chart::PatternDocument pattern;
        pattern.baseBpm = 120;
        pattern.patternOffsetMilliseconds = -500;
        EditorWorkspace range(CreateEditorMode("Taiko"), std::make_unique<chart::ChartEditor>(pattern));
        range.AddNote({0, {}}, 1);
        range.AddNote({1, {}}, 2);
        Check(range.TimelineRangeMilliseconds() == std::pair{-500.0, 1500.0},
              "Timeline must include negative notes and the last note.");

        const auto songs = chart::SongCatalog{}.Load(mrg_client::asset_paths::UserSongs());
        Check(!songs.songs.empty() && !songs.songs.front().patterns.empty(), "Real catalog missing");
        const auto &song = songs.songs.front();
        const auto &selected = song.patterns.front();
        {
            mrg::audio::AudioSystem previewAudio;
            mrg::audio::AudioConfig config;
            config.preferredBackend = mrg::audio::AudioOutputBackend::NoSound;
            std::string error;
            Check(previewAudio.Initialize(config, mrg::audio::CreateFmodAudioBackend,
                                           CountClipLoads, error), error.c_str());
            auto clip = previewAudio.LoadSound(song.audioPath, mrg::audio::AudioLoadMode::StreamAsync, error);
            Check(clip != nullptr, error.c_str());
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{10};
            auto state = clip->LoadState(error);
            while (state == mrg::audio::AudioClipLoadState::Loading && std::chrono::steady_clock::now() < deadline)
            {
                previewAudio.Update();
                std::this_thread::sleep_for(std::chrono::milliseconds{1});
                state = clip->LoadState(error);
            }
            Check(state == mrg::audio::AudioClipLoadState::Ready, "Async MP3 never became ready");
            auto voice = clip->Play({}, nullptr, error);
            Check(voice != nullptr, "Ready async stream must play");
            voice.reset();
            clip.reset();

            mrg::audio::AudioPlaybackManager playback;
            SongPreviewController preview(playback);
            preview.Initialize(previewAudio);
            SongSelectionState selection;
            selection.catalog_.songs.resize(2);
            selection.catalog_.songs[0].audioPath = song.audioPath;
            selection.catalog_.songs[1].audioPath = song.audioPath / "not-a-file";
            selection.visibleSongIndices_ = {0, 1};
            previewLoadCalls = 0;
            const auto poll = [&] {
                previewAudio.Update();
                playback.Update();
                preview.UpdatePreviewAudio(.02);
                preview.SyncPreviewToFocusedSong(selection, true);
                std::this_thread::sleep_for(std::chrono::milliseconds{1});
            };
            const auto awaitVoice = [&] {
                const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{10};
                do { poll(); } while (!playback.PlaybackCount() && std::chrono::steady_clock::now() < deadline);
                Check(playback.PlaybackCount() == 1, "Prepared preview must start one managed voice");
            };
            awaitVoice();
            Check(previewLoadCalls == 1, "Focused ready preview must not reload");
            selection.focusedSongPosition_ = 1;
            for (int i = 0; i < 100; ++i) poll();
            Check(previewLoadCalls == 2, "Failed preview must not reopen its file every Update");
            selection.focusedSongPosition_ = 0;
            awaitVoice();
            Check(previewLoadCalls == 3, "Changing selection must allow preview loading again");
            preview.StopPreviewAudio();
            Check(playback.PlaybackCount() == 0, "Leaving preview must stop its own voice");
            for (int i = 0; i < 10; ++i) preview.UpdatePreviewAudio(.02);
        }
        {
            EditorWorkspace cancelled({selected.patternPath, selected.effectPath, song.audioPath, selected.pattern.mode});
            cancelled.Initialize();
            cancelled.UpdateAnalysis();
            cancelled.Analysis().Stop();
            Check(!cancelled.Analysis().Running(), "Stopped worker must detach its editor immediately");
        }
        EditorWorkspace state({selected.patternPath, selected.effectPath, song.audioPath, selected.pattern.mode});
        state.Initialize();
        state.UpdateAnalysis();
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{120};
        while (state.Analysis().Running() && std::chrono::steady_clock::now() < deadline)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds{10});
            state.UpdateAnalysis();
        }
        Check(!state.Analysis().Running() && state.Analysis().Data().errors.empty(), "Audio worker failed");
        const auto [begin, end] = state.TimelineRangeMilliseconds();
        Check(end >= state.Analysis().Data().sounds.at("Music").durationSeconds * 1000,
              "Timeline must include the entire music file.");

        RecordingRenderer renderer;
        v::ScreenVisual2DManager visuals;
        visuals.Initialize(renderer, {1920, 1080});
        mrg::graphics::MeshRenderSystem meshes;
        mrg::graphics::TextRenderSystem textRendering;
        mrg::audio::AudioSystem audio;
        finger_drum::texts::TextCatalog texts;
        EditorView view(visuals, state, texts);
        view.Initialize({meshes, textRendering, renderer, audio, 1920, 1080});
        const auto hasText = [&](const std::wstring_view text) {
            return std::ranges::any_of(renderer.packets, [&](const auto &packet) { return packet.text == text; });
        };
        visuals.Render({});
        Check(hasText(L"파일"), "Editor view must compose the top menu.");
        Check(hasText(std::wstring(state.Document().Dirty() ? finger_drum::texts::Editor(texts.CurrentLanguage()).saveDirty
                                                           : finger_drum::texts::Editor(texts.CurrentLanguage()).save)),
              "Existing footer Save must remain available.");
        Check(hasText(L"타임라인") && !hasText(L"음악 FFT 스펙트로그램"),
              "Pattern tab must show the timeline without the audio analysis "
              "tracks.");
        state.Seek(state.Document().Notes().front().timing.count() / 1000.0 + 1000);
        state.SelectTab(EditorTab::Audio);
        view.Build();
        visuals.Render({});
        Check(hasText(L"음악 파형") && hasText(L"음악 FFT 스펙트로그램") &&
                  hasText(L"예상 히트사운드 FFT 스펙트로그램"),
              "Three audio tracks missing");
        const auto coloredIn = [&](float top, float bottom) {
            return std::ranges::count_if(renderer.packets, [&](const auto &packet) {
                const float y = 540 - packet.bounds.y - packet.bounds.height;
                return packet.type == v::DrawPacketType::Rectangle && y >= top && y < bottom &&
                       packet.bounds.x >= 205 - 960 && packet.bounds.x < 1770 - 960 && packet.color.red > .3F &&
                       packet.color.green < .95F;
            });
        };
        Check(coloredIn(375, 587) > 10 && coloredIn(640, 852) > 10,
              "Music and expected hits must produce separate visible spectrum "
              "cells.");

        auto *canvas = visuals.FindCanvas(1);
        if (canvas == nullptr)
            throw std::runtime_error("Editor Canvas missing");
        auto *menuBar = FindNode(canvas->Root(), "Editor.MenuBar");
        if (menuBar == nullptr)
            throw std::runtime_error("Editor top menu is missing.");
        Check(menuBar->BoundsInCanvas().height == EditorMenuBar::Height, "Editor top menu geometry is wrong.");
        auto *slider = FindNode(canvas->AnchorNode(v::Anchor::Center), "Editor timeline");
        if (slider == nullptr)
            throw std::runtime_error("Audio timeline slider missing");
        Check(slider->IsVisible(), "Audio timeline slider hidden");
        v::Visual2DInputRouter pointerRouter;
        v::PointerInput pointer{};
        pointer.available = true;
        pointer.position = {205 + 8 + (1645 - 16) * .5F - 960, 540 - 937};
        pointer.leftButtonDown = pointer.leftButtonPressed = true;
        pointerRouter.Process(*canvas, pointer);
        mrg::platform::InputState input;
        Check(!view.Update({0, 0, 1, input, audio, {}}), "Unexpected scene exit");
        Check(std::abs(state.TimeMilliseconds() - (begin + end) * .5) <= 1,
              "Slider action must seek the editor time to its track position.");
        pointer.leftButtonDown = pointer.leftButtonPressed = false;
        pointer.leftButtonReleased = true;
        pointerRouter.Process(*canvas, pointer);
        state.SelectTab(EditorTab::Metadata);
        view.Build();
        Check(!slider->IsVisible(), "Metadata tab must hide the slider");
        state.SelectTab(EditorTab::Audio);
        texts.SetLanguage(finger_drum::texts::Language::English);
        view.Build();
        visuals.Render({});
        Check(hasText(L"AUDIO") && hasText(L"MUSIC FFT SPECTROGRAM") && hasText(L"File"),
              "Audio and menu labels not localized");
        visuals.OnResize(900, 720);
        view.Resize(900, 720);
        view.Build();
        Check(state.TimelineRangeMilliseconds() == std::pair{begin, end}, "Resize changed time range");
        Check(menuBar->BoundsInCanvas().width == canvas->LogicalSize().width,
              "Editor view must resize the composed menu.");
        view.Shutdown();
        Check(visuals.CanvasCount() == 0, "Editor Canvas survived shutdown");
        visuals.OnResize(1920, 1080);
        TestEditorModes(visuals, {meshes, textRendering, renderer, audio, 1920, 1080}, texts,
                        [&renderer]() -> const std::vector<v::DrawPacket> & { return renderer.packets; });
        std::cout << "Editor audio view tests passed.\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
