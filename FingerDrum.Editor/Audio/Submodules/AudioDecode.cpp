#include "Audio/Submodules/AudioDecode.h"
#include <stdexcept>

namespace finger_drum::editor::detail
{
    void AppendPcm(DecodedAudio &audio, const std::span<const float> samples, const PcmConsumer &consume)
    {
        if (!audio.channels || !audio.rate || samples.size() % audio.channels != 0)
            throw std::runtime_error("Audio analysis returned an invalid PCM format.");
        if (samples.empty())
            return;
        const auto count = samples.size() / audio.channels;
        const auto limit = static_cast<std::uint64_t>(audio.rate) * 60 * 60 * 2;
        if (audio.totalFrames > limit || count > limit - audio.totalFrames)
            throw std::runtime_error("Audio exceeds the two-hour editor analysis limit.");
        audio.samples.insert(audio.samples.end(), samples.begin(), samples.end());
        audio.totalFrames += count;
        consume(audio);
    }

}
