#include "App/AssetPaths.h"
#include "GameScene/MusicSelectScene/Submodules/EditorOsuImport.h"
#include "GameScene/MusicSelectScene/Submodules/MusicSelectView.h"
#include "GameScene/MusicSelectScene/Submodules/SongCatalogLoader.h"
#include "GameFlow/PlaySessionLoader.h"
#include "Judgement/AccuracyRange.h"
#include "Presentation/MarqueeTextComponent.h"
#include <chrono>
#include <cmath>
#include <map>
#include <fstream>
#include <iostream>
#include <memory>
#include <thread>
#include <atomic>
#include "Presentation/PlayOptionsDialog.h"

struct InputStateTestAccess
{
    static void Key(InputState &input, std::uint16_t key, bool down) { input.SetKey(key, down, 1); }
    static void Begin(InputState &input) { input.BeginUpdate(); }
};
namespace
{

    void Check(bool value, const char *message)
    {
        if (!value)
            throw std::runtime_error(message);
    }
    class Renderer final : public Visual2DRenderSystem
    {
      public:
        std::vector<std::filesystem::path> loaded;
        ImageHandle LoadImage(const std::filesystem::path &path) override
        {
            if (path.filename() == "preview-cpu-fail.jpg" || path.filename() == "preview-gpu-fail.jpg")
            {
                loaded.push_back(path);
                throw std::runtime_error("Cannot load image");
            }
            const auto id = ++id_;
            if (path.filename().wstring().starts_with(L"preview-"))
            {
                loaded.push_back(path);
                imageSizes_[id] = path.filename() == "preview-portrait.jpg" ? Size{900, 1600} : Size{1600, 900};
            }
            return {id};
        }
        Size GetImageSize(ImageHandle image) const noexcept override
        {
            const auto found = imageSizes_.find(image.value);
            return found == imageSizes_.end() ? Size{96, 96} : found->second;
        }
        void SubmitScreen(const Visual2DCanvas &, const RenderContext &, Point,
                          std::uint32_t) override
        {
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

      private:
        std::uint64_t id_{};
        std::map<std::uint64_t, Size> imageSizes_;
    };
    std::size_t Cards(const Visual2DNode &node)
    {
        std::size_t count = node.Name().starts_with("Song.") ? 1 : 0;
        for (const auto &child : node.Children())
            count += Cards(*child);
        return count;
    }
    bool HasCard(const Visual2DNode &node, std::size_t index)
    {
        if (node.Name() == "Song." + std::to_string(index))
            return true;
        for (const auto &child : node.Children())
            if (HasCard(*child, index))
                return true;
        return false;
    }
    Visual2DNode *Find(Visual2DNode &node, std::string_view name)
    {
        if (node.Name() == name)
            return &node;
        for (const auto &child : node.Children())
            if (auto *found = Find(*child, name))
                return found;
        return nullptr;
    }
    void Click(Visual2DCanvas &canvas, const Visual2DNode &node)
    {
        const auto bounds = node.BoundsInCanvas();
        Visual2DInputRouter router;
        PointerInput pointer{};
        pointer.available = true;
        pointer.position = {bounds.x + bounds.width * .5F, bounds.y + bounds.height * .5F};
        pointer.leftButtonDown = pointer.leftButtonPressed = true;
        router.Process(canvas, pointer);
        pointer.leftButtonDown = pointer.leftButtonPressed = false;
        pointer.leftButtonReleased = true;
        router.Process(canvas, pointer);
    }
    SongCatalogLoadResult AwaitCatalog(SongCatalogLoader &loader)
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
    void TestConverterProcess()
    {
        namespace fs = std::filesystem;
        const auto directory =
            fs::temp_directory_path() /
            (L"YOUMU-import-test-한글 " + std::to_wstring(std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directory(directory);
        const auto cleanup = [](fs::path *path)
        {
            std::error_code ignored;
            fs::remove_all(*path, ignored);
            delete path;
        };
        const auto guard = std::unique_ptr<fs::path, decltype(cleanup)>(new fs::path(directory), cleanup);
        const auto input = directory / L"map.osu", root = directory / L"Songs";
        std::ofstream(input) << "osu file format v14\n[General]\nAudioFilename: audio.mp3\nMode: 1\n"
                                "[Metadata]\nTitle:Song\nArtist:Artist\nCreator:Author\nVersion:Oni\n"
                                "[Difficulty]\nSliderMultiplier:1.4\nSliderTickRate:1\nOverallDifficulty:5\n"
                                "[TimingPoints]\n0,500,4,1,0,100,1,0\n[HitObjects]\n0,0,0,1,0\n";
        std::ofstream(directory / "audio.mp3", std::ios::binary) << "audio fixture";
        EditorOsuImport importer;
        const auto await = [&]
        {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{30};
            while (std::chrono::steady_clock::now() < deadline)
            {
                if (auto result = importer.Poll())
                    return *result;
                std::this_thread::sleep_for(std::chrono::milliseconds{1});
            }
            throw std::runtime_error("Converter process did not finish.");
        };
        importer.Start(input, root);
        const auto result = await();
        Check(result.success, result.message.c_str());
        const auto catalog = SongCatalog{}.Load(root);
        Check(catalog.Succeeded() && catalog.songs.size() == 1 && catalog.PatternCount() == 1,
              "The bundled converter process must produce a complete catalog entry.");
        const auto &convertedPattern = catalog.songs.front().patterns.front().pattern;
        Check(convertedPattern.judgementLevel == JudgeLevel{34.28},
              "An OD 5 import must retain hundredth precision and select the nearest GOOD window.");
        const AccuracyRange convertedRange("Imported.OD5", convertedPattern.judgementLevel);
        Check(convertedRange.HalfWindow(JudgementGrade::Good) == RhythmDuration{79'492},
              "The Client must read the imported level as a 79.492 ms GOOD half-window.");
        Check(convertedRange.Evaluate({}, RhythmTime{79'492}).grade == JudgementGrade::Good &&
                  convertedRange.Evaluate({}, RhythmTime{79'493}).grade == JudgementGrade::Bad,
              "The saved level must control the Client's actual GOOD boundary.");
        importer.Start(input, root);
        const auto collision = await();
        Check(!collision.success && !collision.message.empty(),
              "Converter collisions must report failure through Poll.");
        Check(SongCatalog{}.Load(root).PatternCount() == 1,
              "A failed process must preserve the existing pattern.");
    }
    void TestDifficultyClick(ScreenVisual2DManager &visuals, SongSelectionState &selection,
                             TextCatalog &texts, OptionSettings &options,
                             const EngineServices &services)
    {
        for (const auto purpose : {SongSelectPurpose::Gameplay, SongSelectPurpose::Editor})
        {
            selection.SelectVisibleSong(0);
            selection.selectedPatternIndex_ = 0;
            MusicSelectView clickable(visuals, selection, purpose, texts, options);
            clickable.Initialize(services);
            auto *canvas = visuals.FindCanvas(purpose == SongSelectPurpose::Gameplay ? 2 : 3);
            if (!canvas)
                throw std::runtime_error("Clickable selection Canvas missing.");
            for (int attempt = 0; attempt < 2; ++attempt)
            {
                const auto *difficulty = Find(canvas->Root(), "Difficulty.1");
                if (!difficulty)
                    throw std::runtime_error("Clickable difficulty missing.");
                Click(*canvas, *difficulty);
                InputState noKeys;
                Check(clickable.Update(noKeys, 0) == SongSelectCommand::Launch && selection.selectedPatternIndex_ == 1,
                      "A difficulty click must select that pattern and launch, including an already selected pattern.");
                Check(clickable.Update(noKeys, 0) == SongSelectCommand::None,
                      "A difficulty click must emit the launch command only once.");
            }
            if(purpose==SongSelectPurpose::Editor)
            {
                Check(!Find(canvas->Root(),"GameModeSelect"),"Editor selection retains its existing footer scope.");
                const auto *optionsButton=Find(canvas->Root(),"OptionSelect");
                if(!optionsButton)throw std::runtime_error("Editor global options button is missing.");
                Click(*canvas,*optionsButton);InputState input;static_cast<void>(clickable.Update(input,.2));
                static_cast<void>(clickable.Update(input,.2));
                Check(Find(canvas->Root(),"Options.Panel")->IsVisible(),"Editor footer still opens its global settings.");
            }
            clickable.Shutdown();
        }
    }
    void TestBackgroundPreview()
    {
        Renderer renderer;
        ScreenVisual2DManager visuals;
        visuals.Initialize(renderer, {1280, 720});
        MeshRenderSystem meshes;
        TextRenderSystem textRendering;
        AudioSystem audio;
        TextCatalog texts;
        OptionSettings options("build/background-test-unused.ini");
        SongSelectionState selection;
        SongCatalogEntry song;
        song.music.names.push_back("Background test");
        for (const auto *file : {"preview-wide.jpg", "preview-portrait.jpg", "", "preview-cpu-fail.jpg", "preview-gpu-fail.jpg"})
        {
            SongCatalogPattern pattern;
            pattern.pattern.backgroundFile = file;
            song.patterns.push_back(std::move(pattern));
        }
        selection.catalog_.songs.push_back(std::move(song));
        MusicSelectView view(visuals, selection, SongSelectPurpose::Gameplay, texts, options);
        view.Initialize({meshes, textRendering, renderer, audio, 1280, 720});
        auto *canvas = visuals.FindCanvas(1);
        if (!canvas) throw std::runtime_error("Background Canvas missing.");
        auto *image = Find(canvas->Root(), "PreviewImage");
        auto *empty = Find(canvas->Root(), "PreviewEmpty");
        auto *title = Find(canvas->Root(), "PreviewTitle");
        auto *preview = Find(canvas->Root(), "BackgroundPreview");
        if (!image || !empty || !title || !preview) throw std::runtime_error("Background nodes missing.");
        auto *sprite = image->GetComponent<SpriteVisualComponent>();
        if (!sprite) throw std::runtime_error("Background sprite missing.");
        InputState input;
        const auto tick = [&] { static_cast<void>(view.Update(input, 0)); };
        const auto select = [&](std::size_t index) {
            selection.selectedPatternIndex_ = index;
            view.RefreshSelectionPresentation();
        };
        Check(renderer.loaded.size() == 1 && renderer.loaded.back() == options.SongsRoot() / L"BG" / L"preview-wide.jpg",
              "Artwork must load from the selected Songs/BG.");
        select(1);
        tick();
        Check(renderer.loaded.size() == 2 && renderer.loaded.back() == options.SongsRoot() / L"BG" / L"preview-portrait.jpg",
              "Changing a difficulty must load the selected pattern from Songs/BG.");
        Check(image->IsVisible() && !empty->IsVisible() && !title->IsVisible() && sprite->Style().normalImage.value,
              "Ready artwork must replace the placeholder.");
        Check(std::abs(sprite->UvScale().x - 1) < .001F && std::abs(sprite->UvScale().y - .5625F) < .001F &&
                  std::abs(sprite->UvOffset().y - .21875F) < .001F,
              "Portrait artwork must use a centered aspect-preserving crop.");
        select(0);
        tick();
        Check(std::abs(sprite->UvScale().x - .5625F) < .001F && std::abs(sprite->UvScale().y - 1) < .001F,
              "Landscape artwork must retain its original aspect ratio.");
        const auto calls = renderer.loaded.size();
        for (int i = 0; i < 10; ++i) tick();
        Check(renderer.loaded.size() == calls, "Ready artwork must not be requested every frame.");
        for (const auto width : {900U, 1920U})
        {
            visuals.OnResize(width, 720);
            view.OnResize(width, 720);
            Check(image->NodeSize().width == preview->NodeSize().width &&
                      image->NodeSize().height == preview->NodeSize().height,
                  "Artwork must follow the existing responsive preview bounds.");
        }
        select(2);
        tick();
        Check(!image->IsVisible() && empty->IsVisible() && !sprite->Style().normalImage.value,
              "An unassigned background must clear the previous image.");
        for (const auto index : {3U, 4U})
        {
            select(index);
            tick();
            Check(!image->IsVisible() && empty->IsVisible(), "Decode/upload errors must preserve NO IMAGE.");
            const auto failedCalls = renderer.loaded.size();
            tick();
            view.RefreshSelectionPresentation();
            Check(renderer.loaded.size() == failedCalls, "Failed artwork must not retry every frame.");
        }
        select(0);
        tick();
        selection.searchText_ = L"no match";
        view.RebuildVisibleSongs();
        Check(!image->IsVisible() && empty->IsVisible(), "Empty search results must clear the selected artwork.");
        view.Shutdown();
        Check(visuals.CanvasCount() == 0, "Background shutdown must release the Canvas.");
    }
    void TestRecordHistoryView()
    {
        const auto directory=std::filesystem::temp_directory_path()/
            ("YOUMU-record-view-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(directory);
        struct Cleanup { std::filesystem::path path; ~Cleanup(){std::error_code e;std::filesystem::remove_all(path,e);} } cleanup{directory};
        const auto database=directory/"plays.sqlite3", patternPath=directory/"fixture.ymp";
        std::ofstream(patternPath)<<"Mode: Taiko";
        PlayRecord record;record.patternPath=patternPath;record.mode="Taiko";record.completed=true;record.cleared=true;
        record.totalNotes=record.processedNotes=1;
        for(int i=0;i<8;++i)
        {
            record.options.random=i%2==0;record.judgeLevel=record.options.random?70:50;
            record.accuracy=AccuracyScore::FromRate(.90+i*.001);PlayRecordStore(database).Save(record);
        }
        Renderer renderer;ScreenVisual2DManager visuals;visuals.Initialize(renderer,{1280,720});
        MeshRenderSystem meshes;TextRenderSystem textRendering;AudioSystem audio;
        TextCatalog texts("Client/Texts/Languages");OptionSettings options(directory/"Option.ini");
        SongSelectionState selection;SongCatalogEntry song;song.music.names.push_back("Record fixture");
        SongCatalogPattern pattern;pattern.patternPath=patternPath;pattern.pattern.mode="Taiko";pattern.pattern.name="Recorded";
        song.patterns.push_back(pattern);pattern.patternPath=directory/"other.ymp";song.patterns.push_back(pattern);
        selection.catalog_.songs.push_back(std::move(song));
        MusicSelectView view(visuals,selection,SongSelectPurpose::Gameplay,texts,options,database);
        view.Initialize({meshes,textRendering,renderer,audio,1280,720});auto *canvas=visuals.FindCanvas(1);
        Check(canvas!=nullptr,"Record history owns its selection canvas.");InputState input;
        const auto rows=[&]{return Find(canvas->Root(),"RecordRows")->Children().size();};
        const auto wait=[&](std::size_t expected){
            const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds{5};
            do{static_cast<void>(view.Update(input,0));if(rows()==expected)return;std::this_thread::sleep_for(std::chrono::milliseconds{1});}
            while(std::chrono::steady_clock::now()<deadline);
            throw std::runtime_error("Record history page did not publish the expected rows.");
        };
        wait(6);auto *next=Find(canvas->Root(),"RecordNext");if(!next)throw std::runtime_error("Record next page control is missing.");Check(next->IsEnabled(),"Actual history has an older page.");
        Click(*canvas,*next);static_cast<void>(view.Update(input,0));wait(2);
        Click(*canvas,*Find(canvas->Root(),"RecordPrevious"));static_cast<void>(view.Update(input,0));wait(6);
        const auto thread=GetCurrentThreadId();std::atomic_bool handled{};
        std::jthread queryInput([&](std::stop_token stop){
            while(!stop.stop_requested()&&!handled)
            {
                EnumThreadWindows(thread,[](HWND window,LPARAM p)->BOOL{
                    wchar_t name[32]{};GetClassNameW(window,name,32);
                    if(std::wstring_view(name)!=L"#32770"||!GetDlgItem(window,100))return TRUE;
                    ShowWindow(window,SW_HIDE);SetDlgItemTextW(window,100,L"JudgeLevel >= 60 AND Random = true");
                    PostMessageW(window,WM_COMMAND,IDOK,0);*reinterpret_cast<std::atomic_bool *>(p)=true;return FALSE;
                },reinterpret_cast<LPARAM>(&handled));
                std::this_thread::sleep_for(std::chrono::milliseconds{1});
            }
        });
        Click(*canvas,*Find(canvas->Root(),"RecordQuery"));static_cast<void>(view.Update(input,0));queryInput.request_stop();queryInput.join();
        Check(handled,"The history condition is editable through its actual action.");wait(4);
        for(const auto &row:Find(canvas->Root(),"RecordRows")->Children())
            Check(row->GetComponent<TextVisualComponent>()->Text().find(L"JL 70.00")!=std::wstring::npos,"Only matching effective JudgeLevel records appear.");
        selection.selectedPatternIndex_=1;view.RefreshSelectionPresentation();static_cast<void>(view.Update(input,0));wait(0);
        view.Shutdown();
    }
    void TestPlayOptionsForm()
    {
        TextCatalog texts("Client/Texts/Languages");
        PlayOptions options;
        for (const bool cancel : {false,true})
        {
            struct Driver { bool cancel{}; std::atomic_bool handled{}; } driver;
            driver.cancel=cancel;
            const auto threadId=GetCurrentThreadId();
            std::jthread input([&](std::stop_token stop) {
                while(!stop.stop_requested()&&!driver.handled)
                {
                    EnumThreadWindows(threadId,[](HWND window,LPARAM parameter)->BOOL {
                        auto &d=*reinterpret_cast<Driver *>(parameter);
                        wchar_t name[32]{};GetClassNameW(window,name,32);
                        if(std::wstring_view(name)!=L"#32770"||!GetDlgItem(window,112))return TRUE;
                        ShowWindow(window,SW_HIDE);
                        if(d.cancel)
                        {
                            SetDlgItemTextW(window,108,L"2.0");
                            PostMessageW(window,WM_COMMAND,IDCANCEL,0);
                        }
                        else
                        {
                            SendDlgItemMessageW(window,100,CB_SETCURSEL,1,0);
                            SetDlgItemTextW(window,101,L"240");
                            for(const auto id:{102,103,104,105,110,111})SendDlgItemMessageW(window,id,BM_SETCHECK,BST_CHECKED,0);
                            SendDlgItemMessageW(window,109,BM_SETCHECK,BST_UNCHECKED,0);
                            SetDlgItemTextW(window,106,L"18446744073709551615");
                            SetDlgItemTextW(window,107,L"35");SetDlgItemTextW(window,108,L"1.5");SetDlgItemTextW(window,112,L"60.00");
                            PostMessageW(window,WM_COMMAND,IDOK,0);
                        }
                        d.handled=true;return FALSE;
                    },reinterpret_cast<LPARAM>(&driver));
                    std::this_thread::sleep_for(std::chrono::milliseconds{1});
                }
            });
            const bool accepted=EditPlayOptions(options,texts);
            input.request_stop();input.join();
            Check(driver.handled&&accepted!=cancel,"Native form accepts all fields and supports Cancel.");
            Check(options.scrollUnit==PlayOptions::ScrollUnit::Bpm&&options.scrollSpeed==240&&options.ignoreScrollEffects&&
                options.metronome&&options.mirror&&options.random&&options.randomSeed==UINT64_MAX&&options.randomAttemptProbability==.35&&
                options.accelerate==1.5&&!options.preserveMusicPitch&&options.preserveHitSoundPitch&&options.judgeLevel==JudgeLevel{60},
                "Play form independently captures BPM, seed/probability, pitch flags and hundredth JudgeLevel; Cancel preserves them.");
        }
    }
    void TestOptionsToggle(MusicSelectView &view, Visual2DCanvas &canvas)
    {
        auto *button = Find(canvas.Root(), "OptionSelect");
        auto *panel = Find(canvas.Root(), "Options.Panel");
        if (!button || !panel)
            throw std::runtime_error("Options toggle controls missing.");
        const auto *mode = Find(canvas.Root(), "GameModeSelect");
        Check(mode && mode->BoundsInCanvas().x + mode->BoundsInCanvas().width <= button->BoundsInCanvas().x, "Mode and play option controls must have separate footer bounds.");
        Check(Find(canvas.Root(), "RecordQuery") != nullptr, "History panel exposes its condition query action.");
        InputState keys;
        InputStateTestAccess::Key(keys,VK_LCONTROL,true);
        InputStateTestAccess::Key(keys,'O',true);
        static_cast<void>(view.Update(keys,.2));
        InputStateTestAccess::Begin(keys);
        InputStateTestAccess::Key(keys,'O',false);
        static_cast<void>(view.Update(keys,.2));
        Check(panel->IsVisible(), "Ctrl+O opens global settings independently of play options.");
        InputStateTestAccess::Begin(keys);
        InputStateTestAccess::Key(keys,'O',true);
        static_cast<void>(view.Update(keys,.2));
        InputStateTestAccess::Begin(keys);
        InputStateTestAccess::Key(keys,'O',false);
        static_cast<void>(view.Update(keys,.2));
        Check(!panel->IsVisible(), "Ctrl+O closes global settings.");
    }
} // namespace

int main()
{
    try
    {
        TestConverterProcess();
        TestPlayOptionsForm();
        {
            Visual2DCanvas canvas;
            auto &label = CreateLabel(canvas.Root(), {0, 0, 100, 30}, L"");
            const std::wstring unicode = L"\U0001F600ABCDEFGH";
            auto &marquee = label.AddComponent<MarqueeTextComponent>(unicode, 0.28);
            marquee.Update(0);
            const auto initial = canvas.BuildDrawList();
            const auto source = initial.back().text;
            source->measuredSize = Size{180, 20};
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
        Check(InitializeBuiltInAssets(error), error.c_str());
        TestRecordHistoryView();
        TestBackgroundPreview();
        SongCatalogLoader loader;
        loader.Refresh(UserSongs());
        const auto first = AwaitCatalog(loader);
        Check(!first.songs.empty(), "Worker must publish the real catalog.");
        const auto playable = std::ranges::find_if(first.songs, [](const auto &song) { return !song.patterns.empty(); });
        Check(playable != first.songs.end(), "The worker regression requires a real playable chart.");
        const auto &pattern = playable->patterns.front();
        Check(pattern.pattern.notes.empty() && pattern.noteCount.has_value(),
              "Runtime catalog loading must publish compact metadata with the original note count.");
        PlaySessionLoader sessions;
        sessions.Start({pattern.patternPath, pattern.effectPath, playable->audioPath, pattern.pattern.mode});
        Check(sessions.Busy(), "A session worker must retain its job until publication.");
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{10};
        std::optional<ModeLoadResult> loaded;
        while (!(loaded = sessions.Poll()) && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
        Check(loaded && loaded->Succeeded() && !sessions.Busy() && !loaded->session->Gear().Lanes().empty(),
              "The worker must create and transfer a real playable session without loading on Poll.");
        sessions.Start({"missing-regression.ymp", {}, "missing.mp3", "Taiko"});
        loaded.reset();
        while (!(loaded = sessions.Poll()) && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
        Check(loaded && !loaded->Succeeded() && !loaded->diagnostics.empty(),
              "A failed pattern worker must return diagnostics instead of throwing on the UI thread.");
        loader.Refresh(UserSongs());
        const auto repeated = AwaitCatalog(loader);
        Check(first.songs.size() == repeated.songs.size() && first.PatternCount() == repeated.PatternCount(),
              "A retained parser cache must preserve catalog results.");
        loader.Refresh(UserSongs() / "nonexistent-regression-directory");
        const auto failed = AwaitCatalog(loader);
        Check(failed.songs.empty() && !failed.diagnostics.empty(),
              "Worker I/O errors must be published as diagnostics.");
        loader.Refresh(UserSongs());
        loader.Refresh(UserSongs() / "missing-new-root");
        const auto superseded = AwaitCatalog(loader);
        Check(superseded.songs.empty() && !superseded.diagnostics.empty(),
              "A root change must discard the older pending catalog.");
        loader.Refresh(UserSongs());
        loader.Refresh(UserSongs() / "missing-new-root");
        loader.Refresh(UserSongs());
        Check(!AwaitCatalog(loader).songs.empty(), "Switching back must cancel an obsolete queued root.");

        Renderer renderer;
        ScreenVisual2DManager visuals;
        visuals.Initialize(renderer, {1280, 720});
        MeshRenderSystem meshes;
        TextRenderSystem textRendering;
        AudioSystem audio;
        TextCatalog texts;
        OptionSettings options("build/performance-view-test-unused.ini");
        SongSelectionState selection;
        // Synthetic data stays in this test process; no song or settings file is written.
        for (int i = 0; i < 5000; ++i)
        {
            SongCatalogEntry song;
            song.music.names.push_back("Test song " + std::to_string(i));
            song.music.artists.push_back("Test artist");
            song.patterns.resize(2);
            selection.catalog_.songs.push_back(std::move(song));
        }
        MusicSelectView view(visuals, selection, SongSelectPurpose::Gameplay, texts, options);
        view.Initialize({meshes, textRendering, renderer, audio, 1280, 720});
        auto *canvas = visuals.FindCanvas(1);
        if (!canvas)
            throw std::runtime_error("Music selection Canvas missing.");
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
        TestOptionsToggle(view, *canvas);
        view.Shutdown();
        Check(visuals.CanvasCount() == 0, "Virtualized view must release its Canvas.");
        selection.searchText_.clear();
        TestDifficultyClick(visuals, selection, texts, options, {meshes, textRendering, renderer, audio, 1280, 720});
        selection.catalog_.songs[0].patterns.clear();
        MusicSelectView editor(visuals, selection, SongSelectPurpose::Editor, texts, options);
        editor.Initialize({meshes, textRendering, renderer, audio, 1280, 720});
        canvas = visuals.FindCanvas(4);
        if (!canvas)
            throw std::runtime_error("Editor song-select Canvas missing.");
        auto *add = Find(canvas->Root(), "AddDifficulty.0");
        if (!add)
            throw std::runtime_error("Songs without a chart must offer the plus button.");
        Check(add->IsEnabled(), "Chart creation must be enabled after catalog loading.");
        Click(*canvas, *add);
        InputState input;
        Check(editor.Update(input, 0) == SongSelectCommand::None && selection.focusedSongPosition_ == 0,
              "Plus must select its song and open the menu without creating files.");
        auto *menu = Find(canvas->Root(), "SongAddition.Menu");
        auto *create = Find(canvas->Root(), "SongAddition.NewDifficulty");
        auto *convert = Find(canvas->Root(), "SongAddition.ConvertOsu");
        if (!menu || !create || !convert)
            throw std::runtime_error("Song addition menu is incomplete.");
        Check(menu->IsVisible(), "Plus must show the addition popup.");
        Click(*canvas, *create);
        Check(editor.Update(input, 0) == SongSelectCommand::CreateDifficulty && !menu->IsVisible(),
              "New Difficulty must close the popup and preserve its existing command.");
        add = Find(canvas->Root(), "AddDifficulty.0");
        if (!add)
            throw std::runtime_error("Refreshed plus button missing.");
        Click(*canvas, *add);
        static_cast<void>(editor.Update(input, 0));
        Click(*canvas, *convert);
        Check(editor.Update(input, 0) == SongSelectCommand::ConvertOsu && !menu->IsVisible(),
              "Convert .osu must issue only the import command.");
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
