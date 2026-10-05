#include "App/OptionSettings.h"
#include "Presentation/OptionsPanel.h"

#include <Windows.h>
#include <chrono>
#include <fstream>
#include <iostream>

namespace
{
    namespace a = mrg::audio;
    namespace v = mrg::visual2d;
    void Check(bool condition, const char *message)
    {
        if (!condition) throw std::runtime_error(message);
    }

    class TestBackend final : public a::IAudioBackend
    {
      public:
        bool rejectAsio{};
        a::AudioOutputBackend requested{a::AudioOutputBackend::Automatic};
        int driver{};
        int lastSetDriver{-1};
        std::vector<a::AudioDeviceInfo> devices;
        void SetDevices()
        {
            if (requested == a::AudioOutputBackend::Asio)
                devices = {{requested, 5, "ASIO device", 48000, 2}};
            else devices = {{a::AudioOutputBackend::Wasapi, 0, "Speakers", 48000, 2},
                            {a::AudioOutputBackend::Wasapi, 2, "Headphones", 48000, 2}};
            driver = devices.front().driverIndex;
        }
        bool Initialize(const a::AudioConfig &config, std::string &error) override
        {
            requested = config.preferredBackend;
            SetDevices();
            error.clear();
            return true;
        }
        void Update() override {}
        void Shutdown() noexcept override {}
        std::string_view Name() const noexcept override { return "Test"; }
        a::AudioOutputBackend RequestedOutput() const noexcept override { return requested; }
        a::AudioOutputBackend ActiveOutput() const noexcept override
        {
            return requested == a::AudioOutputBackend::Automatic ? a::AudioOutputBackend::Wasapi : requested;
        }
        bool SetOutputBackend(a::AudioOutputBackend value, std::string &error) override
        {
            if (value == a::AudioOutputBackend::Asio && rejectAsio)
            {
                error = "ASIO device unavailable";
                return false;
            }
            if (value != requested) { requested = value; SetDevices(); }
            error.clear();
            return true;
        }
        int DriverCount() const noexcept override { return 6; }
        const std::vector<a::AudioDeviceInfo> &OutputDrivers() const noexcept override { return devices; }
        int ActiveDriverIndex() const noexcept override { return driver; }
        bool SetOutputDriver(int index, std::string &error) override
        {
            for (const auto &entry : devices)
            {
                if (entry.driverIndex != index) continue;
                lastSetDriver = driver = index;
                error.clear();
                return true;
            }
            error = "Missing device";
            return false;
        }
        int RequestedSampleRate() const noexcept override { return 0; }
        int SampleRate() const noexcept override { return 48000; }
        std::uint32_t DspBufferLength() const noexcept override { return 256; }
        int DspBufferCount() const noexcept override { return 4; }
        double EstimatedDspLatencyMilliseconds() const noexcept override { return 0; }
        bool SetSampleRate(int, std::string &) override { return false; }
        bool SetDspBufferSize(std::uint32_t, int, std::string &) override { return false; }
        std::uint64_t DspClock() const noexcept override { return 0; }
        std::unique_ptr<a::IAudioBusBackend> CreateBus(std::string_view,
            a::IAudioBusBackend *, std::string &) override { return {}; }
    };
    TestBackend *backend{};
    int backendCreations{};
    std::unique_ptr<a::IAudioBackend> CreateBackend()
    {
        auto value = std::make_unique<TestBackend>();
        backend = value.get();
        ++backendCreations;
        return value;
    }
    class TestClip final : public a::IAudioClipBackend
    {
        std::unique_ptr<a::IAudioVoiceBackend> Play(const a::AudioPlaybackSettings &,
            a::IAudioBusBackend *, std::string &) override { return {}; }
    };
    std::unique_ptr<a::IAudioClipBackend> CreateClip(a::IAudioBackend &,
        const std::filesystem::path &, a::AudioLoadMode, std::string &) { return std::make_unique<TestClip>(); }

    v::Visual2DNode &FindNode(v::Visual2DNode &root, std::string_view name)
    {
        if (root.Name() == name) return root;
        for (const auto &child : root.Children())
        {
            try { return FindNode(*child, name); }
            catch (const std::out_of_range &) {}
        }
        throw std::out_of_range("Node not found");
    }
    v::ComboBoxBehaviorComponent &Combo(v::Visual2DCanvas &canvas, std::string_view name)
    {
        return *FindNode(canvas.Root(), name).GetComponent<v::ComboBoxBehaviorComponent>();
    }
    v::Action Select(v::Visual2DCanvas &canvas, std::string_view name, std::size_t index)
    {
        v::Action action;
        action.type = v::ActionType::SelectionChanged;
        action.source = FindNode(canvas.Root(), name).Id();
        action.selectedIndex = index;
        return action;
    }

