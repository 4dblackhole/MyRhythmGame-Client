#include "App/AssetPaths.h"
#include "App/SkinManifest.h"
#include "GameScene/FingerDrumLogoScene/Submodules/LogoView.h"
#include "GameScene/MusicSelectScene/Submodules/MusicSelectView.h"
#include "Presentation/SkinVisualComponent.h"
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace
{
    namespace fs = std::filesystem;
    namespace v = mrg::visual2d;
    void Check(bool value, const char *message)
    {
        if (!value)
            throw std::runtime_error(message);
    }
    void Write(const fs::path &path, std::string_view bytes)
    {
        fs::create_directories(path.parent_path());
        std::ofstream out(path, std::ios::binary);
        out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        Check(out.good(), "Could not write an isolated skin fixture.");
    }
    void Copy(const fs::path &source, const fs::path &target)
    {
        fs::create_directories(target.parent_path());
        fs::copy_file(source, target, fs::copy_options::overwrite_existing);
    }
    void ManifestChecks(const fs::path &root, const fs::path &defaults)
    {
        const auto skin = root / "manifest";
        Write(skin / L"그림 폴더/아이콘 #1.png", "isolated fixture");
        Write(skin / "skin.ini", "\xEF\xBB\xBF[Folders]\r\nLogo=그림 폴더\r\n[Logo]\r\n"
                                 "LogoCenter=\"아이콘 #1.png\"\r\nLogoLeft=missing.png\r\n"
                                 "LogoRight=../escape.png\r\nBackground=\r\n");
        const auto images = mrg_client::LoadSkinImages(skin, defaults);
        Check(images.paths.at("Logo.LogoCenter") == skin / L"그림 폴더/아이콘 #1.png",
              "Both directory and filename must be relative, with UTF-8/BOM/CRLF support.");
        Check(images.paths.at("Logo.LogoLeft") == defaults / "Logo/LeftFade.png",
              "A missing custom filename must fall back by component, not by custom filename.");
        Check(images.paths.at("Logo.LogoRight") == defaults / "Logo/RightFade.png",
              "Parent traversal must fall back without affecting other components.");
        Check(images.paths.at("InGame.note") == defaults / "InGame/note.png" &&
                  images.paths.at("Logo.Background").empty() && images.diagnostics.size() == 2,
              "Unspecified scenes and procedural defaults must remain intact.");
        Write(skin / "skin.ini", "[Folders]\nLogo=C:\\absolute\n[Logo]\nLogoCenter=/rooted.png\n"
                                 "LogoLeft=../escape.png\nLogoRight=\\\\server\\share.png\n"
                                 "[Broken\nLogoCenter=wrong.png\n[Logo]\nLogoCenter=\"\n");
        const auto invalid = mrg_client::LoadSkinImages(skin, defaults);
        Check(invalid.paths.at("Logo.LogoCenter") == defaults / "Logo/Center.png" && invalid.diagnostics.size() >= 6,
              "Invalid paths and sections must not escape into scene loading.");
        Write(skin / "skin.ini", "[Logo]\nLogoCenter=missing.png\nLogoCenter=other.png\n");
        Check(mrg_client::LoadSkinImages(skin, defaults).diagnostics.size() == 2,
              "Duplicate keys and missing files must be diagnosed.");
        Write(skin / "skin.ini", "[Logo]\nLogoCenter=\xFF\n");
        Check(mrg_client::LoadSkinImages(skin, defaults).paths.at("Logo.LogoCenter") == defaults / "Logo/Center.png",
              "Invalid UTF-8 must leave the embedded defaults usable.");
        Write(root / "partial/InGame/note.png", "isolated fixture");
        Check(mrg_client::LoadSkinImages(root / "partial", defaults).paths.at("InGame.note") ==
                  root / "partial/InGame/note.png",
              "Partial skins may inherit the default manifest mappings.");
    }
    class Renderer final : public mrg::graphics::Visual2DRenderSystem
    {
      public:
        v::ImageHandle LoadImage(const fs::path &path) override
        {
            loaded.push_back(path);
            return {loaded.size()};
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
        std::vector<fs::path> loaded;
    };
    v::Visual2DNode &Find(v::Visual2DNode &root, std::string_view name)
    {
        const auto search = [&](auto &&self, v::Visual2DNode &node) -> v::Visual2DNode *
        {
            if (node.Name() == name)
                return &node;
            for (const auto &child : node.Children())
                if (auto *found = self(self, *child))
                    return found;
            return nullptr;
        };
        auto *node = search(search, root);
        if (!node)
            throw std::runtime_error("Skin view node is missing: " + std::string(name));
        return *node;
    }
    v::ImageHandle Image(v::Visual2DNode &node)
    {
        return node.GetComponent<v::SpriteVisualComponent>()->Style().normalImage;
    }
    void ViewChecks(const fs::path &repo, const fs::path &skin, const fs::path &defaults)
    {
        Copy(defaults / "Result/Background/Sky.png", skin / "Title/sky.png");
        Copy(defaults / "Logo/Center.png", skin / "Title/center.png");
        Copy(defaults / "InGame/Progress/Track.png", skin / "Title/button.png");
        Copy(defaults / "InGame/Progress/Fill.png", skin / "Title/hover.png");
        Copy(defaults / "Result/Background/Sky.png", skin / "Select/background.png");
        Copy(defaults / "InGame/Progress/Track.png", skin / "Select/panel.png");
        Copy(defaults / "InGame/note.png", skin / "Play/custom.png");
        Write(skin / "skin.ini", "[Folders]\nLogo=Title\nMusicSelect=Select\nInGame=Play\n"
                                 "[Logo]\nBackground=sky.png\nLogoCenter=center.png\nStartButton=button.png\n"
                                 "StartButton.Hovered=hover.png\n[MusicSelect]\nBackground=background.png\n"
                                 "RecordPanel=panel.png\nBack=panel.png\nSongCardFocused=panel.png\n"
                                 "DifficultySelected=panel.png\n[InGame]\nnote=custom.png\n");
        auto &selection = mrg_client::SkinSetSelection::Instance();
        selection.Initialize(L"Test Skin");
        Check(selection.CurrentName() == L"Test Skin", "The isolated custom skin must be selected.");
        Check(mrg_client::asset_paths::skin::InGame("note") == skin / "Play/custom.png",
              "Gameplay and editor must use the manifest's shared image mapping.");
        Renderer renderer;
        v::ScreenVisual2DManager visuals;
        visuals.Initialize(renderer, {1280, 720});
        mrg::graphics::MeshRenderSystem meshes;
        mrg::graphics::TextRenderSystem textRendering;
        mrg::audio::AudioSystem audio;
        finger_drum::texts::TextCatalog texts;
        mrg_client::OptionSettings options(skin / "unused.ini");
        const mrg::EngineServices services{meshes, textRendering, renderer, audio, 1280, 720};
        LogoView logo(visuals, texts, options);
        logo.Initialize(services);
        logo.BeginScene();
        auto &logoRoot = visuals.FindCanvas(1)->Root();
        auto &start = Find(logoRoot, "FingerDrum.GameStart");
        const auto &style = start.GetComponent<v::SpriteVisualComponent>()->Style();
        Check(style.normalImage && style.hoveredImage && style.normalImage.value != style.hoveredImage.value &&
                  style.pressedImage.value == style.normalImage.value,
              "Buttons must use authored states and normal-image fallback.");
        Check(Find(logoRoot, "FingerDrum.LogoBackground").IsVisible() &&
                  !Find(logoRoot, "FingerDrum.LogoStrip").IsVisible(),
              "A full-screen logo background must replace the strip rather than be covered by it.");
        const auto &backgroundStyle = Find(logoRoot, "FingerDrum.LogoBackground")
                                          .GetComponent<v::SpriteVisualComponent>()->Style();
        Check(backgroundStyle.normal.red == 1 && backgroundStyle.normal.green == 1 && backgroundStyle.normal.blue == 1,
              "A custom logo background must retain its authored RGB.");
        SongSelectionState songs;
        songs.catalog_ = finger_drum::chart::SongCatalog{}.Load(repo / "FingerDrum.Assets/Assets/Songs");
        Check(!songs.catalog_.songs.empty(), "The skin integration test needs the real bundled song catalog.");
        MusicSelectView music(visuals, songs, SongSelectPurpose::Gameplay, texts, options);
        music.Initialize(services);
        music.SetVisible(true);
        auto &musicRoot = visuals.FindCanvas(2)->Root();
        auto &record = Find(musicRoot, "RecordPanel");
        auto &back = Find(musicRoot, "Back");
        Check(Image(record) && !Find(record, "Surface").IsVisible() && Image(back) &&
                  !Find(back, "Back.Fill").IsVisible(),
              "Procedural fills must not obscure panel/button images.");
        auto &card = Find(musicRoot, "Song.0");
        Check(Image(card) && !Find(card, "Surface").IsVisible(), "Virtualized song cards must accept skin images.");
        const auto loads = renderer.loaded.size();
        for (int i = 0; i < 100; ++i)
            visuals.Update(.016);
        Check(renderer.loaded.size() == loads, "Steady frames must reuse registered images.");
        // Editing a manifest does not perform I/O during a frame. Re-selecting
        // another skin and returning explicitly invalidates the snapshot.
        Write(skin / "skin.ini", "[Folders]\nInGame=Play\n[InGame]\nnote=missing.png\n");
        Check(selection.Image("InGame", "note") == skin / "Play/custom.png", "The manifest must be cached.");
        std::string error;
        Check(selection.Select(L"Default Skin", error), error.c_str());
        visuals.Update(0);
        static_cast<void>(logo.Update(mrg::platform::InputState{}, 0));
        Check(!Image(start) && !Image(record) && Find(record, "Surface").IsVisible() &&
                  Find(back, "Back.Fill").IsVisible() && Find(logoRoot, "FingerDrum.LogoStrip").IsVisible(),
              "Returning to the default skin must restore all procedural designs and logo panels.");
        Check(selection.Select(L"Test Skin", error), error.c_str());
        Check(selection.Image("InGame", "note") == defaults / "InGame/note.png",
              "Re-selecting must reload edited configuration and use per-element fallback.");
        music.Shutdown();
        logo.Shutdown();
        Check(visuals.CanvasCount() == 0, "View shutdown must destroy owned skin bindings.");
    }
} // namespace
int wmain(int argc, wchar_t **argv)
{
    try
    {
        Check(argc == 2, "Expected repository root.");
        std::string error;
        Check(finger_drum::assets::InitializeBuiltInAssets(error), error.c_str());
        const auto root = finger_drum::assets::UserAssetRoot();
        Check(root == finger_drum::assets::ExecutableDirectory() / "Asset",
              "Skin fixtures must run in an isolated deployment directory.");
        const auto defaults = finger_drum::assets::BuiltInAssetRoot() / "assets/skins/Default Skin";
        ManifestChecks(root / "fixtures", defaults);
        ViewChecks(fs::path(argv[1]), root / "Skins/Test Skin", defaults);
        std::cout
            << "Skin manifest paths, fallback, cache, logo, button states and song-selection integration passed.\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
