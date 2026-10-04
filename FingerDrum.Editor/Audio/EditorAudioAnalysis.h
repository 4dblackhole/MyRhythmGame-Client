#pragma once
#include <array>
#include <cstddef>
#include <filesystem>
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
    // Decode and FFT on a worker; no renderer, game clock or FMOD ownership.
    AudioAnalysis AnalyzeAudio(const std::filesystem::path &path, std::stop_token stop);
} // namespace finger_drum::editor