    void TestOptions(const std::filesystem::path &folder)
    {
        std::string error;
        mrg_client::OptionSettings settings(folder / "Option.ini");
        Check(settings.Load(error), error.c_str());
        Check(std::filesystem::exists(settings.Path()), "First load must create Option.ini.");
        auto values = settings.Values();
        values.skinSet = L"한국어 Skin";
        values.language = finger_drum::texts::Language::English;
        values.audioOutput = a::AudioOutputBackend::Wasapi;
        values.driverIndex = 2;
        values.driverName = "Headphones";
        Check(settings.Save(values, error), error.c_str());
        mrg_client::OptionSettings restored(settings.Path());
        Check(restored.Load(error), error.c_str());
        Check(restored.Values().skinSet == values.skinSet && restored.Values().driverIndex == 2 &&
              restored.Values().language == values.language && restored.Values().driverName == values.driverName,
              "Unicode skin, language and audio settings must survive another instance.");
        {
            std::ofstream file(folder / "invalid.ini");
            file << "[Audio]\nDriverIndex=-5\n";
        }
        mrg_client::OptionSettings invalid(folder / "invalid.ini");
        Check(!invalid.Load(error) && invalid.Values().driverIndex == -1,
              "Malformed settings must not publish a partial candidate.");
        values.driverName = "bad\nentry";
        Check(!restored.Save(values, error) && restored.Values().driverName == "Headphones",
              "INI line injection must be rejected without changing values.");

        // The panel uses the existing audio system and real driver indices, not row indices.
        values = restored.Values();
        values.skinSet = L"Default Skin";
        values.driverIndex = 2;
        Check(settings.Save(values, error), error.c_str());
        finger_drum::texts::TextCatalog texts;
        texts.SetLanguage(values.language);
        a::AudioSystem audio;
        a::AudioConfig config;
        config.preferredBackend = values.audioOutput;
        Check(audio.Initialize(config, &CreateBackend, &CreateClip, error), error.c_str());
        auto clip = audio.LoadSound("test.wav", error);
        Check(clip != nullptr, "Test clip must load.");

        v::Visual2DCanvas canvas;
        finger_drum::presentation::OptionsPanel panel(texts, settings);
        panel.Initialize(canvas, audio);
        panel.Toggle();
        panel.Update(0.2);
        Check(panel.IsVisible(), "The panel must finish its slide animation.");
        Check(Combo(canvas, "Options.Middleware.Combo").Items() == std::vector<std::wstring>{L"FMOD"},
              "Only installed middleware may appear.");
        Check(panel.ProcessAction(Select(canvas, "Options.Driver.Combo", 0)) && backend->lastSetDriver == 0,
              "Driver selection must apply immediately.");
        Check(panel.ProcessAction(Select(canvas, "Options.Driver.Combo", 1)) && backend->lastSetDriver == 2,
              "Sparse driver numbers must use the device's index rather than ComboBox row.");
        backend->rejectAsio = true;
        Check(panel.ProcessAction(Select(canvas, "Options.Output.Combo", 2)) && audio.ActiveDriverIndex() == 2 &&
              settings.Values().audioOutput == a::AudioOutputBackend::Wasapi,
              "Rejected output must preserve the previous selection and persisted preferences.");
        backend->rejectAsio = false;
        // Scroll-position geometry: the ASIO popup row overlaps the driver field.
        // Exercise the real hit router so the lower field cannot steal this click.
        FindNode(canvas.Root(), "Options.Content").SetPosition({0, 0});
        v::Visual2DInputRouter router;
        auto click = [&](v::Point position) {
            v::PointerInput input;
            input.position = position;
            input.leftButtonDown = input.leftButtonPressed = true;
            router.Process(canvas, input);
            input.leftButtonDown = input.leftButtonPressed = false;
            input.leftButtonReleased = true;
            router.Process(canvas, input);
            return canvas.TakeActions();
        };
        const auto bounds = FindNode(canvas.Root(), "Options.Output.Combo").BoundsInCanvas();
        static_cast<void>(click({bounds.x + bounds.width / 2, bounds.y + bounds.height / 2}));
        Check(Combo(canvas, "Options.Output.Combo").IsExpanded(), "Output list must open through pointer input.");
        const auto actions = click({bounds.x + bounds.width / 2, bounds.y - 2.5F * 42.0F});
        Check(actions.size() == 1 && actions.front().source == FindNode(canvas.Root(), "Options.Output.Combo").Id() &&
              actions.front().selectedIndex == 2 && panel.ProcessAction(actions.front()),
              "The ASIO popup row must receive clicks ahead of the driver field below it.");
        Check(audio.ActiveOutput() == a::AudioOutputBackend::Asio && backendCreations == 1 && clip->IsValid(),
              "Output switching must reuse the system and retain loaded clips without restart.");
        Check(Combo(canvas, "Options.Driver.Combo").Items() == std::vector<std::wstring>{L"5: ASIO device"},
              "Drivers must refresh from the selected output API only.");
        Check(settings.Values().driverIndex == 5 && settings.Values().driverName == "ASIO device",
              "Successful switching must persist the new output's actual driver.");

        Check(SetFileAttributesW(settings.Path().c_str(), FILE_ATTRIBUTE_READONLY) != FALSE,
              "Test file must be made read-only.");
        Check(panel.ProcessAction(Select(canvas, "Options.Output.Combo", 1)) &&
              audio.ActiveOutput() == a::AudioOutputBackend::Asio && audio.ActiveDriverIndex() == 5 &&
              settings.Values().audioOutput == a::AudioOutputBackend::Asio,
              "Saving failure must restore runtime audio and preserve persisted values.");
        Check(panel.ProcessAction(Select(canvas, "Options.Language.Combo", 0)) &&
              texts.CurrentLanguage() == finger_drum::texts::Language::English,
              "Saving failure must preserve the current language.");
        Check(SetFileAttributesW(settings.Path().c_str(), FILE_ATTRIBUTE_NORMAL) != FALSE,
              "Test file attributes must be restored.");
        Check(panel.ProcessAction(Select(canvas, "Options.Language.Combo", 0)) &&
              texts.CurrentLanguage() == finger_drum::texts::Language::Korean,
              "Language must apply after a successful save.");
        mrg_client::OptionSettings nextRun(settings.Path());
        Check(nextRun.Load(error) && nextRun.Values().language == finger_drum::texts::Language::Korean &&
              nextRun.Values().audioOutput == a::AudioOutputBackend::Asio && nextRun.Values().driverIndex == 5,
              "The next run must read the most recent successful settings.");
        backend->devices.clear();
        backend->driver = -1;
        panel.RefreshTexts();
        Check(!FindNode(canvas.Root(), "Options.Driver.Combo").IsEnabled() &&
              Combo(canvas, "Options.Driver.Combo").Items().size() == 1,
              "An empty device list must be explicit and non-selectable.");
        panel.Shutdown();
        Check(!panel.ProcessAction(actions.front()), "A closed panel must reject stale Canvas actions.");
        clip.reset();
    }

