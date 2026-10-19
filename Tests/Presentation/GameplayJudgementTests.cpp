#include "App/AssetPaths.h"
#include "FingerDrumAssets.h"
#include "GameScene/RhythmTestScene/Submodules/GameplayJudgementView.h"
#include "GameScene/RhythmTestScene/Submodules/GameplayPresenter.h"
#include "Parsing/ChartParser.h"
#include "Presentation/LaneKeyBeam.h"
#include "Taiko/TaikoMode.h"
#include "Time/RhythmTimer.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <limits>
#include <map>
#include <stdexcept>

namespace
{
    namespace v = mrg::visual2d;
    namespace r = finger_drum::rhythm;
    void Check(const bool condition, const char *message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }
    bool Near(const float left, const float right)
    {
        return std::abs(left - right) < 0.001F;
    }
    v::Visual2DNode *Find(v::Visual2DNode &root, const std::string_view name)
    {
        if (root.Name() == name)
            return &root;
        for (const auto &child : root.Children())
            if (auto *found = Find(*child, name))
                return found;
        return nullptr;
    }
    v::Visual2DNode &Node(v::Visual2DNode &root, const std::string_view name)
    {
        auto *node = Find(root, name);
        Check(node != nullptr, "Expected judgement node missing.");
        return *node;
    }
    std::size_t NodeCount(const v::Visual2DNode &root)
    {
        std::size_t count = 1;
        for (const auto &child : root.Children())
            count += NodeCount(*child);
        return count;
    }
    class AssetRenderer final : public mrg::graphics::Visual2DRenderSystem
    {
      public:
        std::size_t loads{};
        v::ImageHandle LoadImage(const std::filesystem::path &path) override
        {
            // Read the real embedded/fallback PNG metadata, without a GPU.
            std::ifstream file(path, std::ios::binary);
            std::array<unsigned char, 24> header{};
            file.read(reinterpret_cast<char *>(header.data()), static_cast<std::streamsize>(header.size()));
            Check(file.gcount() == static_cast<std::streamsize>(header.size()) && header[0] == 137 && header[1] == 'P',
                  "Judgement sprite must resolve to a real PNG.");
            const auto dimension = [&header](const std::size_t offset)
            {
                return (static_cast<std::uint32_t>(header[offset]) << 24) |
                       (static_cast<std::uint32_t>(header[offset + 1]) << 16) |
                       (static_cast<std::uint32_t>(header[offset + 2]) << 8) |
                       static_cast<std::uint32_t>(header[offset + 3]);
            };
            const auto id = ++loads;
            sizes_[id] = {static_cast<float>(dimension(16)), static_cast<float>(dimension(20))};
            return {id};
        }
        v::Size GetImageSize(const v::ImageHandle image) const noexcept override
        {
            const auto found = sizes_.find(image.value);
            return found != sizes_.end() ? found->second : v::Size{};
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
        std::map<std::uint64_t, v::Size> sizes_;
    };
    r::NoteEvent Event(const r::NoteEventType type, const r::JudgementGrade grade, const r::RhythmDuration error = {})
    {
        r::NoteEvent event;
        event.type = type;
        event.judgement = {grade, error, 1.0};
        return event;
    }
    void CheckOverlay(v::Visual2DNode &root, const std::string_view word)
    {
        constexpr std::array names{"MAX", "PERFECT", "GREAT", "GOOD", "BAD", "MISS", "POOR", "OK"};
        for (const auto name : names)
            Check(Node(root, "Lane.Judgement." + std::string(name)).IsVisible() == (word == name),
                  "Exactly the expected judgement image must be visible.");
    }
    void TestGuideAndOverlays()
    {
        AssetRenderer renderer;
        v::ScreenVisual2DManager visuals;
        visuals.Initialize(renderer, {1280.0F, 720.0F});
        v::Visual2DCanvas canvas;
        auto &lane = canvas.Root().CreateChild("Lane");
        lane.Transform().SetRotationRollPitchYaw(0.0F, 0.0F, -DirectX::XM_PIDIV2);
        auto &circle = v::CreateSprite(lane, {30.0F, 40.0F, 96.0F, 96.0F}, {}, "Circle");
        GameplayJudgementView view;
        finger_drum::texts::TextCatalog texts;
        const r::AccuracyRange base;
        view.Initialize(visuals, canvas, circle, base, texts);
        auto &root = canvas.Root();
        auto &marker = Node(root, "Judgement.Marker");
        auto &bad = Node(root, "Judgement.Band.4");
        auto &margin = Node(root, "Judgement.Margin");
        Check(!marker.IsVisible() && Near(bad.NodeSize().width, 400.0F), "Initial guide uses base BAD +/-90ms.");
        Check(Near(margin.NodeSize().width, 444.44445F), "Guide includes 10ms margin on each side.");
        for (std::size_t index = 0; index < base.Bands().size(); ++index)
        {
            auto &band = Node(root, "Judgement.Band." + std::to_string(index));
            const auto color = band.GetComponent<v::SpriteVisualComponent>()->Style().normal;
            const auto beamColor =
                finger_drum::presentation::LaneKeyBeam::GradeColor(static_cast<r::JudgementGrade>(index));
            Check(Near(color.red, beamColor.red) && Near(color.green, beamColor.green) &&
                      Near(color.blue, beamColor.blue),
                  "Each independent band must match the key-beam grade color.");
        }
        const std::array words{"MAX", "PERFECT", "GREAT", "GOOD", "BAD"};
        for (std::size_t index = 0; index < words.size(); ++index)
        {
            view.Reset();
            const auto type = index == 4 ? r::NoteEventType::InputRejected : r::NoteEventType::HitAccepted;
            view.Present(Event(type, static_cast<r::JudgementGrade>(index), -base.Bands()[index].halfWindow));
            CheckOverlay(root, words[index]);
            Check(marker.IsVisible() && marker.Bounds().x < 0.0F, "Early input must place marker to the left.");
            const auto overlay = Node(root, "Lane.Judgement." + std::string(words[index])).BoundsInCanvas();
            const auto target = circle.BoundsInCanvas();
            Check(Near(overlay.x + overlay.width * 0.5F, target.x + target.width * 0.5F) &&
                      Near(overlay.y + overlay.height * 0.5F, target.y + target.height * 0.5F),
                  "Overlay and judgement circle must have the same world centre.");
            const auto world = DirectX::XMLoadFloat4x4(
                &Node(root, "Lane.Judgement." + std::string(words[index])).Transform().WorldMatrix());
            const auto right = DirectX::XMVector3TransformNormal(DirectX::XMVectorSet(1, 0, 0, 0), world);
            Check(Near(DirectX::XMVectorGetX(right), 1.0F) && Near(DirectX::XMVectorGetY(right), 0.0F),
                  "Lettering must remain upright in the rotated lane.");
        }
        view.Reset();
        view.Present(Event(r::NoteEventType::HitAccepted, r::JudgementGrade::Good, r::RhythmDuration{54'500}));
        Check(marker.Bounds().x > 0.0F, "Late input must place marker to the right.");
        view.Update(0.1);
        Check(Near(Node(root, "Lane.Judgement.GOOD").GetComponent<v::SpriteVisualComponent>()->Style().normal.alpha,
                   0.5F),
              "Judgement overlay must fade over 200ms.");
        view.Update(0.1);
        CheckOverlay(root, "");
        Check(marker.IsVisible(), "Recent hits remain visible until displaced from the 32-hit ring.");

        view.Reset();
        view.Present(Event(r::NoteEventType::InputRejected, r::JudgementGrade::Max));
        CheckOverlay(root, "POOR");
        Check(Near(marker.Bounds().x, 0.0F), "Zero-error POOR keeps the actual input timing.");
        view.Reset();
        view.Present(Event(r::NoteEventType::InputRejected, r::JudgementGrade::Miss));
        CheckOverlay(root, "");
        Check(!marker.IsVisible(), "Empty/out-of-range input must not show POOR or a marker.");
        view.Present(Event(r::NoteEventType::TickAccepted, r::JudgementGrade::Unjudged));
        CheckOverlay(root, "");
        view.Present(Event(r::NoteEventType::Missed, r::JudgementGrade::Miss, r::RhythmDuration{500'000}));
        CheckOverlay(root, "MISS");
        Check(!marker.IsVisible(), "Automatic MISS must not invent an input marker.");

        const auto count = NodeCount(root);
        const auto loads = renderer.loads;
        const r::AccuracyRange sync{"sync", 50, r::AccuracyRange::DefaultLevel50Bands(), r::RhythmDuration{10'000}};
        auto synced = Event(r::NoteEventType::HitAccepted, r::JudgementGrade::Good);
        synced.judgement = sync.Evaluate({}, r::RhythmTime{60'000});
        view.Present(synced);
        CheckOverlay(root, "GOOD");
        Check(Near(bad.NodeSize().width, 400.0F), "Syncopation must not resize the base guide.");
        view.Reset();
        view.Present(Event(r::NoteEventType::InputRejected, r::JudgementGrade::Bad, r::RhythmDuration{500'000}));
        Check(Near(marker.Bounds().x, margin.NodeSize().width * 0.5F), "Marker must be clamped to the visible guide.");
        view.OnResize(200.0F);
        Check(Near(margin.NodeSize().width, 160.0F), "Wide ranges must fit a narrow viewport.");
        Check(Near(marker.Bounds().x, 80.0F), "Resize must preserve the timing position.");
        view.OnResize(1280.0F);
        Check(Near(bad.NodeSize().width, 400.0F), "Resize restores the fixed time scale.");
        Check(NodeCount(root) == count && renderer.loads == loads, "Repeated hits/resize must reuse nodes and images.");
        view.Reset();
        for (int i = 0; i < 32; ++i)
            view.Present(Event(r::NoteEventType::HitAccepted, r::JudgementGrade::Perfect, r::RhythmDuration{i * 1000}));
        view.Update(1);
        Check(marker.IsVisible() && !Node(root, "Judgement.RetiringMarker.0").IsVisible(),
              "All 32 retained hits persist without retiring a slot early.");
        for (int i = 32; i < 41; ++i)
            view.Present(Event(r::NoteEventType::HitAccepted, r::JudgementGrade::Great, r::RhythmDuration{i * 1000}));
        Check(Near(marker.Bounds().x, 32 * 1000 * 400.0F / 180'000), "Ring wrap must replace the oldest hit.");
        for (int i = 0; i < 8; ++i)
            Check(Node(root, "Judgement.RetiringMarker." + std::to_string(i)).IsVisible(),
                  "At most eight displaced hits have reusable fade slots.");
        view.Update(.1);
        auto &retiring = Node(root, "Judgement.RetiringMarker.0");
        Check(Near(retiring.GetComponent<v::SpriteVisualComponent>()->Style().normal.alpha, .5F) && marker.IsVisible(),
              "Only displaced hits fade over 200ms.");
        view.Update(.1);
        Check(!retiring.IsVisible() && marker.IsVisible(), "Retiring hits expire while the recent 32 remain.");
        Check(NodeCount(root) == count && renderer.loads == loads, "The hit rings allocate no per-hit nodes/images.");
        view.Shutdown();
        view.Update(1.0);
        view.OnResize(100.0F);
        view.Present(synced);
        CheckOverlay(root, "");

        v::Visual2DCanvas strictCanvas;
        auto &strictCircle = v::CreateSprite(strictCanvas.Root(), {0.0F, 0.0F, 96.0F, 96.0F}, {}, "Circle");
        GameplayJudgementView strictView;
        strictView.Initialize(visuals, strictCanvas, strictCircle, r::AccuracyRange{"strict", 100}, texts);
        Check(Near(Node(strictCanvas.Root(), "Judgement.Band.4").NodeSize().width, 200.0F),
              "JudgeLevel 100 must halve the base BAD band length.");
        strictView.Shutdown();
    }
    void CheckGearTransform(v::Visual2DNode &root)
    {
        auto &gear = Node(root, "ScrollGear");
        Check(Near(Node(root, "ScrollGear.Border").BoundsInCanvas().y, 0),
              "The outer gear bottom must touch the screen's horizontal centre.");
        std::vector<std::pair<v::Visual2DNode *, v::Rect>> descendants;
        const auto collect = [&](const auto &self, v::Visual2DNode &parent) -> void
        {
            for (const auto &child : parent.Children())
            {
                descendants.emplace_back(child.get(), child->BoundsInCanvas());
                self(self, *child);
            }
        };
        collect(collect, gear);
        for (const auto name : {"ScrollGear.Border", "ScrollGear.Surface", "ScrollGear.Lane", "InputPresentation"})
            Check(Node(root, name).Parent() == &gear, "Every gear branch must share the same transform parent.");
        const auto hud = Node(root, "Hud.GameProgress").BoundsInCanvas();
        const auto position = gear.Position();
        gear.SetPosition({position.x + 37, position.y + 29});
        for (const auto &[node, before] : descendants)
        {
            const auto after = node->BoundsInCanvas();
            Check(Near(after.x - before.x, 37) && Near(after.y - before.y, 29) && Near(after.width, before.width) &&
                      Near(after.height, before.height),
                  "One gear transform must move all nested keys, lane parts, notes and judgements together.");
        }
        const auto unchangedHud = Node(root, "Hud.GameProgress").BoundsInCanvas();
        Check(Near(hud.x, unchangedHud.x) && Near(hud.y, unchangedHud.y),
              "Screen HUD must stay independent of the gear transform.");
        gear.SetPosition(position);
    }
    void TestGameplayProgress()
    {
        AssetRenderer renderer;
        v::ScreenVisual2DManager visuals;
        visuals.Initialize(renderer, {1280, 720});
        finger_drum::chart::PatternDocument pattern;
        pattern.mode = "Taiko";
        pattern.baseBpm = 120;
        // A late normal head must not hide an earlier long note's later tail.
        pattern.notes = {{{1, {1, 4}}, 1}, {{0, {}}, 11, 1}, {{2, {}}, 11, 2}};
        auto loaded = finger_drum::mode::TaikoMode{}.CreateSession(pattern);
        Check(loaded.Succeeded(), "Progress fixture must load.");
        mrg::graphics::MeshRenderSystem meshes;
        mrg::graphics::TextRenderSystem textRendering;
        mrg::audio::AudioSystem audio;
        finger_drum::texts::TextCatalog texts;
        GameplayPresenter presenter(visuals, texts);
        presenter.ConfigureBackground(mrg_client::asset_paths::skin::title::Center(), 70, -25);
        presenter.Initialize({meshes, textRendering, renderer, audio, 1280, 720}, *loaded.session);
        auto &root = visuals.FindCanvas(1)->Root();
        auto &inputPanel = Node(root, "Input.Panel");
        std::size_t keyIndex = 0;
        for (const auto &child : inputPanel.Children())
        {
            if (child->Name() != "Input.Key.PressFlash")
                continue;
            const auto color = child->GetComponent<v::SpriteVisualComponent>()->Style().normal;
            const bool kat = keyIndex == 0 || keyIndex == 3;
            Check(kat ? color.blue > color.red : color.red > color.blue,
                  "Both key flash sides must use their Don red / Kat blue ambient color.");
            ++keyIndex;
        }
        Check(keyIndex == 4, "All four key flashes must exist.");
        auto &progress = Node(root, "Hud.GameProgress");
        auto &track = Node(root, "Progress.Track");
        auto &fill = Node(root, "Progress.Fill");
        auto &marker = Node(root, "Progress.Marker");
        auto &currentText = *Node(root, "Progress.CurrentTime").GetComponent<v::TextVisualComponent>();
        const auto &totalText = *Node(root, "Progress.TotalTime").GetComponent<v::TextVisualComponent>();
        Check(totalText.Text() == L"0:04", "Total time includes the last long tail, without judgement grace.");
        const auto checkFraction = [&](const float expected)
        {
            Check(Near(fill.Bounds().width, track.Bounds().width * expected), "Progress fill must follow play time.");
            Check(Near(marker.Bounds().x + marker.Bounds().width * 0.5F,
                       track.Bounds().x + track.Bounds().width * expected),
                  "Marker must follow the fill endpoint.");
            Check(Near(fill.GetComponent<v::SpriteVisualComponent>()->UvScale().x, expected),
                  "Fill texture must be cropped with progress.");
        };
        presenter.UpdatePresentation(r::RhythmTime{-2'000'000});
        checkFraction(0);
        Check(!fill.IsVisible() && currentText.Text() == L"0:00", "Pre-roll must not show negative elapsed time.");
        presenter.UpdatePresentation(r::RhythmTime{2'000'000});
        checkFraction(.5F);
        CheckGearTransform(root);
        Check(fill.IsVisible() && currentText.Text() == L"0:02", "The current time label must be dynamic.");

        r::RhythmTimer timer;
        timer.Start(0, 1'000'000);
        timer.Pause(1'000'000);
        presenter.UpdatePresentation(timer.Now(3'000'000));
        checkFraction(.25F);
        timer.Resume(3'000'000);
        presenter.UpdatePresentation(timer.Now(4'000'000));
        checkFraction(.5F);
        for (const auto viewport : std::array<v::Size, 4>{{{1920, 1080}, {2560, 1080}, {800, 900}, {100, 720}}})
        {
            const auto width = static_cast<std::uint32_t>(viewport.width);
            const auto height = static_cast<std::uint32_t>(viewport.height);
            visuals.OnResize(width, height);
            presenter.OnResize(width, height);
            const auto bg = Node(root, "Background").Bounds();
            const auto logicalWidth = viewport.width / viewport.height * 720;
            Check(bg.x <= -logicalWidth / 2 && bg.x + bg.width >= logicalWidth / 2 && bg.y <= -360 &&
                      bg.y + bg.height >= 360,
                  "Chart backgrounds must cover every viewport, including authored offsets.");
            Check(Near(bg.x + bg.width / 2, 70) && Near(bg.y + bg.height / 2, -25),
                  "Background offsets must follow the engine's +Y-up coordinate system.");
            checkFraction(.5F);
            CheckGearTransform(root);
            Check(Near(progress.Bounds().width, std::max(viewport.width / viewport.height * 720 - 40, 1.0F)),
                  "Progress must fit the actual logical viewport width.");
            Check(marker.Bounds().x >= 0 && marker.Bounds().x + marker.Bounds().width <= progress.NodeSize().width,
                  "Marker must remain inside the bar after resize.");
        }
        presenter.UpdatePresentation(r::RhythmTime{4'000'000});
        checkFraction(1);
        presenter.UpdatePresentation(r::RhythmTime{9'000'000});
        checkFraction(1);
        Check(currentText.Text() == totalText.Text(), "Post-roll must keep the completed time and progress.");
        loaded.session->Reset();
        presenter.ApplyFeedback({.reset = true});
        presenter.UpdatePresentation(r::RhythmTime{-1});
        checkFraction(0);
        presenter.Shutdown();
        Check(visuals.FindCanvas(1) == nullptr, "Shutdown must destroy the progress Canvas.");

        // Test the view's constant allocation/resource boundary independently
        // of note/measure visual pools that legitimately change during play.
        v::Visual2DCanvas canvas;
        GameplayProgressView view;
        pattern.timing = {{{1, {}}, finger_drum::chart::TimingDirectiveType::Bpm, 240},
                          {{1, {}}, finger_drum::chart::TimingDirectiveType::DelayMilliseconds, 500}};
        loaded = finger_drum::mode::TaikoMode{}.CreateSession(pattern);
        Check(loaded.Succeeded(), "Compiled BPM/delay progress fixture must load.");
        view.Initialize(visuals, canvas.Root(), *loaded.session, texts);
        view.SetLayout({0, 0, 1200, view.Height()});
        const auto nodes = NodeCount(canvas.Root());
        const auto loads = renderer.loads;
        const auto end = loaded.session->Timeline().Compile({2, {}});
        view.Update(end / 2);
        Check(Near(Node(canvas.Root(), "Progress.Fill").GetComponent<v::SpriteVisualComponent>()->UvScale().x, .5F),
              "Progress must use compiled BPM/delay timing rather than authored beat count.");
        for (int index = 0; index < 1000; ++index)
            view.Update(r::RhythmTime{index * 5000});
        Check(nodes == NodeCount(canvas.Root()) && loads == renderer.loads,
              "Progress updates must not create nodes or reload images.");
        view.Shutdown();
        view.Update(r::RhythmTime{1});
        view.SetLayout({0, 0, 1, 1});

        const auto checkZeroDuration = [&](const bool empty, const double offset)
        {
            v::Visual2DCanvas edgeCanvas;
            finger_drum::mode::PlaySession emptySession;
            pattern.notes = {{{0, {}}, 1}};
            pattern.timing.clear();
            pattern.patternOffsetMilliseconds = offset;
            loaded = finger_drum::mode::TaikoMode{}.CreateSession(pattern);
            Check(loaded.Succeeded(), "Zero/negative note fixture must load.");
            view.Initialize(visuals, edgeCanvas.Root(), empty ? emptySession : *loaded.session, texts);
            view.SetLayout({0, 0, 1200, view.Height()});
            const auto noteTime = loaded.session->Gear().Lanes().front()->Notes().front()->Timing();
            view.Update(noteTime - r::RhythmDuration{1});
            Check(!Node(edgeCanvas.Root(), "Progress.Fill").IsVisible(), "Zero-duration progress starts empty.");
            view.Update(noteTime);
            const auto fraction =
                Node(edgeCanvas.Root(), "Progress.Fill").GetComponent<v::SpriteVisualComponent>()->UvScale().x;
            Check(std::isfinite(fraction) && Near(fraction, empty ? 0.0F : 1.0F),
                  "Empty charts stay empty; nonpositive endpoints finish at the note time without division by zero.");
            view.Shutdown();
        };
        checkZeroDuration(true, 0);
        checkZeroDuration(false, 0);
        checkZeroDuration(false, -200);
    }
    void TestSlowNotesDoNotExpandAllVisuals()
    {
        finger_drum::chart::PatternDocument pattern;
        pattern.mode = "Taiko";
        pattern.baseBpm = 180;
        for (std::int64_t i = 0; i < 5000; ++i)
            pattern.notes.push_back({{i / 16, {i % 16, 16}}, 1});
        auto loaded = finger_drum::mode::TaikoMode{}.CreateSession(pattern);
        Check(loaded.Succeeded(), "Large visibility fixture must compile.");
        loaded.session->SetNoteScrollMultiplier(5000, .001);
        AssetRenderer renderer;
        v::ScreenVisual2DManager visuals;
        visuals.Initialize(renderer, {1280, 720});
        mrg::graphics::MeshRenderSystem meshes;
        mrg::graphics::TextRenderSystem textRendering;
        mrg::audio::AudioSystem audio;
        finger_drum::texts::TextCatalog texts;
        GameplayPresenter presenter(visuals, texts);
        presenter.Initialize({meshes, textRendering, renderer, audio, 1280, 720}, *loaded.session);
        auto &root = visuals.FindCanvas(1)->Root();
        presenter.UpdatePresentation({});
        Check(NodeCount(root) < 400 && Find(root, "Lane.Note.5000"),
              "One slow future note must remain visible without creating thousands of ordinary offscreen notes.");
        const auto checkOrder = [&]
        {
            auto &first = Node(root, "Lane.Note.1");
            auto &second = Node(root, "Lane.Note.2");
            auto &notes = Node(root, "Lane.Notes");
            Check(first.Parent() == &notes && second.Parent() == &notes && first.ZIndex() > second.ZIndex(),
                  "Earlier notes and their entire long-note subtrees must render in front of later notes.");
            Check(notes.ZIndex() > Node(root, "Lane.KeyBeam").ZIndex() &&
                      notes.ZIndex() > Node(root, "Lane.JudgementCircle").ZIndex() &&
                      Node(root, "Lane.JudgementCircle").ZIndex() > Node(root, "Lane.KeyBeam").ZIndex(),
                  "Notes must render above the judgement circle, which remains above the key beam.");
        };
        checkOrder();
        Check(Node(root, "Lane.Note.1").ZIndex() > Node(root, "Lane.Note.5000").ZIndex(),
              "A very slow future note must remain behind earlier notes despite its unusual visibility.");
        loaded.session->SetNoteScrollMultiplier(5000, 1);
        presenter.UpdatePresentation({});
        Check(!Find(root, "Lane.Note.5000"), "Changing Separate must refresh the visibility projection.");
        presenter.UpdatePresentation(loaded.session->Gear().Lanes().front()->Notes().back()->Timing());
        Check(NodeCount(root) < 400 && Find(root, "Lane.Note.5000"), "Forward seek must query the end of the score.");
        presenter.UpdatePresentation({});
        Check(Find(root, "Lane.Note.1") && !Find(root, "Lane.Note.5000"),
              "Backward seek must restore only visible notes.");
        checkOrder();
        visuals.OnResize(2560, 1080);
        presenter.OnResize(2560, 1080);
        presenter.UpdatePresentation({});
        Check(NodeCount(root) < 400, "A wide viewport must retain bounded visibility work.");
        presenter.Shutdown();
    }

    void TestMeasureLinesShareNoteMotion()
    {
        AssetRenderer renderer;
        v::ScreenVisual2DManager visuals;
        visuals.Initialize(renderer, {1280, 720});
        finger_drum::chart::PatternDocument pattern;
        pattern.mode = "Taiko";
        pattern.baseBpm = 120;
        pattern.notes = {{{1, {}}, 1}};
        const auto effects = finger_drum::chart::ChartParser{}.ParseEffect(
            "[Speed]\n1,0/1,#ScrollSpeed Separate,Value=0.25\n"
            "1,0/1,Area,3,0/1,#ScrollSpeed Whole,From=1,To=2,Curve=Linear\n",
            "bar-motion.yme");
        Check(effects.Succeeded(), "Bar motion effects must parse.");
        auto loaded = finger_drum::mode::TaikoMode{}.CreateSession(pattern, effects.document);
        Check(loaded.Succeeded(), "Bar motion session must load.");
        mrg::graphics::MeshRenderSystem meshes;
        mrg::graphics::TextRenderSystem textRendering;
        mrg::audio::AudioSystem audio;
        finger_drum::texts::TextCatalog texts;
        GameplayPresenter presenter(visuals, texts);
        presenter.Initialize({meshes, textRendering, renderer, audio, 1280, 720}, *loaded.session);
        auto &root = visuals.FindCanvas(1)->Root();
        for (const auto viewport : {v::Size{1280, 720}, v::Size{1920, 1080}, v::Size{2560, 1080}})
        {
            visuals.OnResize(static_cast<std::uint32_t>(viewport.width), static_cast<std::uint32_t>(viewport.height));
            presenter.OnResize(static_cast<std::uint32_t>(viewport.width), static_cast<std::uint32_t>(viewport.height));
            for (const auto us : {0, 500'000, 1'500'000, 2'000'000})
            {
                presenter.UpdatePresentation(r::RhythmTime{us});
                const auto &note = Node(root, "Lane.Note.1");
                const auto centre = note.Position();
                const auto &lane = Node(root, "ScrollGear.Lane");
                const auto aligned =
                    std::ranges::count_if(lane.Children(),
                                          [&](const auto &child)
                                          {
                                              const auto position = child->Position();
                                              return child->Name() == "Lane.MeasureLine" && child->IsVisible() &&
                                                     Near(position.x, centre.x) && Near(position.y, centre.y);
                                          });
                Check(aligned == 1,
                      "Coincident note and bar must stay centred together under Whole/Separate and resize.");
            }
        }
        presenter.Shutdown();
    }

    void TestCountOnlyHeadsAndCounters()
    {
        for (const int type : {11, 13, 15, 16})
        {
            AssetRenderer renderer;
            v::ScreenVisual2DManager visuals;
            visuals.Initialize(renderer, {1280, 720});
            finger_drum::chart::PatternDocument pattern;
            pattern.mode = "Taiko";
            pattern.baseBpm = 120;
            pattern.patternOffsetMilliseconds = 500;
            pattern.notes = {{{0, {}}, type, 1, {}, {"HitCount=3"}}, {{0, {1, 2}}, type, 2}};
            auto loaded = finger_drum::mode::TaikoMode{}.CreateSession(pattern);
            Check(loaded.Succeeded(), "Ungraded head feedback fixture must load.");
            mrg::graphics::MeshRenderSystem meshes;
            mrg::graphics::TextRenderSystem textRendering;
            mrg::audio::AudioSystem audio;
            finger_drum::texts::TextCatalog texts;
            GameplayPresenter presenter(visuals, texts);
            presenter.Initialize({meshes, textRendering, renderer, audio, 1280, 720}, *loaded.session);
            auto &root = visuals.FindCanvas(1)->Root();
            presenter.UpdatePresentation({});
            auto &counter = Node(root, "Hud.NoteCounter.1");
            const auto &remaining = *Node(counter, "RemainingHits").GetComponent<v::TextVisualComponent>();
            Check(!counter.IsVisible(), "A hit counter must stay hidden before the first accepted hit.");
            for (int index = 0; index < 3; ++index)
            {
                const char key = type == 16 && index == 1 ? 'D' : 'F';
                presenter.ApplyFeedback({.keyPressed = true,
                                         .result = loaded.session->ProcessInput(
                                             key, r::InputEdge::Pressed, r::RhythmTime{900'000 + index * 100'000})});
                if (index < 2 || type == 11 || type == 13)
                    CheckOverlay(root, "");
                Check(!Node(root, "Judgement.Marker").IsVisible(), "Count-only hits must not create a timing marker.");
                const auto beam = Node(root, "Lane.KeyBeam").GetComponent<v::SpriteVisualComponent>()->Style().normal;
                Check(Near(beam.red, 1) && Near(beam.green, 1) && Near(beam.blue, 1),
                      "Count-only hits must use the ungraded white key beam.");
                presenter.UpdatePresentation(r::RhythmTime{900'000 + index * 100'000});
                const bool active = index < 2 || type == 11 || type == 13;
                Check(counter.IsVisible() == active, "Every active counted note must show its remaining hits.");
                if (active)
                    Check(remaining.Text() == std::to_wstring(2 - index),
                          "Each accepted hit must decrement the counter.");
            }
            if (type == 11 || type == 13)
            {
                Check(!Find(root, "Hud.NoteProcessing.1"), "Roll counters must not replace the moving roll head.");
                presenter.ApplyFeedback(
                    {.keyPressed = true,
                     .result = loaded.session->ProcessInput('F', r::InputEdge::Pressed, r::RhythmTime{1'200'000})});
                presenter.UpdatePresentation(r::RhythmTime{1'200'000});
                Check(counter.IsVisible() && remaining.Text() == L"0",
                      "Extra roll hits must not underflow the counter.");
            }
            presenter.ApplyFeedback({.result = loaded.session->Update(r::RhythmTime{1'500'000})});
            CheckOverlay(root, "OK");
            presenter.UpdatePresentation(r::RhythmTime{1'500'000});
            Check(!counter.IsVisible(), "The hit counter must disappear at completion.");
            loaded.session->Reset();
            presenter.ApplyFeedback({.reset = true});
            presenter.UpdatePresentation(r::RhythmTime{500'000});
            Check(!Node(root, "Hud.NoteCounter.1").IsVisible() &&
                      loaded.session->Gear().Lanes().front()->Notes().front()->Progress() == r::NoteProgress{0, 3},
                  "Reset must clear roll/balloon/DengDeng progress and hide its counter.");
            presenter.ApplyFeedback({.result = loaded.session->Update(r::RhythmTime{1'500'000})});
            CheckOverlay(root, "MISS");
            presenter.Shutdown();
        }
    }

    void TestDengDengWrongOrderFeedback()
    {
        AssetRenderer renderer;
        v::ScreenVisual2DManager visuals;
        visuals.Initialize(renderer, {1280, 720});
        finger_drum::chart::PatternDocument pattern;
        pattern.mode = "Taiko";
        pattern.baseBpm = 120;
        pattern.notes = {{{0, {}}, 16, 1, {}, {"HitCount=3"}}, {{0, {1, 2}}, 16, 2}};
        auto loaded = finger_drum::mode::TaikoMode{}.CreateSession(pattern);
        Check(loaded.Succeeded(), "Wrong-order presentation fixture must load.");
        mrg::graphics::MeshRenderSystem meshes;
        mrg::graphics::TextRenderSystem textRendering;
        mrg::audio::AudioSystem audio;
        finger_drum::texts::TextCatalog texts;
        GameplayPresenter presenter(visuals, texts);
        presenter.Initialize({meshes, textRendering, renderer, audio, 1280, 720}, *loaded.session);
        auto &root = visuals.FindCanvas(1)->Root();
        for (const char first : {'F', 'D'})
        {
            loaded.session->Reset();
            presenter.ApplyFeedback({.reset = true});
            const char other = first == 'F' ? 'D' : 'F';
            const std::array<char, 5> keys{first, first, other, other, first};
            const std::array<std::size_t, 5> counts{1, 1, 2, 2, 3};
            for (std::size_t index = 0; index < keys.size(); ++index)
            {
                presenter.ApplyFeedback(
                    {.keyPressed = true,
                     .result = loaded.session->ProcessInput(keys[index], r::InputEdge::Pressed,
                                                            r::RhythmTime{10'000 * static_cast<std::int64_t>(index)})});
                CheckOverlay(root, index == 4 ? "OK" : "");
                Check(!Node(root, "Judgement.Marker").IsVisible(),
                      "Wrong-order presses must not display a timing marker.");
                const auto beam = Node(root, "Lane.KeyBeam").GetComponent<v::SpriteVisualComponent>()->Style().normal;
                Check(Near(beam.red, 1) && Near(beam.green, 1) && Near(beam.blue, 1),
                      "Wrong-order DengDeng input must remain white, never POOR/red.");
                Check(loaded.session->Gear().Lanes().front()->Notes().front()->Progress() ==
                          r::NoteProgress{counts[index], 3},
                      "The displayed count must stay unchanged on repeated actions.");
            }
        }
        presenter.Shutdown();
    }

    void TestPresenterEventPriority()
    {
        AssetRenderer renderer;
        v::ScreenVisual2DManager visuals;
        visuals.Initialize(renderer, {1280.0F, 720.0F});
        finger_drum::chart::PatternDocument pattern;
        pattern.mode = "Taiko";
        pattern.baseBpm = 180;
        finger_drum::chart::PatternNote note;
        note.keyType = 1;
        pattern.notes.push_back(note);
        auto loaded = finger_drum::mode::TaikoMode{}.CreateSession(pattern);
        Check(loaded.Succeeded(), "Presenter fixture must build.");
        mrg::graphics::MeshRenderSystem meshes;
        mrg::graphics::TextRenderSystem textRendering;
        mrg::audio::AudioSystem audio;
        finger_drum::texts::TextCatalog texts;
        GameplayPresenter presenter(visuals, texts);
        presenter.Initialize({meshes, textRendering, renderer, audio, 1280, 720}, *loaded.session);
        auto &root = visuals.FindCanvas(1)->Root();
        presenter.PresentErrorMessage("quality-regression.yme:4 [HitSound] invalid value");
        auto &errorLabel = Node(root, "Hud.Error");
        Check(errorLabel.IsVisible(), "An automation diagnostic must be visible in the gameplay HUD.");
        presenter.PresentErrorMessage({});
        Check(!errorLabel.IsVisible(), "Reset/cleared errors must hide the previous diagnostic.");
        r::NoteProcessResult rollover;
        rollover.events.push_back(Event(r::NoteEventType::Missed, r::JudgementGrade::Miss));
        rollover.events.push_back(Event(r::NoteEventType::HitAccepted, r::JudgementGrade::Max));
        rollover.events.push_back(Event(r::NoteEventType::Completed, r::JudgementGrade::Max));
        presenter.ApplyFeedback({.keyPressed = true, .result = rollover});
        CheckOverlay(root, "MAX");
        presenter.ApplyFeedback({.reset = true});
        CheckOverlay(root, "");
        r::NoteProcessResult empty;
        empty.events.push_back(Event(r::NoteEventType::InputRejected, r::JudgementGrade::Miss));
        empty.events.push_back(Event(r::NoteEventType::Missed, r::JudgementGrade::Miss));
        presenter.ApplyFeedback({.keyPressed = true, .result = empty});
        CheckOverlay(root, "");
        const auto beam = Node(root, "Lane.KeyBeam").GetComponent<v::SpriteVisualComponent>()->Style().normal;
        Check(Near(beam.red, 1.0F) && Near(beam.green, 1.0F) && Near(beam.blue, 1.0F),
              "Out-of-range input must only produce the white key beam.");
        r::NoteProcessResult wrong;
        wrong.events.push_back(Event(r::NoteEventType::InputRejected, r::JudgementGrade::Perfect));
        presenter.ApplyFeedback({.keyPressed = false, .result = wrong});
        CheckOverlay(root, "");
        presenter.ApplyFeedback({.keyPressed = true, .result = wrong});
        CheckOverlay(root, "POOR");
        presenter.Shutdown();
    }
    void TestRollCompletionAndProcessedTicks()
    {
        AssetRenderer renderer;
        v::ScreenVisual2DManager visuals;
        visuals.Initialize(renderer, {1280, 720});
        finger_drum::chart::PatternDocument pattern;
        pattern.mode = "Taiko";
        pattern.baseBpm = 120;
        pattern.notes = {{{0, {}}, 12, 1, {}, {"TickDivision=16"}}, {{0, {1, 4}}, 12, 2}};
        auto loaded = finger_drum::mode::TaikoMode{}.CreateSession(pattern);
        Check(loaded.Succeeded(), "Tick presentation fixture must load.");
        mrg::graphics::MeshRenderSystem meshes;
        mrg::graphics::TextRenderSystem textRendering;
        mrg::audio::AudioSystem audio;
        finger_drum::texts::TextCatalog texts;
        GameplayPresenter presenter(visuals, texts);
        presenter.Initialize({meshes, textRendering, renderer, audio, 1280, 720}, *loaded.session);
        auto &root = visuals.FindCanvas(1)->Root();
        const auto visibleTicks = [&]
        {
            std::size_t count = 0;
            const auto visit = [&](const auto &self, const v::Visual2DNode &node) -> void
            {
                if (node.Name() == "LongNote.Tick" && node.IsVisible())
                    ++count;
                for (const auto &child : node.Children())
                    self(self, *child);
            };
            visit(visit, root);
            return count;
        };
        presenter.UpdatePresentation(r::RhythmTime{});
        Check(visibleTicks() == 4, "All body ticks, including the aligned tail, must start visible.");
        presenter.ApplyFeedback(
            {.keyPressed = true, .result = loaded.session->ProcessInput('F', r::InputEdge::Pressed, r::RhythmTime{})});
        presenter.ApplyFeedback(
            {.keyPressed = true,
             .result = loaded.session->ProcessInput('F', r::InputEdge::Pressed, r::RhythmTime{75'000})});
        presenter.UpdatePresentation(r::RhythmTime{75'000});
        Check(visibleTicks() == 3, "An early accepted tick must disappear before its scheduled time.");
        presenter.ApplyFeedback(
            {.keyPressed = true,
             .result = loaded.session->ProcessInput('F', r::InputEdge::Pressed, r::RhythmTime{250'000})});
        presenter.ApplyFeedback(
            {.keyPressed = true,
             .result = loaded.session->ProcessInput('F', r::InputEdge::Pressed, r::RhythmTime{375'000})});
        presenter.UpdatePresentation(r::RhythmTime{375'000});
        Check(visibleTicks() == 1, "The unprocessed tail tick must remain visible after the other ticks.");
        presenter.ApplyFeedback(
            {.keyPressed = true,
             .result = loaded.session->ProcessInput('F', r::InputEdge::Pressed, r::RhythmTime{500'000})});
        presenter.UpdatePresentation(r::RhythmTime{500'000});
        Check(visibleTicks() == 0, "An accepted tail tick must disappear immediately.");
        presenter.ApplyFeedback({.result = loaded.session->Update(r::RhythmTime{500'000})});
        CheckOverlay(root, "OK");
        presenter.AdvanceEffects(.2);
        CheckOverlay(root, "");
        loaded.session->Reset();
        presenter.ApplyFeedback({.reset = true});
        presenter.UpdatePresentation(r::RhythmTime{});
        Check(visibleTicks() == 4, "Reset must restore consumed tick sprites, including the tail.");
        presenter.ApplyFeedback({.result = loaded.session->Update(r::RhythmTime{500'000})});
        CheckOverlay(root, "MISS");
        presenter.UpdatePresentation(r::RhythmTime{500'000});
        Check(visibleTicks() == 0, "Expired/missed ticks must also be hidden.");
        presenter.Shutdown();
        pattern.notes.back().position = {0, {1, 1}};
        pattern.notes.front().extraData = {"TickDivision=8"};
        pattern.timing.push_back({{0, {1, 2}}, finger_drum::chart::TimingDirectiveType::DelayMilliseconds, -750});
        loaded = finger_drum::mode::TaikoMode{}.CreateSession(pattern);
        Check(loaded.Succeeded(), "Negative-delay tick fixture must load.");
        presenter.Initialize({meshes, textRendering, renderer, audio, 1280, 720}, *loaded.session);
        auto &delayedRoot = visuals.FindCanvas(2)->Root();
        const auto visibleDelayedTicks = [&]
        {
            return std::ranges::count_if(Node(delayedRoot, "Lane.Note.1").Children(), [](const auto &child)
                                         { return child->Name() == "LongNote.Tick" && child->IsVisible(); });
        };
        const auto minimumTickY = [&]
        {
            float minimum = std::numeric_limits<float>::infinity();
            for (const auto &child : Node(delayedRoot, "Lane.Note.1").Children())
                if (child->Name() == "LongNote.Tick" && child->IsVisible())
                    minimum = std::min(minimum, child->Bounds().y);
            return minimum;
        };
        presenter.ApplyFeedback(
            {.keyPressed = true, .result = loaded.session->ProcessInput('F', r::InputEdge::Pressed, r::RhythmTime{})});
        presenter.UpdatePresentation(r::RhythmTime{200'000});
        const auto firstY = minimumTickY();
        Check(visibleDelayedTicks() == 5,
              "Overwritten ticks must not be presented or judged; the active tail remains.");
        presenter.ApplyFeedback(
            {.keyPressed = true,
             .result = loaded.session->ProcessInput('F', r::InputEdge::Pressed, r::RhythmTime{200'000})});
        presenter.UpdatePresentation(r::RhythmTime{200'000});
        Check(std::isfinite(firstY) && minimumTickY() > firstY && visibleDelayedTicks() == 4,
              "Surviving processed ticks must disappear after overwritten ticks have been excluded.");
        presenter.Shutdown();
    }
} // namespace

int main()
{
    try
    {
        std::string error;
        Check(finger_drum::assets::InitializeBuiltInAssets(error), error.c_str());
        TestGuideAndOverlays();
        TestGameplayProgress();
        TestSlowNotesDoNotExpandAllVisuals();
        TestMeasureLinesShareNoteMotion();
        TestCountOnlyHeadsAndCounters();
        TestDengDengWrongOrderFeedback();
        TestPresenterEventPriority();
        TestRollCompletionAndProcessedTicks();
        std::puts("Gameplay progress and judgement guide/marker/overlay tests passed.");
        return 0;
    }
    catch (const std::exception &error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
    catch (...)
    {
        std::fputs("Unknown judgement test failure.\n", stderr);
        return 1;
    }
}
