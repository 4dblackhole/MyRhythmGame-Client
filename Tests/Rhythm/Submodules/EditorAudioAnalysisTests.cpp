#include "Audio/Submodules/AudioDecode.h"
#include "TestSupport.h"
#include <numbers>

namespace finger_drum::tests
{
    namespace
    {
        void VerifyToneAnalysis(const editor::AudioAnalysis &analysis)
        {
            Require(analysis.sampleRate == 48'000 && analysis.frames.size() == 100 &&
                        std::abs(analysis.durationSeconds - 1) < 1e-9,
                    "Analysis must retain the PCM sample rate and exact duration.");
            const auto &frame = analysis.frames[30];
            const auto strongest = std::ranges::max_element(frame.bands);
            const auto band = std::distance(frame.bands.begin(), strongest);
            const double frequency =
                analysis.minimumFrequencyHz *
                std::pow(analysis.maximumFrequencyHz / analysis.minimumFrequencyHz,
                         (static_cast<double>(band) + .5) / static_cast<double>(frame.bands.size()));
            const float db = editor::SpectrumFloorDb * (1 - *strongest);
            Require(std::abs(frequency - 1000) < 80 && db > -9 && db < -4 && frame.minimum < -.49F &&
                        frame.maximum > .49F,
                    "A half-scale 1 kHz stereo tone must stay visible at about -6 dBFS, even "
                    "when its channels have opposite phases.");
            Require(std::ranges::all_of(analysis.frames[90].bands, [](float level) { return level < .001F; }) &&
                        analysis.frames[90].peak < .0001F,
                    "Silence must remain at the spectrum floor without a false waveform.");
        }

        void TestStereoSpectrumFrequencyAndLevel()
        {
            TemporaryDirectory directory("finger-drum-spectrum");
            const auto path = directory.Path() / "opposite-phase.wav";
            std::ofstream wav(path, std::ios::binary);
            const auto integer = [&](std::uint32_t value, int bytes)
            {
                for (int byte = 0; byte < bytes; ++byte)
                    wav.put(static_cast<char>((value >> (8 * byte)) & 255));
            };
            constexpr std::uint32_t rate = 48'000, dataBytes = rate * 2 * 2;
            wav.write("RIFF", 4);
            integer(36 + dataBytes, 4);
            wav.write("WAVEfmt ", 8);
            integer(16, 4);
            integer(1, 2);
            integer(2, 2);
            integer(rate, 4);
            integer(rate * 4, 4);
            integer(4, 2);
            integer(16, 2);
            wav.write("data", 4);
            integer(dataBytes, 4);
            for (std::uint32_t sample = 0; sample < rate; ++sample)
            {
                const auto amplitude = sample < rate * 7 / 10
                                           ? static_cast<std::int16_t>(std::lround(
                                                 16384 * std::sin(2 * std::numbers::pi * 1000 * sample / rate)))
                                           : std::int16_t{0};
                integer(static_cast<std::uint16_t>(amplitude), 2);
                integer(static_cast<std::uint16_t>(-amplitude), 2);
            }
            wav.close();
            VerifyToneAnalysis(editor::AnalyzeAudio(path, {}));
        }

        void TestVorbisAnalysis()
        {
            const auto fixture =
                std::filesystem::path(__FILE__).parent_path().parent_path() / "Fixtures/Audio/OppositePhase.ogg";
            VerifyToneAnalysis(editor::AnalyzeAudio(fixture, {}));
            TemporaryDirectory directory("finger-drum-vorbis");
            for (const auto *name : {L"음원 解析.OGG", L"renamed.mp3"})
            {
                const auto path = directory.Path() / name;
                std::filesystem::copy_file(fixture, path);
                VerifyToneAnalysis(editor::AnalyzeAudio(path, {}));
            }

            std::stop_source cancellation;
            cancellation.request_stop();
            Require(editor::AnalyzeAudio(fixture, cancellation.get_token()).frames.empty(),
                    "An already-cancelled analysis must not open or decode the file.");
            std::stop_source inFlight;
            const auto partial = editor::detail::DecodeAudio(fixture, inFlight.get_token(),
                                                             [&](editor::detail::DecodedAudio &audio)
                                                             {
                                                                 if (audio.totalFrames)
                                                                     inFlight.request_stop();
                                                             });
            Require(partial.totalFrames > 0 && partial.totalFrames <= 4096,
                    "Vorbis decoding must observe cancellation between bounded PCM chunks.");

            const auto broken = directory.Path() / "truncated.ogg";
            {
                std::ifstream input(fixture, std::ios::binary);
                std::array<char, 32> header{};
                input.read(header.data(), static_cast<std::streamsize>(header.size()));
                std::ofstream output(broken, std::ios::binary);
                output.write(header.data(), static_cast<std::streamsize>(header.size()));
            }
            bool rejected = false;
            try
            {
                static_cast<void>(editor::AnalyzeAudio(broken, {}));
            }
            catch (const std::runtime_error &error)
            {
                rejected = std::string_view(error.what()).find("Ogg Vorbis") != std::string_view::npos;
            }
            Require(rejected, "A truncated Ogg stream must report its decoder failure.");
        }
    } // namespace

    void TestEditorAudioAnalysis(const std::filesystem::path &songsRoot)
    {
        TestStereoSpectrumFrequencyAndLevel();
        TestVorbisAnalysis();
        const auto catalog = chart::SongCatalog{}.Load(songsRoot);
        Require(!catalog.songs.empty(), "Audio analysis requires the shipped catalog.");
        const auto analysis = editor::AnalyzeAudio(catalog.songs.front().audioPath, {});
        Require(analysis.durationSeconds > 1 && analysis.frames.size() > 100 &&
                    std::ranges::any_of(analysis.frames, [](const auto &f) { return f.peak > 0.01F; }),
                "The shipped MP3 must decode into real waveform/spectrum data.");
        Require(std::ranges::any_of(
                    analysis.frames, [](const auto &frame)
                    { return std::ranges::any_of(frame.bands, [](const float level) { return level > .06F; }); }),
                "The shipped MP3 must produce visible music spectrum bands.");
        const auto soundsRoot = songsRoot.parent_path() / "Skins/Default Skin/HitSounds/TaikoMode";
        if (std::filesystem::is_directory(soundsRoot))
            for (const auto *name : {"don.wav", "kat.wav", "bigdon.wav", "bigkat.wav"})
            {
                const auto hit = editor::AnalyzeAudio(soundsRoot / name, {});
                Require(!hit.frames.empty() &&
                            std::ranges::any_of(
                                hit.frames, [](const auto &frame)
                                { return std::ranges::any_of(frame.bands, [](float value) { return value > .1F; }); }),
                        "Every default hit sound must produce a non-empty real FFT "
                        "spectrum.");
            }
    }
} // namespace finger_drum::tests
