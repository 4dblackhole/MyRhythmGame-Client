#pragma once
#include <array>
#include <cstddef>
#include <filesystem>
#include <cstdint>
#include <functional>
#include <span>
#include <stop_token>
#include <vector>

namespace finger_drum::editor
{
    inline constexpr std::size_t SpectrumBandCount = 96;
    inline constexpr float SpectrumFloorDb = -90.0F;
    struct SpectrumFrame
    {
        float peak{};
        float minimum{}, maximum{};
        std::array<float, SpectrumBandCount> bands{};
    };
    struct AudioAnalysis
    {
        double secondsPerFrame{};
        double durationSeconds{};
        unsigned int sampleRate{};
        double minimumFrequencyHz{20}, maximumFrequencyHz{};
        std::vector<SpectrumFrame> frames;
    };
    using PcmConsumer = std::function<void(std::uint32_t channels, std::uint32_t sampleRate,
                                           std::span<const float> samples)>;
    using PcmReader = std::function<void(const std::filesystem::path&, std::stop_token, const PcmConsumer&)>;
    // Source decoding is supplied by the host; this domain owns only PCM/FFT.
    AudioAnalysis AnalyzeAudio(const std::filesystem::path &path, std::stop_token stop, const PcmReader &read);
} // namespace finger_drum::editor
