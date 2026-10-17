#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <span>
#include <stop_token>
#include <vector>

namespace finger_drum::editor::detail
{
    struct DecodedAudio
    {
        std::uint32_t channels{}, rate{};
        std::vector<float> samples;
        std::size_t firstFrame{}, totalFrames{};
    };
    using PcmConsumer = std::function<void(DecodedAudio &)>;

    // The analyzer consumes completed windows and removes old samples from this
    // rolling buffer. Decoders only append interleaved frames and advance its total.
    void AppendPcm(DecodedAudio &audio, std::span<const float> samples, const PcmConsumer &consume);
    DecodedAudio DecodeAudio(const std::filesystem::path &path, std::stop_token stop, const PcmConsumer &consume);
    DecodedAudio DecodeVorbis(const std::filesystem::path &path, std::stop_token stop, const PcmConsumer &consume);
} // namespace finger_drum::editor::detail