    void TestActualAudio()
    {
        a::AudioSystem audio;
        a::AudioConfig config;
        config.allowNoSoundFallback = false;
        std::string error;
        Check(audio.Initialize(config, nullptr, nullptr, error), error.c_str());
        auto clip = audio.LoadSound(std::filesystem::path(L"FingerDrum.Assets/Assets/Skins/Default Skin/HitSounds/TaikoMode/pop.wav"), error);
        Check(clip != nullptr, error.c_str());
        Check(audio.SetOutputBackend(a::AudioOutputBackend::Wasapi, error), error.c_str());
        Check(audio.ActiveOutput() == a::AudioOutputBackend::Wasapi && !audio.OutputDrivers().empty(),
              "Actual FMOD must report WASAPI and its current drivers after switching.");
        Check(audio.SetOutputDriver(audio.ActiveDriverIndex(), error), error.c_str());
        Check(clip->IsValid() && clip->LoadState(error) == a::AudioClipLoadState::Ready,
              "A loaded native clip must remain usable after output/driver selection.");
        std::cout << "Actual FMOD Automatic -> WASAPI and driver selection passed (loaded clip retained).\n";
    }
}

int main()
{
    const auto folder = std::filesystem::temp_directory_path() /
        (L"FingerDrumOptionsTest-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
         std::to_wstring(std::chrono::steady_clock::now().time_since_epoch().count()));
    try
    {
        std::filesystem::create_directory(folder);
        TestOptions(folder);
        TestActualAudio();
        std::filesystem::remove_all(folder);
        std::cout << "Options panel and persistence tests passed.\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        SetFileAttributesW((folder / L"Option.ini").c_str(), FILE_ATTRIBUTE_NORMAL);
        std::error_code ignored;
        std::filesystem::remove_all(folder, ignored);
        std::cerr << error.what() << '\n';
        return 1;
    }
}
