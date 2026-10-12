#include "FingerDrumAssets.h"
#include "GameFlow/FingerDrumSceneIds.h"
#include "GameScene/ResultScene/ResultScene.h"
#include "GameScene/ResultScene/Submodules/ResultViewFactory.h"
#include "GameScene/ResultScene/Submodules/Taiko/TaikoResultGraphs.h"
#include "GameScene/RhythmTestScene/RhythmTestScene.h"
#include "Taiko/TaikoMode.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <format>
#include <stdexcept>

namespace
{
    namespace v = mrg::visual2d;
    namespace mode = finger_drum::mode;
    namespace r = finger_drum::rhythm;
    void Check(bool condition, const char *message)
    {
        if (!condition)
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
    bool HasText(const v::Visual2DCanvas &canvas, std::wstring_view text)
    {
        const auto packets = canvas.BuildDrawList();
        return std::ranges::any_of(packets,
                                   [&](const auto &packet) { return packet.text.find(text) != std::wstring::npos; });
    }
    const v::Visual2DNode *FindNode(const v::Visual2DNode &node, std::string_view name)
    {
        if (node.Name() == name)
            return &node;
        for (const auto &child : node.Children())
            if (const auto *found = FindNode(*child, name))
                return found;
        return nullptr;
    }
    std::unique_ptr<mode::IPlayResult> FixtureResult()
    {
        finger_drum::chart::PatternDocument pattern;
        pattern.mode = "Taiko";
        pattern.name = "Result regression";
        for (int i = 0; i < 3; ++i)
            pattern.notes.push_back(
                {.position = {0, {i, 4}}, .keyType = 1, .sourceOrder = static_cast<std::size_t>(i)});
        auto loaded = mode::TaikoMode{}.CreateSession(pattern);
        Check(loaded.Succeeded(), "Result fixture must load.");
        loaded.session->SetResultInitialTime(r::RhythmTime{-2'000'000});
        for (const std::int64_t us : {0, 510'000, 990'000})
            static_cast<void>(loaded.session->ProcessInput('F', r::InputEdge::Pressed, r::RhythmTime{us}));
        return loaded.session->TakeResult(mode::PlayEndReason::Completed, r::RhythmTime{1'000'000});
    }
    void TestGraphsAndView()
    {
        auto result = FixtureResult();
        auto *taiko = dynamic_cast<mode::TaikoPlayResult *>(result.get());
        if (!taiko)
            throw std::runtime_error("Taiko result must retain its concrete data.");
        const v::Rect scatter{0, 0, 500, 150}, histogram{550, 0, 500, 150};
        auto data = taiko_result_ui::BuildGraphs(*taiko, scatter, histogram);
        Check(data.maximumBinCount == 1 && data.halfRangeMilliseconds == 100 && data.startSeconds == -2 &&
                  data.endSeconds == 1,
              "Graph axes must cover real time/error samples and retain signed timing.");
        Check(data.rectangles.size() == 10,
              "Three timing inputs must yield three scatter points and three histogram bars.");
        for (const auto &rect : data.rectangles)
            Check(std::isfinite(rect.bounds.x) && std::isfinite(rect.bounds.y) && rect.bounds.width > 0 &&
                      rect.bounds.height > 0,
                  "Graph primitives must have valid bounded geometry.");
        // Stress only graph data, without creating thousands of widget nodes.
        const auto sample = taiko->timingErrors.front();
        taiko->timingErrors.insert(taiko->timingErrors.end(), 10'000, sample);
        NullRenderer renderer;
        v::ScreenVisual2DManager visuals;
        visuals.Initialize(renderer, {1280, 720});
        finger_drum::texts::TextCatalog texts;
        mrg::graphics::MeshRenderSystem meshes;
        mrg::graphics::TextRenderSystem textRendering;
        mrg::audio::AudioSystem audio;
        auto view = CreateResultView(*result, visuals, texts);
        view->Initialize({meshes, textRendering, renderer, audio, 1280, 720});
        view->SetVisible(true);
        auto *canvas = visuals.FindCanvas(1);
        if (!canvas)
            throw std::runtime_error("Result Canvas must exist.");
        Check(HasText(*canvas, L"플레이 결과") &&
                  HasText(*canvas, std::format(L"{:.6f}%", taiko->summary.accuracy.Percent())),
              "Result view must display the recorded six-decimal accuracy.");
        const auto packets = canvas->BuildDrawList();
        Check(std::ranges::count(packets, v::DrawPacketType::RectangleBatch, &v::DrawPacket::type) == 1 &&
                  packets.size() < 60,
              "Large result graphs must use one batch with constant widget count.");
        mrg::platform::InputState input;
        for (int i = 0; i < 20; ++i)
            Check(!view->Update(input), "Idle result view must not leave the Scene.");
        Check(canvas->BuildDrawList().size() == packets.size(), "Idle updates must not rebuild graph nodes.");
        texts.SetLanguage(finger_drum::texts::Language::English);
        Check(!view->Update(input) && HasText(*canvas, L"Play Result"),
              "Result strings must follow the shared language catalog.");
        for (const v::Size size : {v::Size{1024, 768}, v::Size{2560, 1080}, v::Size{720, 1280}})
        {
            view->OnResize(static_cast<std::uint32_t>(size.width), static_cast<std::uint32_t>(size.height));
            const auto *root = FindNode(canvas->Root(), "Result.Content");
            if (!root)
                throw std::runtime_error("Result content node must exist after resize.");
            const auto bounds = root->BoundsInCanvas();
            Check(bounds.width <= canvas->LogicalSize().width && bounds.height <= canvas->LogicalSize().height,
                  "The authored result panel must fit narrow and wide screens.");
        }
        const auto *back = FindNode(canvas->Root(), "Result.Back");
        if (!back)
            throw std::runtime_error("Result return button must exist.");
        const auto bounds = back->BoundsInCanvas();
        v::Visual2DInputRouter pointerRouter;
        v::PointerInput pointer;
        pointer.available = true;
        pointer.position = {bounds.x + bounds.width / 2, bounds.y + bounds.height / 2};
        pointer.leftButtonDown = pointer.leftButtonPressed = true;
        pointerRouter.Process(*canvas, pointer);
        pointer.leftButtonDown = pointer.leftButtonPressed = false;
        pointer.leftButtonReleased = true;
        pointerRouter.Process(*canvas, pointer);
        Check(view->Update(input), "The result button click must request returning to song select.");
        pointerRouter.Reset(*canvas);
        view->Shutdown();
        Check(visuals.CanvasCount() == 0, "Result view must release its owned Canvas.");
        taiko->timingErrors.clear();
        data = taiko_result_ui::BuildGraphs(*taiko, scatter, histogram);
        Check(data.rectangles.size() == 4 && data.maximumBinCount == 0, "Empty graphs must not invent samples.");
    }
    class ReturnRoute final : public mrg::scene::GameScene
    {
      public:
        void Initialize(const mrg::EngineServices &) override
        {
        }
        void Update(const mrg::UpdateContext &, mrg::scene::SceneManager &) override
        {
        }
        void Render(const mrg::graphics::RenderContext &) override
        {
        }
        void Shutdown() noexcept override
        {
        }
    };
    class TransitionGame final : public mrg::scene::SceneGameClient
    {
      public:
        explicit TransitionGame(bool &verified) : verified_(verified)
        {
        }
        mrg::EngineConfig GetEngineConfig() const override
        {
            mrg::EngineConfig config;
            config.showWindow = false;
            config.audio.allowNoSoundFallback = false;
            config.renderRateOverrideHz = 60;
            return config;
        }

      protected:
        std::string_view InitialSceneId() const noexcept override
        {
            return finger_drum::scene_ids::RhythmTest;
        }
        void RegisterScenes(mrg::scene::SceneManager &scenes) override
        {
            finger_drum::chart::PatternDocument pattern;
            pattern.name = "Automatic completion regression";
            pattern.mode = "Taiko";
            pattern.notes.push_back({.keyType = 1});
            auto loaded = mode::TaikoMode{}.CreateSession(pattern);
            Check(loaded.Succeeded(), "Transition fixture must load.");
            launch_ = std::make_shared<finger_drum::GameplayLaunchStore>();
            launch_->SetValidated({}, std::move(loaded.session));
            Check(scenes.RegisterScene<RhythmTestScene>(std::string(finger_drum::scene_ids::RhythmTest),
                                                        mrg::scene::SceneRetention::DestroyOnExit, launch_,
                                                        std::ref(AudioPlayback()), std::ref(ScreenVisuals()), false,
                                                        std::ref(texts_), std::ref(pending_)),
                  "Register production gameplay Scene.");
            Check(scenes.RegisterScene(
                      std::string(finger_drum::scene_ids::Result), mrg::scene::SceneRetention::DestroyOnExit,
                      [this]
                      {
                          ++resultCreations_;
                          resultData_ = pending_.get();
                          Check(resultData_ != nullptr, "Factory requires moved play data.");
                          return std::make_unique<ResultScene>(std::move(pending_), ScreenVisuals(), texts_);
                      }),
                  "Register production Result Scene.");
            Check(scenes.RegisterScene<ReturnRoute>(std::string(finger_drum::scene_ids::Lobby),
                                                    mrg::scene::SceneRetention::KeepAlive),
                  "Register test return route.");
            Check(resultCreations_ == 0 && !pending_, "Result construction must be deferred.");
        }
        void OnClientUpdated(const mrg::UpdateContext &context) override
        {
            Check(context.totalSeconds < 12, "Result transition regression timed out.");
            if (Scenes().CurrentSceneId() == finger_drum::scene_ids::Result && !returnRequested_)
            {
                const auto &summary = resultData_->Summary();
                Check(summary.endReason == mode::PlayEndReason::Completed && summary.processedNotes == 1 &&
                          summary.unprocessedNotes == 0 && context.totalSeconds >= 4.9,
                      "All notes must finalize before the three-second Result delay.");
                Check(resultCreations_ == 1 && !pending_ && ScreenVisuals().CanvasCount() == 1 &&
                          !ScreenVisuals().FindCanvas(1),
                      "The moved result must survive gameplay Scene/Canvas destruction.");
                Check(Scenes().ChangeScene(finger_drum::scene_ids::Lobby), "Queue returning to song select.");
                returnRequested_ = true;
            }
            else if (Scenes().CurrentSceneId() == finger_drum::scene_ids::Lobby && returnRequested_)
            {
                Check(ScreenVisuals().CanvasCount() == 0 && resultCreations_ == 1,
                      "Leaving Result must destroy its transient Canvas.");
                verified_ = true;
                Scenes().Quit();
            }
        }

      private:
        bool &verified_;
        bool returnRequested_{};
        std::size_t resultCreations_{};
        std::shared_ptr<finger_drum::GameplayLaunchStore> launch_;
        std::unique_ptr<mode::IPlayResult> pending_;
        const mode::IPlayResult *resultData_{};
        finger_drum::texts::TextCatalog texts_;
    };
} // namespace
int main()
{
    try
    {
        TestGraphsAndView();
        std::string error;
        Check(finger_drum::assets::InitializeBuiltInAssets(error), error.c_str());
        bool verified{};
        Check(mrg::Run(std::make_unique<TransitionGame>(verified)) == 0 && verified,
              "Actual D3D12/FMOD Result transition must complete.");
        std::puts("Result graphs, six-decimal display, ownership and actual deferred transition passed.");
        return 0;
    }
    catch (const std::exception &error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
    catch (...)
    {
        std::fputs("Unknown Result test failure.\n", stderr);
        return 1;
    }
}
