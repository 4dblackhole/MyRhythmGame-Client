#include "App/AssetPaths.h"
#include "GameScene/MusicSelectScene/Submodules/MusicSelectView.h"
#include "GameScene/MusicSelectScene/Submodules/SongCatalogLoader.h"
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
        std::cout << "Music selection viewport and catalog worker tests passed.\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
