#include "App/AssetPaths.h"
#include "GameScene/MusicSelectScene/Submodules/MusicSelectView.h"
#include "GameScene/MusicSelectScene/Submodules/SongCatalogLoader.h"
#include "Presentation/MarqueeTextComponent.h"
#include <chrono>
#include <iostream>
#include <thread>

namespace
{
    namespace v = mrg::visual2d;
    void Check(bool value, const char *message)
    {
        if (!value)
            throw std::runtime_error(message);
    }
    class Renderer final : public mrg::graphics::Visual2DRenderSystem
    {
      public:
        v::ImageHandle LoadImage(const std::filesystem::path &) override
        {
            return {++id_};
        }
        v::Size GetImageSize(v::ImageHandle) const noexcept override
        {
            return {96, 96};
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
        std::uint64_t id_{};
    };
    std::size_t Cards(const v::Visual2DNode &node)
    {
        std::size_t count = node.Name().starts_with("Song.") ? 1 : 0;
        for (const auto &child : node.Children())
            count += Cards(*child);
        return count;
    }
    bool HasCard(const v::Visual2DNode &node, std::size_t index)
    {
        if (node.Name() == "Song." + std::to_string(index))
            return true;
        for (const auto &child : node.Children())
            if (HasCard(*child, index))
                return true;
        return false;
    }
    v::Visual2DNode *Find(v::Visual2DNode &node, std::string_view name)
    {
        if (node.Name() == name)
            return &node;
        for (const auto &child : node.Children())
            if (auto *found = Find(*child, name))
                return found;
        return nullptr;
    }
    finger_drum::chart::SongCatalogLoadResult AwaitCatalog(SongCatalogLoader &loader)
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{10};
        do
        {
            if (auto result = loader.Poll())
                return std::move(*result);
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
        } while (std::chrono::steady_clock::now() < deadline);
        throw std::runtime_error("Catalog worker timed out.");
    }
} // namespace

