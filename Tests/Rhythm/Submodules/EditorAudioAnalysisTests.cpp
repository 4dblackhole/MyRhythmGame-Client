#include "TestSupport.h"
#include <numbers>

namespace tests
{
    void TestEditorAudioAnalysis()
    {
        // The domain consumes injected PCM; codec/filesystem integration is
        // exercised by the Client's editor presentation regression runner.
        constexpr std::uint32_t rate = 48'000, channels = 2;
        std::vector<float> samples(static_cast<std::size_t>(rate) * channels);
        for (std::uint32_t frame = 0; frame < rate; ++frame)
        {
            const auto value = frame < rate * 7 / 10
                ? .5F * static_cast<float>(std::sin(2 * std::numbers::pi * 1000 * frame / rate)) : 0.0F;
            samples[frame * channels] = value;
            samples[frame * channels + 1] = -value;
        }
        std::size_t reads{};
        const PcmReader reader = [&](const std::filesystem::path &, std::stop_token stop, const PcmConsumer &consume) {
            ++reads;
            constexpr std::size_t chunk = 4096 * channels;
            const auto pcm = std::span<const float>(samples);
            for (std::size_t offset = 0; offset < pcm.size() && !stop.stop_requested(); offset += chunk)
                consume(channels, rate, pcm.subspan(offset, std::min(chunk, pcm.size() - offset)));
        };
        const auto analysis = AnalyzeAudio({}, {}, reader);
        Require(reads == 1 && analysis.sampleRate == rate && analysis.frames.size() == 100 &&
                    std::abs(analysis.durationSeconds - 1) < 1e-9,
                "Analysis must retain injected PCM sample rate and exact duration.");
        const auto &frame = analysis.frames[30];
        const auto strongest = std::ranges::max_element(frame.bands);
        const auto band = std::distance(frame.bands.begin(), strongest);
        const double frequency = analysis.minimumFrequencyHz *
            std::pow(analysis.maximumFrequencyHz / analysis.minimumFrequencyHz,
                     (static_cast<double>(band) + .5) / static_cast<double>(frame.bands.size()));
        const float db = SpectrumFloorDb * (1 - *strongest);
        Require(std::abs(frequency - 1000) < 80 && db > -9 && db < -4 && frame.minimum < -.49F && frame.maximum > .49F,
                "Opposite-phase stereo must preserve its 1 kHz, half-scale spectrum and waveform.");
        Require(std::ranges::all_of(analysis.frames[90].bands, [](float level) { return level < .001F; }) &&
                    analysis.frames[90].peak < .0001F,
                "Injected silence must remain at the spectrum floor.");

        std::stop_source stop;
        stop.request_stop();
        Require(AnalyzeAudio({}, stop.get_token(), reader).frames.empty() && reads == 1,
                "An already-cancelled analysis must not invoke the PCM reader.");
    }
}
