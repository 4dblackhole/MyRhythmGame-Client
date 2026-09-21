#pragma once
#include <array>
#include <filesystem>
#include <stop_token>
#include <vector>

namespace finger_drum::editor
{
    struct SpectrumFrame
    {
        float peak{};
        std::array<float, 24> bands{};
    };
    struct AudioAnalysis
    {
        double secondsPerFrame{};
        double durationSeconds{};
        std::vector<SpectrumFrame> frames;
    };
    // Decode and FFT on a worker; no renderer, game clock or FMOD ownership.
    AudioAnalysis AnalyzeAudio(const std::filesystem::path &path, std::stop_token stop);
} // namespace finger_drum::editor
