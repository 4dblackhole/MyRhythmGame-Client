#include "App/AssetPaths.h"
#include "Catalog/SongCatalog.h"
#include "EditorScene/Submodules/EditorView.h"
#include <chrono>
#include <iostream>
#include <thread>

namespace
{
    namespace v = mrg::visual2d;
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
        void SubmitScreen(const v::Visual2DCanvas &canvas, const mrg::graphics::RenderContext &,
                          v::Point, std::uint32_t) override
        {
            packets = canvas.BuildDrawList();
        }
        void SubmitPlane(const v::Visual2DCanvas &, const mrg::graphics::RenderContext &,
                         const DirectX::XMFLOAT4X4 &, v::Size, const DirectX::XMFLOAT4X4 &) override
        {
        }
        mrg::graphics::RenderTargetTextureHandle CreateCanvasRenderTarget(std::uint32_t,
                                                                          std::uint32_t) override
        {
            return {};
        }
        void RenderToTexture(const v::Visual2DCanvas &,
                             const mrg::graphics::RenderTargetTextureHandle &,
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
        namespace chart = finger_drum::chart;
        EditorWorkspace range(finger_drum::GameplayLaunchRequest{});
        chart::PatternDocument pattern;
        pattern.baseBpm = 120;
        pattern.patternOffsetMilliseconds = -500;
        range.editor = std::make_unique<chart::ChartEditor>(pattern);
        range.editor->AddNote({0, {}}, 1);
        range.editor->AddNote({1, {}}, 2);
        Check(range.TimelineRangeMilliseconds() == std::pair{-500.0, 1500.0},
              "Timeline must include negative notes and the last note.");

        const auto songs = chart::SongCatalog{}.Load(mrg_client::asset_paths::UserSongs());
        Check(!songs.songs.empty() && !songs.songs.front().patterns.empty(),
              "Real catalog missing");
        const auto &song = songs.songs.front();
        const auto &selected = song.patterns.front();
        EditorWorkspace state(
            {selected.patternPath, selected.effectPath, song.audioPath, selected.pattern.mode});
        state.Initialize();
        state.UpdateAnalysis();
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{120};
        while (state.analysis.Running() && std::chrono::steady_clock::now() < deadline)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds{10});
            state.UpdateAnalysis();
        }
        Check(!state.analysis.Running() && state.analysis.Data().errors.empty(),
              "Audio worker failed");
        const auto [begin, end] = state.TimelineRangeMilliseconds();
        Check(end >= state.analysis.Data().sounds.at("Music").durationSeconds * 1000,
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
            return std::ranges::any_of(renderer.packets,
                                       [&](const auto &packet) { return packet.text == text; });
        };
        visuals.Render({});
        Check(hasText(L"타임라인") && !hasText(L"음악 FFT 스펙트로그램"),
              "Pattern tab must show the timeline without the audio analysis "
              "tracks.");
        state.timeMs = state.editor->Notes().front().timing.count() / 1000.0 + 1000;
        state.tab = 4;
        view.Build();
        visuals.Render({});
        Check(hasText(L"음악 파형") && hasText(L"음악 FFT 스펙트로그램") &&
                  hasText(L"예상 히트사운드 FFT 스펙트로그램"),
              "Three audio tracks missing");
        const auto coloredIn = [&](float top, float bottom) {
            return std::ranges::count_if(renderer.packets, [&](const auto &packet) {
                const float y = 540 - packet.bounds.y - packet.bounds.height;
                return packet.type == v::DrawPacketType::Rectangle && y >= top && y < bottom &&
                       packet.bounds.x >= 205 - 960 && packet.bounds.x < 1770 - 960 &&
                       packet.color.red > .3F && packet.color.green < .95F;
            });
        };
        Check(coloredIn(375, 587) > 10 && coloredIn(640, 852) > 10,
              "Music and expected hits must produce separate visible spectrum "
              "cells.");

        auto *canvas = visuals.FindCanvas(1);
        Check(canvas != nullptr, "Editor Canvas missing");
        auto *slider = FindNode(canvas->AnchorNode(v::Anchor::Center), "Editor timeline");
        Check(slider && slider->IsVisible(), "Audio timeline slider missing");
        v::Visual2DInputRouter pointerRouter;
        v::PointerInput pointer{};
        pointer.available = true;
        pointer.position = {205 + 8 + (1645 - 16) * .5F - 960, 540 - 937};
        pointer.leftButtonDown = pointer.leftButtonPressed = true;
        pointerRouter.Process(*canvas, pointer);
        mrg::platform::InputState input;
        Check(!view.Update({0, 0, 1, input, audio, {}}), "Unexpected scene exit");
        Check(std::abs(state.timeMs - (begin + end) * .5) <= 1,
              "Slider action must seek the editor time to its track position.");
        pointer.leftButtonDown = pointer.leftButtonPressed = false;
        pointer.leftButtonReleased = true;
        pointerRouter.Process(*canvas, pointer);
        state.tab = 2;
        view.Build();
        Check(!slider->IsVisible(), "Metadata tab must hide the slider");
        state.tab = 4;
        texts.SetLanguage(finger_drum::texts::Language::English);
        view.Build();
        visuals.Render({});
        Check(hasText(L"AUDIO") && hasText(L"MUSIC FFT SPECTROGRAM"), "Audio labels not localized");
        visuals.OnResize(900, 720);
        view.Resize(900, 720);
        view.Build();
        Check(state.TimelineRangeMilliseconds() == std::pair{begin, end},
              "Resize changed time range");
        view.Shutdown();
        Check(visuals.CanvasCount() == 0, "Editor Canvas survived shutdown");
        std::cout << "Editor audio view tests passed.\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
