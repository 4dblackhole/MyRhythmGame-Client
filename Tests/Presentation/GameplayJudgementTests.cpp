#include "GameScene/RhythmTestScene/Submodules/GameplayJudgementView.h"
#include "GameScene/RhythmTestScene/Submodules/GameplayPresenter.h"
#include "Presentation/LaneKeyBeam.h"
#include "Taiko/TaikoMode.h"
#include "FingerDrumAssets.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <fstream>
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
            const auto dimension = [&header](const std::size_t offset) {
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
        void SubmitScreen(const v::Visual2DCanvas &, const mrg::graphics::RenderContext &,
                          v::Point, std::uint32_t) override {}
        void SubmitPlane(const v::Visual2DCanvas &, const mrg::graphics::RenderContext &,
                         const DirectX::XMFLOAT4X4 &, v::Size, const DirectX::XMFLOAT4X4 &) override {}
        mrg::graphics::RenderTargetTextureHandle CreateCanvasRenderTarget(std::uint32_t, std::uint32_t) override
        { return {}; }
        void RenderToTexture(const v::Visual2DCanvas &, const mrg::graphics::RenderTargetTextureHandle &,
                             const mrg::graphics::RenderContext &) override {}
      private:
        std::map<std::uint64_t, v::Size> sizes_;
    };
    r::NoteEvent Event(const r::NoteEventType type, const r::JudgementGrade grade,
                       const r::RhythmDuration error = {})
    {
        r::NoteEvent event;
        event.type = type;
        event.judgement = {grade, error, 1.0};
        return event;
    }
    void CheckOverlay(v::Visual2DNode &root, const std::string_view word)
    {
        constexpr std::array names{"MAX", "PERFECT", "GREAT", "GOOD", "BAD", "MISS", "POOR"};
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
            const auto beamColor = finger_drum::presentation::LaneKeyBeam::GradeColor(static_cast<r::JudgementGrade>(index));
            Check(Near(color.red, beamColor.red) && Near(color.green, beamColor.green) && Near(color.blue, beamColor.blue),
                  "Each independent band must match the key-beam grade color.");
        }
        const std::array words{"MAX", "PERFECT", "GREAT", "GOOD", "BAD"};
        for (std::size_t index = 0; index < words.size(); ++index)
        {
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
        view.Present(Event(r::NoteEventType::HitAccepted, r::JudgementGrade::Good, r::RhythmDuration{54'500}));
        Check(marker.Bounds().x > 0.0F, "Late input must place marker to the right.");
        view.Update(0.1);
        Check(Near(Node(root, "Lane.Judgement.GOOD").GetComponent<v::SpriteVisualComponent>()->Style().normal.alpha, 0.5F),
              "Judgement overlay must fade over 200ms.");
        view.Update(0.1);
        CheckOverlay(root, "");
        Check(!marker.IsVisible(), "Marker expires after 200ms.");

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
        view.Present(Event(r::NoteEventType::InputRejected, r::JudgementGrade::Bad, r::RhythmDuration{500'000}));
        Check(Near(marker.Bounds().x, margin.NodeSize().width * 0.5F), "Marker must be clamped to the visible guide.");
        view.OnResize(200.0F);
        Check(Near(margin.NodeSize().width, 160.0F), "Wide ranges must fit a narrow viewport.");
        Check(Near(marker.Bounds().x, 80.0F), "Resize must preserve the timing position.");
        view.OnResize(1280.0F);
        Check(Near(bad.NodeSize().width, 400.0F), "Resize restores the fixed time scale.");
        Check(NodeCount(root) == count && renderer.loads == loads, "Repeated hits/resize must reuse nodes and images.");
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
    void TestPresenterEventPriority()
    {
        AssetRenderer renderer;
        v::ScreenVisual2DManager visuals;
        visuals.Initialize(renderer, {1280.0F, 720.0F});
        finger_drum::chart::PatternDocument pattern;
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
}

int main()
{
    try
    {
        std::string error;
        Check(finger_drum::assets::InitializeBuiltInAssets(error), error.c_str());
        TestGuideAndOverlays();
        TestPresenterEventPriority();
        std::puts("Gameplay judgement guide/marker/overlay tests passed.");
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
