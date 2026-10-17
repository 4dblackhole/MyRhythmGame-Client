#include "Audio/Submodules/AudioDecode.h"
#define STB_VORBIS_HEADER_ONLY
#include <cstdio>
#include <limits>
#include <memory>
#include <stb_vorbis.c>
#include <stdexcept>
#include <string>

namespace finger_drum::editor::detail
{
    DecodedAudio DecodeVorbis(const std::filesystem::path &path, const std::stop_token stop, const PcmConsumer &consume)
    {
        std::FILE *opened{};
        if (_wfopen_s(&opened, path.c_str(), L"rb") != 0 || !opened)
            throw std::runtime_error("Could not open the Ogg Vorbis file for analysis.");
        const std::unique_ptr<std::FILE, decltype(&std::fclose)> file(opened, &std::fclose);
        int error{};
        const std::unique_ptr<stb_vorbis, decltype(&stb_vorbis_close)> decoder(
            stb_vorbis_open_file(file.get(), 0, &error, nullptr), &stb_vorbis_close);
        if (!decoder)
            throw std::runtime_error("Ogg Vorbis analysis could not open the stream (decoder error " +
                                     std::to_string(error) + ").");
        const auto info = stb_vorbis_get_info(decoder.get());
        constexpr std::size_t framesPerChunk = 4096;
        if (info.channels <= 0 ||
            static_cast<std::size_t>(info.channels) >
                static_cast<std::size_t>(std::numeric_limits<int>::max()) / framesPerChunk ||
            !info.sample_rate)
            throw std::runtime_error("Ogg Vorbis analysis returned an invalid PCM format.");

        DecodedAudio decoded{static_cast<std::uint32_t>(info.channels), info.sample_rate, {}};
        consume(decoded);
        std::vector<float> chunk(framesPerChunk * decoded.channels);
        while (!stop.stop_requested())
        {
            const int frames = stb_vorbis_get_samples_float_interleaved(decoder.get(), info.channels, chunk.data(),
                                                                        static_cast<int>(chunk.size()));
            error = stb_vorbis_get_error(decoder.get());
            if (error != VORBIS__no_error)
                throw std::runtime_error("Ogg Vorbis analysis failed while decoding (decoder error " +
                                         std::to_string(error) + ").");
            if (frames == 0)
                break;
            if (frames < 0 || static_cast<std::size_t>(frames) > framesPerChunk)
                throw std::runtime_error("Ogg Vorbis analysis returned an invalid PCM frame count.");
            AppendPcm(decoded, std::span{chunk}.first(static_cast<std::size_t>(frames) * decoded.channels), consume);
        }
        return decoded;
    }
} // namespace finger_drum::editor::detail
