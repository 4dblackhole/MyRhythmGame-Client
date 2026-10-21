#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <span>
#include <stop_token>
#include <vector>

struct DecodedAudio
{
    std::uint32_t channels{}, rate{};
    std::vector<float> samples;
    std::size_t firstFrame{}, totalFrames{};
};
using DecodedAudioConsumer = std::function<void(DecodedAudio &)>;

// The analyzer consumes completed windows and removes old samples from this
// rolling buffer. Decoders only append interleaved frames and advance its total.
void AppendPcm(DecodedAudio &audio, std::span<const float> samples, const DecodedAudioConsumer &consume);
