#include "App/AssetPaths.h"
#include "Catalog/SongCatalog.h"
#include "Editing/ChartEditor.h"
#include "EditorMenuTests.h"
#include "EditorAudioAnalysisIntegrationTests.h"
#include "EditorMetronomeTests.h"
#include "EditorModeTests.h"
#include "EditorPlaybackTests.h"
#include "EditorScene/Submodules/EditorView.h"
#include "EditorScene/Submodules/Modes/EditorModeFactory.h"
#include "GameScene/MusicSelectScene/Submodules/SongPreviewController.h"
#include <chrono>
#include <iostream>
#include <thread>

namespace
{

    std::size_t previewLoadCalls{};
    std::unique_ptr<IAudioClipBackend> CountClipLoads(IAudioBackend &backend,
        const std::filesystem::path &path, AudioLoadMode mode, std::string &error)
    {
        ++previewLoadCalls;
        return CreateFmodAudioClipBackend(backend, path, mode, error);
    }
    void Check(bool condition, const char *message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }
    class RecordingRenderer final : public Visual2DRenderSystem
    {
      public:
        std::vector<DrawPacket> packets;
        ImageHandle LoadImage(const std::filesystem::path &) override
        {
            return {++imageId};
        }
        std::uint64_t imageId{};
        Size GetImageSize(ImageHandle) const noexcept override
        {
            return {};
        }
        void SubmitScreen(const Visual2DCanvas &canvas, const RenderContext &, Point,
                          std::uint32_t) override
        {
            packets.clear();
            const auto batches = canvas.BuildDrawList();
            ForEachDrawPrimitive(batches, [this](const DrawPacket &packet) { packets.push_back(packet); });
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
    Visual2DNode *FindNode(Visual2DNode &node, const std::string_view name)
    {
        if (node.Name() == name)
            return &node;
        for (const auto &child : node.Children())
            if (auto *found = FindNode(*child, name))
                return found;
        return nullptr;
    }
    std::vector<DrawPacket> SpectrumGeometry(Visual2DNode &root)
    {
        std::vector<DrawPacket> result;
        for (const auto name : {"Audio.MusicSpectrum", "Audio.HitSpectrum"})
            if (auto *track = FindNode(root, name))
                for (const auto &tile : track->Children())
                    if (tile->Name() == "Spectrum.Tile")
                        if (auto *geometry = tile->GetComponent<EditorVisual>()) geometry->AppendDrawPackets(result);
        return result;
    }

    void TestAudioRangeSelection(Visual2DCanvas &canvas, EditorView &view, EditorWorkspace &state,
                                 RecordingRenderer &renderer, const TextCatalog &texts)
    {
        auto *track = FindNode(canvas.Root(), "Audio.Selection.0");
        if (!track)
            throw std::runtime_error("Waveform selection input missing.");
        Check(track->IsVisible(), "Waveform selection input must be visible in the audio tab.");
        const auto bounds = track->BoundsInCanvas();
        const auto revision = state.Document().Revision();
        const double firstMs = state.TimeMilliseconds();
        const double lastMs = firstMs + state.audioWindow * 500;
        const auto oldGeometry = SpectrumGeometry(canvas.Root());
        Visual2DInputRouter router;
        PointerInput pointer;
        pointer.position = {bounds.x + bounds.width * .75F, bounds.y + bounds.height * .5F};
        pointer.leftButtonDown = pointer.leftButtonPressed = true;
        router.Process(canvas, pointer);
        pointer.leftButtonPressed = false;
        pointer.position.x = bounds.x + bounds.width * .25F;
        router.Process(canvas, pointer);
        pointer.leftButtonDown = false;
        pointer.leftButtonReleased = true;
        router.Process(canvas, pointer);
        const auto selected = state.SelectedAudioRange();
        if (!selected)
            throw std::runtime_error("Dragging did not create an audio range.");
        Check(std::abs(selected->begin.count() / 1000.0 - firstMs) <= .005 &&
                  std::abs(selected->end.count() / 1000.0 - lastMs) <= .005,
              "Reverse dragging must produce ordered source times through the transformed track bounds.");
        view.Build();
        renderer.packets.clear();
        // Expand the same draw packets as the recording renderer without a physical window.
        ForEachDrawPrimitive(canvas.BuildDrawList(), [&](const auto &packet) { renderer.packets.push_back(packet); });
        const double begin = selected->begin.count() / 1000.0, end = selected->end.count() / 1000.0;
        const auto label = texts.Format("Editor.audioSelectionFormat", begin, end, end - begin);
        Check(std::ranges::any_of(renderer.packets, [&](const auto &packet) {
                  return packet.text && packet.text->value == label;
              }), "Selection start/end/duration must be displayed with localized millisecond formatting.");
        Check(std::ranges::count_if(renderer.packets, [](const auto &packet) {
                  return packet.type == DrawPacketType::Rectangle && std::abs(packet.color.alpha - .24F) < .001F;
              }) == 3, "The same selected interval must shade all three audio tracks.");
        const auto newGeometry = SpectrumGeometry(canvas.Root());
        Check(std::ranges::all_of(newGeometry, [&](const auto &packet) {
                  return std::ranges::any_of(oldGeometry, [&](const auto &old) { return old.rectangles == packet.rectangles; });
              }), "Dragging a selection must reuse FFT tiles instead of rebuilding analysis geometry.");

        // Captured drags keep updating outside the hit rectangle and clamp to the viewport.
        pointer.leftButtonReleased = false;
        pointer.leftButtonDown = pointer.leftButtonPressed = true;
        pointer.position.x = bounds.x + bounds.width * .75F;
        router.Process(canvas, pointer);
        pointer.leftButtonPressed = false;
        pointer.position.x = bounds.x - 200;
        router.Process(canvas, pointer);
        pointer.leftButtonDown = false;
        pointer.leftButtonReleased = true;
        router.Process(canvas, pointer);
        Check(std::abs(state.SelectedAudioRange()->begin.count() / 1000.0 -
                       (state.TimeMilliseconds() - state.audioWindow * 250)) <= .005 &&
                  state.Document().Revision() == revision,
              "Selection must clamp outside drags and leave chart content and revision unchanged.");
    }
} // namespace

int main(int argc, char** argv)
{
    try
    {
        std::string assetError;
        Check(InitializeBuiltInAssets(assetError), assetError.c_str());
        TestEditorMenu();
        TestEditorPlayback();
        TestEditorMetronome();

        PatternDocument pattern;
        pattern.baseBpm = 120;
        pattern.patternOffsetMilliseconds = -500;
        EditorWorkspace range(CreateEditorMode("Taiko"), std::make_unique<ChartEditor>(pattern));
        range.AddNote({0, {}}, 1);
        range.AddNote({1, {}}, 2);
        Check(range.TimelineRangeMilliseconds() == std::pair{-500.0, 1500.0},
              "Timeline must include negative notes and the last note.");

        RunEditorAudioAnalysisIntegrationTests(
            std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "YOUMU.Assets/Assets/Songs");
        const auto songs = SongCatalog{}.Load(UserSongs());
        Check(!songs.songs.empty() && !songs.songs.front().patterns.empty(), "Real catalog missing");
        auto songIt = songs.songs.begin();
        if (argc > 1)
        {
            const std::u8string_view filter{reinterpret_cast<const char8_t*>(argv[1])};
            songIt = std::ranges::find_if(songs.songs, [&](const auto& item) {
                return item.metadataPath.generic_u8string().find(filter) != std::u8string::npos;
            });
            Check(songIt != songs.songs.end(), "Requested timing song is missing.");
        }
        const auto &song = *songIt;
        TestEditorPlaybackWithFmod(song.audioPath);
        const auto &selected = song.patterns.front();
        {
            AudioSystem previewAudio;
            AudioConfig config;
            config.preferredBackend = AudioOutputBackend::NoSound;
            std::string error;
            Check(previewAudio.Initialize(config, CreateFmodAudioBackend,
                                           CountClipLoads, error), error.c_str());
            auto clip = previewAudio.LoadSound(song.audioPath, AudioLoadMode::StreamAsync, error);
            Check(clip != nullptr, error.c_str());
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{10};
            auto state = clip->LoadState(error);
            while (state == AudioClipLoadState::Loading && std::chrono::steady_clock::now() < deadline)
            {
                previewAudio.Update();
                std::this_thread::sleep_for(std::chrono::milliseconds{1});
                state = clip->LoadState(error);
            }
            Check(state == AudioClipLoadState::Ready, "Async MP3 never became ready");
            auto voice = clip->Play({}, nullptr, error);
            Check(voice != nullptr, "Ready async stream must play");
            voice.reset();
            clip.reset();

            AudioPlaybackManager playback;
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
        // Cancellation must leave the shared queue available for the next real
        // analysis, including when the requesting editor has been destroyed.
        for (int attempt = 0; attempt < 8; ++attempt)
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
        if (argc > 1)
        {
            const auto& music = state.Analysis().Data().sounds.at("Music");
            const auto attack = std::ranges::find_if(music.frames, [](const auto& frame) {
                return frame.peak > .0316227766F;
            });
            Check(attack != music.frames.end(), "Requested timing song has no measurable attack.");
            const double onsetBucket = static_cast<double>(attack - music.frames.begin()) *
                                       music.secondsPerFrame * 1000;
            state.SeekFirstNote();
            const double firstNote = state.TimeMilliseconds();
            Check(firstNote >= onsetBucket && firstNote < onsetBucket + music.secondsPerFrame * 1000,
                  "The first note must fall inside the editor waveform's first attack interval.");
            std::cout << "Canonical editor timing: first_note_ms=" << firstNote
                      << " waveform_attack_bucket_ms=" << onsetBucket
                      << " spectrum_frame_ms=" << music.secondsPerFrame * 1000 << '\n';
            state.Seek(0);
        }
        const auto musicDuration = state.Analysis().MusicDurationSeconds();
        if (!musicDuration)
            throw std::runtime_error("Current music duration is unavailable after analysis.");
        const double musicEndMs = *musicDuration * 1000;
        const double lastNoteMs = state.Document().NoteTimeRange().second.count() / 1000.0;
        Check(std::abs(lastNoteMs - musicEndMs) > 1,
              "The real chart must have distinct music/note ends for X testing.");
        state.SeekLastNote();
        Check(state.TimeMilliseconds() == lastNoteMs, "First X must seek the real chart's last note.");
        state.SeekLastNote();
        Check(state.TimeMilliseconds() == musicEndMs, "Second X must seek the music end.");
        state.SeekLastNote();
        Check(state.TimeMilliseconds() == lastNoteMs, "Third X must return to the last note.");
        state.Analysis().Invalidate();
        Check(!state.Analysis().MusicDurationSeconds(), "Invalidated analysis must not provide a stale end position.");
        bool pending = false;
        try
        {
            state.SeekLastNote();
        }
        catch (const std::runtime_error &)
        {
            pending = true;
        }
        Check(pending && state.TimeMilliseconds() == lastNoteMs, "An unavailable end must not move the cursor.");
        state.UpdateAnalysis(); // Unchanged files can reuse their completed analysis.
        state.SeekLastNote();
        Check(state.TimeMilliseconds() == musicEndMs, "Retry must retain the second-X destination.");
        state.Seek(0);
        const auto [begin, end] = state.TimelineRangeMilliseconds();
        Check(end >= state.Analysis().Data().sounds.at("Music").durationSeconds * 1000,
              "Timeline must include the entire music file.");

        RecordingRenderer renderer;
        ScreenVisual2DManager visuals;
        visuals.Initialize(renderer, {1920, 1080});
        MeshRenderSystem meshes;
        TextRenderSystem textRendering;
        AudioSystem audio;
        TextCatalog texts;
        EditorView view(visuals, state, texts);
        view.Initialize({meshes, textRendering, renderer, audio, 1920, 1080});
        const auto hasText = [&](const std::wstring_view text)
        {
            return std::ranges::any_of(renderer.packets,
                                       [&](const auto &packet) { return packet.text && packet.text->value == text; });
        };
        visuals.Render({});
        Check(hasText(L"파일"), "Editor view must compose the top menu.");
        Check(hasText(std::wstring(state.Document().Dirty() ? Editor(texts).saveDirty
                                                            : Editor(texts).save)),
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
        const auto coloredIn = [&](float top, float bottom)
        {
            return std::ranges::count_if(
                renderer.packets,
                [&](const auto &packet)
                {
                    DirectX::XMFLOAT3 corner;
                    DirectX::XMStoreFloat3(
                        &corner,
                        DirectX::XMVector3TransformCoord(
                            DirectX::XMVectorSet(packet.bounds.x, packet.bounds.y + packet.bounds.height, 0, 1),
                            DirectX::XMLoadFloat4x4(&packet.nodeTransform)));
                    const float y = 540 - corner.y;
                    return packet.type == DrawPacketType::Rectangle && y >= top && y < bottom &&
                           corner.x >= 205 - 960 && corner.x < 1770 - 960 && packet.color.red > .3F &&
                           packet.color.green < .95F;
                });
        };
        Check(coloredIn(375, 587) > 10 && coloredIn(640, 852) > 10,
              "Music and expected hits must produce separate visible spectrum "
              "cells.");

        auto *canvas = visuals.FindCanvas(1);
        if (canvas == nullptr)
            throw std::runtime_error("Editor Canvas missing");
        const auto geometryBefore = SpectrumGeometry(canvas->Root());
        state.Seek(state.TimeMilliseconds() + 16);
        view.Build();
        const auto geometryAfter = SpectrumGeometry(canvas->Root());
        const auto reused = std::ranges::count_if(
            geometryAfter,
            [&](const auto &packet)
            {
                return packet.rectangles && std::ranges::any_of(geometryBefore, [&](const auto &old)
                                                                { return old.rectangles == packet.rectangles; });
            });
        Check(reused >= 8, "Moving audio time must reuse both tracks' cached tile geometry.");
        const auto oldWindow = state.audioWindow;
        state.audioWindow /= 2;
        view.Build();
        const auto zoomed = SpectrumGeometry(canvas->Root());
        Check(std::ranges::none_of(zoomed,
                                   [&](const auto &packet)
                                   {
                                       return packet.rectangles &&
                                              std::ranges::any_of(geometryAfter, [&](const auto &old)
                                                                  { return old.rectangles == packet.rectangles; });
                                   }),
              "Changing audio zoom must invalidate the cached tile resolution.");
        state.audioWindow = oldWindow;
        view.Build();
        TestAudioRangeSelection(*canvas, view, state, renderer, texts);
        auto *menuBar = FindNode(canvas->Root(), "Editor.MenuBar");
        if (menuBar == nullptr)
            throw std::runtime_error("Editor top menu is missing.");
        Check(menuBar->BoundsInCanvas().height == EditorMenuBar::Height, "Editor top menu geometry is wrong.");
        auto *slider = FindNode(canvas->AnchorNode(Anchor::Center), "Editor timeline");
        if (slider == nullptr)
            throw std::runtime_error("Audio timeline slider missing");
        Check(slider->IsVisible(), "Audio timeline slider hidden");
        Visual2DInputRouter pointerRouter;
        PointerInput pointer{};
        pointer.available = true;
        pointer.position = {205 + 8 + (1645 - 16) * .5F - 960, 540 - 937};
        pointer.leftButtonDown = pointer.leftButtonPressed = true;
        pointerRouter.Process(*canvas, pointer);
        InputState input;
        Check(!view.Update({0, 0, 1, input, audio, {}}), "Unexpected scene exit");
        Check(std::abs(state.TimeMilliseconds() - (begin + end) * .5) <= 1,
              "Slider action must seek the editor time to its track position.");
        pointer.leftButtonDown = pointer.leftButtonPressed = false;
        pointer.leftButtonReleased = true;
        pointerRouter.Process(*canvas, pointer);
        state.SelectTab(EditorTab::Metadata);
        view.Build();
        Check(!slider->IsVisible(), "Metadata tab must hide the slider");
        auto *hiddenSelection = FindNode(canvas->Root(), "Audio.Selection.0");
        auto *hiddenRate = FindNode(canvas->Root(), "Editor playback rate");
        if (!hiddenSelection || !hiddenRate)
            throw std::runtime_error("Audio controls missing after tab switch.");
        Check(!hiddenSelection->IsVisible() && !hiddenRate->IsVisible(),
              "Metadata must hide audio selection hit areas and rate controls.");
        state.SelectTab(EditorTab::Audio);
        texts.SetLanguage(Language::English);
        view.Build();
        visuals.Render({});
        Check(hasText(L"AUDIO") && hasText(L"MUSIC FFT SPECTROGRAM") && hasText(L"File"),
              "Audio and menu labels not localized");
        Check(hasText(L"METRONOME: OFF") && hasText(L"−1 ms") && hasText(L"+1 ms"),
              "Audio tab must expose offset adjustment and metronome controls.");
        auto *rateSlider = FindNode(canvas->Root(), "Editor playback rate");
        if (!rateSlider)
            throw std::runtime_error("Audio playback rate slider missing.");
        Check(rateSlider->IsVisible(), "Audio playback rate slider must be visible.");
        const auto rateBounds = rateSlider->BoundsInCanvas();
        for (const float value : {0.0F, 1.0F})
        {
            pointer.position = {rateBounds.x + 8 + (rateBounds.width - 16) * value,
                                rateBounds.y + rateBounds.height * .5F};
            pointer.leftButtonDown = pointer.leftButtonPressed = true;
            pointer.leftButtonReleased = false;
            pointerRouter.Process(*canvas, pointer);
            Check(!view.Update({0, 0, 1, input, audio, {}}), "Rate control unexpectedly left the editor.");
            const auto expected = value == 0 ? .25 : 2.0;
            Check(state.PlaybackRate() == expected, "The rate slider must reach quarter and double speed.");
            visuals.Render({});
            Check(hasText(texts.Format("Editor.playbackRateFormat", expected)), "Current rate label missing.");
            pointer.leftButtonDown = pointer.leftButtonPressed = false;
            pointer.leftButtonReleased = true;
            pointerRouter.Process(*canvas, pointer);
        }
        state.SetPlaybackRate(1);
        visuals.OnResize(900, 720);
        view.Resize(900, 720);
        view.Build();
        Check(state.TimelineRangeMilliseconds() == std::pair{begin, end}, "Resize changed time range");
        Check(menuBar->BoundsInCanvas().width == canvas->LogicalSize().width,
              "Editor view must resize the composed menu.");
        TestAudioRangeSelection(*canvas, view, state, renderer, texts);
        state.SelectTab(EditorTab::Pattern);
        state.SetRealtime(true);
        view.Build();
        visuals.Render({});
        Check(hasText(L"−") && hasText(L"+") && hasText(L"TIME ZOOM 100%"),
              "Realtime view must expose the time-axis zoom buttons and current scale.");
        view.Shutdown();
        Check(visuals.CanvasCount() == 0, "Editor Canvas survived shutdown");
        visuals.OnResize(1920, 1080);
        TestEditorModes(visuals, {meshes, textRendering, renderer, audio, 1920, 1080}, texts,
                        [&renderer]() -> const std::vector<DrawPacket> & { return renderer.packets; });
        std::cout << "Editor audio view tests passed.\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