int main()
{
    try
    {
        {
            v::Visual2DCanvas canvas;
            auto &label = v::CreateLabel(canvas.Root(), {0, 0, 100, 30}, L"");
            const std::wstring unicode = L"\U0001F600ABCDEFGH";
            auto &marquee = label.AddComponent<finger_drum::presentation::MarqueeTextComponent>(unicode, 0.28);
            marquee.Update(0);
            const auto initial = canvas.BuildDrawList();
            const auto source = initial.back().text;
            source->measuredSize = v::Size{180, 20};
            marquee.Update(1.3);
            const auto moved = canvas.BuildDrawList();
            if (moved.size() != 2 || moved[0].text != source || moved[1].text != source || moved[0].textOffset.x >= 0 ||
                moved[1].textOffset.x <= 0 || source->value != unicode)
                throw std::runtime_error("Marquee must preserve its Unicode content and share its repeated layout.");
            if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, source->value.data(),
                                    static_cast<int>(source->value.size()), nullptr, 0, nullptr, nullptr) <= 0)
                throw std::runtime_error("Marquee scrolling corrupted UTF-16.");
            label.SetSize({300, 30});
            marquee.Update(0);
            const auto resized = canvas.BuildDrawList();
            if (resized.size() != 1 || resized[0].textOffset.x != 0 || !label.ClipRect())
                throw std::runtime_error("A resized label must stop scrolling when the full title fits.");
            marquee.SetText(L"Short");
            marquee.Update(0);
            if (canvas.BuildDrawList().back().text->value != L"Short")
                throw std::runtime_error("Marquee content replacement failed.");
        }
        std::string error;
        Check(finger_drum::assets::InitializeBuiltInAssets(error), error.c_str());
        SongCatalogLoader loader;
        loader.Refresh(mrg_client::asset_paths::UserSongs());
        const auto first = AwaitCatalog(loader);
        Check(!first.songs.empty(), "Worker must publish the real catalog.");
        loader.Refresh(mrg_client::asset_paths::UserSongs());
        const auto repeated = AwaitCatalog(loader);
        Check(first.songs.size() == repeated.songs.size() && first.PatternCount() == repeated.PatternCount(),
              "A retained parser cache must preserve catalog results.");
        loader.Refresh(mrg_client::asset_paths::UserSongs() / "nonexistent-regression-directory");
        const auto failed = AwaitCatalog(loader);
        Check(failed.songs.empty() && !failed.diagnostics.empty(),
              "Worker I/O errors must be published as diagnostics.");

        Renderer renderer;
        v::ScreenVisual2DManager visuals;
        visuals.Initialize(renderer, {1280, 720});
        mrg::graphics::MeshRenderSystem meshes;
        mrg::graphics::TextRenderSystem textRendering;
        mrg::audio::AudioSystem audio;
        finger_drum::texts::TextCatalog texts;
        mrg_client::OptionSettings options("build/performance-view-test-unused.ini");
        SongSelectionState selection;
        // Synthetic data stays in this test process; no song or settings file is written.
        for (int i = 0; i < 5000; ++i)
        {
            finger_drum::chart::SongCatalogEntry song;
            song.music.names.push_back("Test song " + std::to_string(i));
            song.music.artists.push_back("Test artist");
            song.patterns.resize(2);
            selection.catalog_.songs.push_back(std::move(song));
        }
        MusicSelectView view(visuals, selection, SongSelectPurpose::Gameplay, texts, options);
        view.Initialize({meshes, textRendering, renderer, audio, 1280, 720});
        auto *canvas = visuals.FindCanvas(1);
        Check(canvas && !Find(canvas->Root(), "AddDifficulty.0"), "Gameplay cards must not offer chart creation.");
        Check(canvas != nullptr && Cards(canvas->Root()) > 0 && Cards(canvas->Root()) < 20,
              "A 5,000-song library must create only viewport cards.");
        for (const auto target : {2500U, 4999U, 0U})
        {
            selection.SelectVisibleSong(target);
            view.RebuildVisibleSongs();
            Check(Cards(canvas->Root()) < 20 && HasCard(canvas->Root(), target),
                  "Focus changes must preserve the original row index and keep the active tree bounded.");
        }
        visuals.OnResize(900, 720);
        view.OnResize(900, 720);
        Check(Cards(canvas->Root()) < 20 && HasCard(canvas->Root(), 0), "Resize must retain the focused visible card.");
        selection.searchText_ = L"no matching song";
        view.RebuildVisibleSongs();
        Check(Cards(canvas->Root()) == 0, "Empty searches must remove the former cards.");
        view.Shutdown();
        Check(visuals.CanvasCount() == 0, "Virtualized view must release its Canvas.");
        selection.searchText_.clear();
        selection.catalog_.songs[0].patterns.clear();
        MusicSelectView editor(visuals, selection, SongSelectPurpose::Editor, texts, options);
        editor.Initialize({meshes, textRendering, renderer, audio, 1280, 720});
        canvas = visuals.FindCanvas(2);
        if (!canvas)
            throw std::runtime_error("Editor song-select Canvas missing.");
        auto *add = Find(canvas->Root(), "AddDifficulty.0");
        if (!add)
            throw std::runtime_error("Songs without a chart must offer the plus button.");
        Check(add->IsEnabled(), "Chart creation must be enabled after catalog loading.");
        const auto bounds = add->BoundsInCanvas();
        v::Visual2DInputRouter router;
        v::PointerInput pointer{};
        pointer.available = true;
        pointer.position = {bounds.x + bounds.width * .5F, bounds.y + bounds.height * .5F};
        pointer.leftButtonDown = pointer.leftButtonPressed = true;
        router.Process(*canvas, pointer);
        pointer.leftButtonDown = pointer.leftButtonPressed = false;
        pointer.leftButtonReleased = true;
        router.Process(*canvas, pointer);
        mrg::platform::InputState input;
        Check(editor.Update(input, 0) == SongSelectCommand::CreateDifficulty && selection.focusedSongPosition_ == 0,
              "Plus must select its song and emit chart creation rather than launch.");
        editor.Shutdown();
        std::cout << "Music selection viewport and catalog worker tests passed.\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
