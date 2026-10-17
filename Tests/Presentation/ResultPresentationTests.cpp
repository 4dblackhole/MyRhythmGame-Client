#include "FingerDrumAssets.h"
#include "GameFlow/FingerDrumSceneIds.h"
#include "GameScene/ResultScene/ResultScene.h"
#include "GameScene/ResultScene/Submodules/ResultViewFactory.h"
#include "GameScene/ResultScene/Submodules/Taiko/TaikoResultGraphs.h"
#include "GameScene/RhythmTestScene/RhythmTestScene.h"
#include "Taiko/TaikoMode.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <format>
#include <fstream>
#include <map>
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
        v::ImageHandle LoadImage(const std::filesystem::path &path) override
        {
            std::ifstream file(path, std::ios::binary);
            std::array<unsigned char, 24> header{};
            file.read(reinterpret_cast<char *>(header.data()), static_cast<std::streamsize>(header.size()));
            Check(file.gcount() == static_cast<std::streamsize>(header.size()) && header[0] == 137 &&
                      header[1] == 'P' && header[2] == 'N' && header[3] == 'G',
                  "Result assets must resolve to real embedded/fallback PNGs.");
            const auto dimension = [&header](const std::size_t offset)
            {
                return (static_cast<std::uint32_t>(header[offset]) << 24) |
                       (static_cast<std::uint32_t>(header[offset + 1]) << 16) |
                       (static_cast<std::uint32_t>(header[offset + 2]) << 8) |
                       static_cast<std::uint32_t>(header[offset + 3]);
            };
            const auto id = ++imageId_;
            images_.emplace(id,
                            LoadedImage{{static_cast<float>(dimension(16)), static_cast<float>(dimension(20))}, path});
            return {id};
        }
        v::Size GetImageSize(v::ImageHandle image) const noexcept override
        {
            const auto found = images_.find(image.value);
            return found == images_.end() ? v::Size{} : found->second.size;
        }
        const std::filesystem::path &ImagePath(v::ImageHandle image) const
        {
            return images_.at(image.value).path;
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

      private:
        struct LoadedImage
        {
            v::Size size;
            std::filesystem::path path;
        };
        std::map<std::uint64_t, LoadedImage> images_;
        std::uint64_t imageId_{};
    };
    bool HasText(const v::Visual2DCanvas &canvas, std::wstring_view text)
    {
        const auto packets = canvas.BuildDrawList();
        return std::ranges::any_of(packets, [&](const auto &packet)
                                   { return packet.text && packet.text->value.find(text) != std::wstring::npos; });
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
    const v::Visual2DNode *FindText(const v::Visual2DNode &node, std::wstring_view value)
    {
        const auto *text = node.GetComponent<v::TextVisualComponent>();
        if (text && text->Text() == value)
            return &node;
        for (const auto &child : node.Children())
            if (const auto *found = FindText(*child, value))
                return found;
        return nullptr;
    }
    void TestAccuracyRanks()
    {
        struct RankCase
        {
            std::uint32_t units;
            std::wstring_view rank;
        };
        constexpr std::array<RankCase, 21> cases{{
            {100'000'000, L"Y"},  {99'999'999, L"PERFECT"}, {99'000'000, L"PERFECT"},
            {98'999'999, L"SSS"}, {98'000'000, L"SSS"},     {97'999'999, L"SS"},
            {96'000'000, L"SS"},  {95'999'999, L"S"},       {93'000'000, L"S"},
            {92'999'999, L"A"},   {90'000'000, L"A"},       {89'999'999, L"B"},
            {85'000'000, L"B"},   {84'999'999, L"C"},       {80'000'000, L"C"},
            {79'999'999, L"D"},   {75'000'000, L"D"},       {74'999'999, L"E"},
            {70'000'000, L"E"},   {69'999'999, L"F"},       {0, L"F"},
        }};
        for (const auto &test : cases)
        {
            mode::TaikoPlayResult result;
            result.summary.totalNotes = result.summary.processedNotes = 1;
            result.summary.accuracy = r::AccuracyScore::FromUnits(test.units);
            NullRenderer renderer;
            v::ScreenVisual2DManager visuals;
            visuals.Initialize(renderer, {1280, 720});
            finger_drum::texts::TextCatalog texts;
            mrg::graphics::MeshRenderSystem meshes;
            mrg::graphics::TextRenderSystem textRendering;
            mrg::audio::AudioSystem audio;
            auto view = CreateResultView(result, visuals, texts);
            view->Initialize({meshes, textRendering, renderer, audio, 1280, 720});
            auto *canvas = visuals.FindCanvas(1);
            if (!canvas)
                throw std::runtime_error("Rank view must own a result Canvas.");
            const auto checkRank = [&]
            {
                const auto *rank = FindNode(canvas->Root(), "Result.Rank");
                if (!rank)
                    throw std::runtime_error("Rank must render an image instead of a placeholder.");
                const auto *sprite = rank->GetComponent<v::SpriteVisualComponent>();
                if (!sprite)
                    throw std::runtime_error("Rank must own an image component.");
                Check(renderer.ImagePath(sprite->Style().normalImage).stem().wstring() == test.rank,
                      "Rank PNG must match the exact six-decimal accuracy boundary.");
                const auto bounds = rank->Bounds();
                const auto native = renderer.GetImageSize(sprite->Style().normalImage);
                Check(bounds.width > 0 && bounds.height > 0 && bounds.x >= 824 && bounds.y >= 598 &&
                          bounds.x + bounds.width <= 1096.01F && bounds.y + bounds.height <= 764.01F &&
                          std::abs(bounds.width / bounds.height - native.width / native.height) < .001F,
                      "Every rank must retain its aspect ratio inside the existing rank frame.");
            };
            checkRank();
            view->OnResize(960, 1080);
            checkRank();
            view->Shutdown();
            Check(visuals.CanvasCount() == 0, "Rank view must release its Canvas on shutdown.");
        }
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
    void TestMillisecondHistogram()
    {
        mode::TaikoPlayResult result;
        for (const std::int64_t error : {-1500, -500, -499, 0, 499, 500, 1500})
            result.timingErrors.push_back({.error = r::RhythmDuration{error}, .grade = r::JudgementGrade::Max});
        const v::Rect scatter{0, 0, 400, 100}, histogram{0, 200, 400, 120};
        auto graph = taiko_result_ui::BuildGraphs(result, scatter, histogram);
        const auto firstBar = 2 + result.timingErrors.size();
        Check(graph.maximumBinCount == 3 && graph.rectangles.size() == firstBar + 5,
              "Symmetric half-ms boundaries must produce five occupied 1ms bins and three zero-bin inputs.");
        const auto &zero = graph.rectangles[firstBar + 2].bounds;
        Check(zero.x < histogram.width / 2 && zero.x + zero.width > histogram.width / 2 &&
                  std::abs(zero.height - histogram.height) < .001F,
              "The zero-ms bin must be centered on zero and its height must represent the full count.");
        for (std::size_t i = firstBar + 1; i < graph.rectangles.size(); ++i)
            Check(std::abs(graph.rectangles[i].bounds.x - graph.rectangles[i - 1].bounds.x - 2) < .001F,
                  "Adjacent histogram bars must be exactly 1ms apart on the shared error axis.");
        const auto narrow = taiko_result_ui::BuildGraphs(result, scatter, histogram, 3);
        Check(narrow.maximumBinCount == graph.maximumBinCount && narrow.rectangles.size() == graph.rectangles.size(),
              "A minimum pixel width must never merge the 1ms bins or change their counts.");
        for (std::size_t i = firstBar; i < narrow.rectangles.size(); ++i)
            Check(narrow.rectangles[i].bounds.width >= 3 && narrow.rectangles[i].bounds.x >= histogram.x &&
                      narrow.rectangles[i].bounds.x + narrow.rectangles[i].bounds.width <=
                          histogram.x + histogram.width,
                  "Small viewports must keep bars visible and inside the plot.");
        result.timingErrors = {{.error = r::RhythmDuration{-125'000}}, {.error = r::RhythmDuration{126'000}}};
        graph = taiko_result_ui::BuildGraphs(result, scatter, histogram);
        Check(graph.halfRangeMilliseconds == 130 && graph.maximumBinCount == 1 && graph.rectangles.size() == 6 &&
                  graph.rectangles.back().bounds.width < 2,
              "Expanding the error axis must preserve 1ms bin resolution instead of keeping 41 wide bins.");
        result.timingErrors = {{.error = r::RhythmDuration{-1'000'000'000'000}},
                               {.error = r::RhythmDuration{1'000'000'000'000}}};
        graph = taiko_result_ui::BuildGraphs(result, scatter, histogram);
        Check(graph.rectangles.size() == 6 && graph.maximumBinCount == 1,
              "Large outliers must allocate only the occupied bins.");
        for (const auto &rectangle : graph.rectangles)
            Check(std::isfinite(rectangle.bounds.x) && std::isfinite(rectangle.bounds.width) &&
                      rectangle.bounds.width > 0,
                  "Sparse outlier graphs must keep finite positive geometry.");
    }
    bool SameColor(v::Color left, v::Color right)
    {
        return left.red == right.red && left.green == right.green && left.blue == right.blue &&
               left.alpha == right.alpha;
    }
    void TestHistogramJudgementColors()
    {
        mode::TaikoPlayResult result;
        result.timingErrors = {{.error = r::RhythmDuration{4500}, .grade = r::JudgementGrade::Max},
                               {.error = r::RhythmDuration{4501}, .grade = r::JudgementGrade::Perfect},
                               {.error = r::RhythmDuration{4900}, .grade = r::JudgementGrade::Perfect},
                               {.error = r::RhythmDuration{15000}, .grade = r::JudgementGrade::Great},
                               {.error = r::RhythmDuration{35000}, .grade = r::JudgementGrade::Good},
                               {.error = r::RhythmDuration{70000}, .grade = r::JudgementGrade::Bad}};
        const v::Rect scatter{0, 0, 400, 100}, histogram{0, 200, 400, 120};
        const auto graph = taiko_result_ui::BuildGraphs(result, scatter, histogram);
        const auto firstBar = 2 + result.timingErrors.size();
        Check(graph.maximumBinCount == 3 && graph.rectangles.size() == firstBar + 5,
              "Peak frequency must sum every judgement in the same 1ms bin.");
        const auto &max = graph.rectangles[firstBar];
        const auto &perfect = graph.rectangles[firstBar + 1];
        Check(max.bounds.x == perfect.bounds.x && max.bounds.width == perfect.bounds.width &&
                  std::abs(max.bounds.y - histogram.y) < .001F &&
                  std::abs(max.bounds.height - histogram.height / 3) < .001F &&
                  std::abs(perfect.bounds.y - max.bounds.y - max.bounds.height) < .001F &&
                  std::abs(perfect.bounds.y + perfect.bounds.height - histogram.y - histogram.height) < .001F,
              "Mixed MAX/PERFECT must stack in one bin, preserving each count and the total height.");
        for (const auto [bar, sample] :
             {std::pair{0, 0}, std::pair{1, 1}, std::pair{2, 3}, std::pair{3, 4}, std::pair{4, 5}})
            Check(SameColor(graph.rectangles[firstBar + bar].color, graph.rectangles[2 + sample].color),
                  "Every histogram judgement color must match the corresponding scatter sample.");
        std::ranges::reverse(result.timingErrors);
        const auto reversed = taiko_result_ui::BuildGraphs(result, scatter, histogram);
        for (std::size_t i = firstBar; i < graph.rectangles.size(); ++i)
            Check(SameColor(graph.rectangles[i].color, reversed.rectangles[i].color) &&
                      graph.rectangles[i].bounds.x == reversed.rectangles[i].bounds.x &&
                      graph.rectangles[i].bounds.y == reversed.rectangles[i].bounds.y &&
                      graph.rectangles[i].bounds.height == reversed.rectangles[i].bounds.height,
                  "Histogram stack order must be stable regardless of the input record order.");
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
        Check(data.rectangles.size() == 8,
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
        auto view = CreateResultView(*result, visuals, texts,
                                     {L"Recorded song", L"Recorded creator", true, L"Recorded artist"});
        view->Initialize({meshes, textRendering, renderer, audio, 1280, 720});
        view->SetVisible(true);
        auto *canvas = visuals.FindCanvas(1);
        if (!canvas)
            throw std::runtime_error("Result Canvas must exist.");
        Check(HasText(*canvas, L"정확도") && HasText(*canvas, L"Recorded song") &&
                  HasText(*canvas, L"Recorded creator") && HasText(*canvas, L"Recorded artist") &&
                  HasText(*canvas, std::format(L"{:.6f}%", taiko->summary.accuracy.Percent())),
              "Result view must display the recorded six-decimal accuracy.");
        const auto packets = canvas->BuildDrawList();
        const auto *banner = FindNode(canvas->Root(), "Result.SongBanner");
        const auto *song = FindText(canvas->Root(), L"Recorded song");
        const auto *artist = FindText(canvas->Root(), L"Recorded artist");
        const auto *creator = FindText(canvas->Root(), L"Recorded creator");
        const auto *rank = FindNode(canvas->Root(), "Result.RankFrame");
        if (!banner || !song || !artist || !creator || !rank)
            throw std::runtime_error("Result must retain each metadata element independently.");
        Check(banner->Bounds().width == 1920 && banner->Bounds().height == 88 && song->Bounds().x == 444 &&
                  song->Bounds().y == 1034 && artist->Bounds().x == 444 && artist->Bounds().y == 1005 &&
                  creator->Bounds().x == 744 && creator->Bounds().y == 396 && rank->Bounds().x == 705.75F,
              "Song/artist occupy the new top banner, while rank and creator match the centered Penpot column.");
        for (const auto &packet : packets)
            if (packet.type == v::DrawPacketType::RectangleBatch)
                for (const auto &primitive : *packet.rectangles)
                {
                    const auto b = primitive.bounds;
                    const bool inHistogram =
                        b.x >= 1420 && b.x + b.width <= 1844.01F && b.y >= 692 && b.y + b.height <= 898.01F;
                    const bool inScatter =
                        b.x >= 1434 && b.x + b.width <= 1844.01F && b.y >= 272 && b.y + b.height <= 460.01F;
                    Check(inHistogram || inScatter,
                          "Actual graph samples must remain inside the narrower design plots.");
                }
        Check(std::ranges::count(packets, v::DrawPacketType::RectangleBatch, &v::DrawPacket::type) == 1 &&
                  packets.size() < 200,
              "Large result graphs must use one batch with constant widget count.");
        mrg::platform::InputState input;
        for (int i = 0; i < 20; ++i)
            Check(view->Update(input) == ResultAction::None, "Idle result view must not leave the Scene.");
        Check(canvas->BuildDrawList().size() == packets.size(), "Idle updates must not rebuild graph nodes.");
        const auto *retry = FindNode(canvas->Root(), "Result.Retry");
        const auto *replay = FindNode(canvas->Root(), "Result.Replay");
        if (!retry || !replay)
            throw std::runtime_error("Result retry/replay nodes must exist.");
        Check(retry->IsEnabled() && !replay->IsEnabled(),
              "Retry is available for a played selection; replay remains disabled.");
        Check(replay->Bounds().x == 1344 && retry->Bounds().x == 1616 && replay->Bounds().width == 256 &&
                  retry->Bounds().width == 256,
              "Actions must fit the new 528px right column.");
        const auto click = [&](const v::Visual2DNode &button)
        {
            const auto b = button.BoundsInCanvas();
            v::Visual2DInputRouter router;
            v::PointerInput p;
            p.available = true;
            p.position = {b.x + b.width / 2, b.y + b.height / 2};
            p.leftButtonDown = p.leftButtonPressed = true;
            router.Process(*canvas, p);
            p.leftButtonDown = p.leftButtonPressed = false;
            p.leftButtonReleased = true;
            router.Process(*canvas, p);
            router.Reset(*canvas);
        };
        click(*retry);
        Check(view->Update(input) == ResultAction::Retry, "A retry click must emit the neutral retry action.");
        click(*replay);
        Check(view->Update(input) == ResultAction::None, "Disabled replay must not emit any action.");
        texts.SetLanguage(finger_drum::texts::Language::English);
        Check(view->Update(input) == ResultAction::None && HasText(*canvas, L"Judgement counts"),
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
        Check(view->Update(input) == ResultAction::Back,
              "The result button click must request returning to song select.");
        pointerRouter.Reset(*canvas);
        view->Shutdown();
        Check(visuals.CanvasCount() == 0, "Result view must release its owned Canvas.");
        taiko->timingErrors.clear();
        data = taiko_result_ui::BuildGraphs(*taiko, scatter, histogram);
        Check(data.rectangles.size() == 2 && data.maximumBinCount == 0, "Empty graphs must not invent samples.");
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
            launch_ = std::make_shared<finger_drum::GameplayLaunchStore>();
            PrepareSession();
            Check(scenes.RegisterScene<RhythmTestScene>(std::string(finger_drum::scene_ids::RhythmTest),
                                                        mrg::scene::SceneRetention::DestroyOnExit, launch_,
                                                        std::ref(AudioPlayback()), std::ref(ScreenVisuals()), false,
                                                        std::ref(texts_), std::ref(pending_)),
                  "Register production gameplay Scene.");
            Check(scenes.RegisterScene(std::string(finger_drum::scene_ids::Result),
                                       mrg::scene::SceneRetention::DestroyOnExit,
                                       [this]
                                       {
                                           ++resultCreations_;
                                           resultData_ = pending_.get();
                                           Check(resultData_ != nullptr, "Factory requires moved play data.");
                                           return std::make_unique<ResultScene>(
                                               std::move(pending_), ScreenVisuals(), texts_,
                                               ResultViewContext{L"Retry fixture", L"Regression", true},
                                               [this](mrg::scene::SceneManager &manager)
                                               {
                                                   PrepareSession();
                                                   return manager.ChangeScene(finger_drum::scene_ids::RhythmTest);
                                               });
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
            if (Scenes().CurrentSceneId() == finger_drum::scene_ids::Result && !retryRequested_)
            {
                const auto &summary = resultData_->Summary();
                Check(summary.endReason == mode::PlayEndReason::Completed && summary.processedNotes == 1 &&
                          summary.unprocessedNotes == 0 && context.totalSeconds >= 4.9,
                      "All notes must finalize before the three-second Result delay.");
                Check(resultCreations_ == 1 && !pending_ && ScreenVisuals().CanvasCount() == 1 &&
                          !ScreenVisuals().FindCanvas(1),
                      "The moved result must survive gameplay Scene/Canvas destruction.");
                auto *canvas = ScreenVisuals().FindCanvas(2);
                if (!canvas)
                    throw std::runtime_error("The production Result Canvas must be active.");
                const auto *retry = FindNode(canvas->Root(), "Result.Retry");
                if (!retry)
                    throw std::runtime_error("The retry button must exist.");
                Check(retry->IsEnabled(), "The retry button must be enabled.");
                const auto bounds = retry->BoundsInCanvas();
                v::PointerInput p;
                p.available = true;
                p.position = {bounds.x + bounds.width / 2, bounds.y + bounds.height / 2};
                p.leftButtonDown = p.leftButtonPressed = true;
                v::Visual2DInputRouter router;
                router.Process(*canvas, p);
                p.leftButtonDown = p.leftButtonPressed = false;
                p.leftButtonReleased = true;
                router.Process(*canvas, p);
                router.Reset(*canvas);
                retryRequested_ = true;
            }
            else if (Scenes().CurrentSceneId() == finger_drum::scene_ids::RhythmTest && retryRequested_ &&
                     !returnRequested_)
            {
                Check(sessionCreations_ == 2 && resultCreations_ == 1 && !pending_ &&
                          ScreenVisuals().CanvasCount() == 1 && !ScreenVisuals().FindCanvas(2),
                      "Retry must construct fresh gameplay after destroying Result and its Canvas.");
                Check(Scenes().ChangeScene(finger_drum::scene_ids::Lobby), "Queue returning after retry.");
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
        void PrepareSession()
        {
            finger_drum::chart::PatternDocument pattern;
            pattern.name = "Automatic completion regression";
            pattern.mode = "Taiko";
            pattern.notes.push_back({.keyType = 1});
            auto loaded = mode::TaikoMode{}.CreateSession(pattern);
            Check(loaded.Succeeded(), "Transition fixture must load.");
            launch_->SetValidated({}, std::move(loaded.session));
            ++sessionCreations_;
        }
        bool &verified_;
        bool retryRequested_{};
        bool returnRequested_{};
        std::size_t sessionCreations_{};
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
        std::string error;
        Check(finger_drum::assets::InitializeBuiltInAssets(error), error.c_str());
        TestAccuracyRanks();
        TestMillisecondHistogram();
        TestHistogramJudgementColors();
        TestGraphsAndView();
        bool verified{};
        Check(mrg::Run(std::make_unique<TransitionGame>(verified)) == 0 && verified,
              "Actual D3D12/FMOD Result transition must complete.");
        std::puts("Result ranks (21 boundary cases), graphs, six-decimal display, ownership and actual deferred "
                  "transition passed.");
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
