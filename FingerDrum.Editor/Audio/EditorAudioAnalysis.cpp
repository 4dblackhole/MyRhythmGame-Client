#include "Audio/EditorAudioAnalysis.h"
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <complex>
#include <functional>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <numbers>
#include <stdexcept>
#include <wrl/client.h>

namespace finger_drum::editor
{
    namespace
    {
        void Check(HRESULT result)
        {
            if (FAILED(result))
                throw std::runtime_error("Audio analysis failed (Media Foundation): " +
                                         std::to_string(result));
        }
        struct MediaRuntime
        {
            MediaRuntime()
            {
                Check(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
                const auto result = MFStartup(MF_VERSION);
                if (FAILED(result))
                {
                    CoUninitialize();
                    Check(result);
                }
            }
            ~MediaRuntime()
            {
                MFShutdown();
                CoUninitialize();
            }
        };
        constexpr std::size_t Window = 2048;
        constexpr DWORD AudioStream = static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM);
        constexpr DWORD AllStreams = static_cast<DWORD>(MF_SOURCE_READER_ALL_STREAMS);
        void FFT(std::array<std::complex<float>, Window> &values)
        {
            for (std::size_t i = 1, j = 0; i < Window; ++i)
            {
                std::size_t bit = Window >> 1;
                for (; j & bit; bit >>= 1)
                    j ^= bit;
                j ^= bit;
                if (i < j)
                    std::swap(values[i], values[j]);
            }
            for (std::size_t length = 2; length <= Window; length <<= 1)
            {
                const auto root =
                    std::polar(1.0F, -2 * std::numbers::pi_v<float> / static_cast<float>(length));
                for (std::size_t i = 0; i < Window; i += length)
                {
                    std::complex<float> w{1, 0};
                    for (std::size_t j = 0; j < length / 2; ++j)
                    {
                        const auto a = values[i + j], b = values[i + j + length / 2] * w;
                        values[i + j] = a + b;
                        values[i + j + length / 2] = a - b;
                        w *= root;
                    }
                }
            }
        }
        struct DecodedAudio
        {
            UINT32 channels{}, rate{};
            std::vector<float> samples;
            std::size_t firstFrame{}, totalFrames{};
        };

        DecodedAudio DecodeAudio(const std::filesystem::path &path, const std::stop_token &stop,
                                 const std::function<void(DecodedAudio &)> &consume)
        {
            using Microsoft::WRL::ComPtr;
            ComPtr<IMFSourceReader> reader;
            Check(MFCreateSourceReaderFromURL(path.c_str(), nullptr, &reader));
            Check(reader->SetStreamSelection(AllStreams, FALSE));
            Check(reader->SetStreamSelection(AudioStream, TRUE));
            ComPtr<IMFMediaType> type;
            Check(MFCreateMediaType(&type));
            Check(type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio));
            Check(type->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_Float));
            Check(reader->SetCurrentMediaType(AudioStream, nullptr, type.Get()));
            type.Reset();
            Check(reader->GetCurrentMediaType(AudioStream, &type));
            const auto channels = MFGetAttributeUINT32(type.Get(), MF_MT_AUDIO_NUM_CHANNELS, 0);
            const auto rate = MFGetAttributeUINT32(type.Get(), MF_MT_AUDIO_SAMPLES_PER_SECOND, 0);
            if (!channels || !rate)
                throw std::runtime_error("Audio analysis returned an invalid PCM format.");
            DecodedAudio decoded{channels, rate, {}};
            consume(decoded);
            while (!stop.stop_requested())
            {
                DWORD flags{};
                ComPtr<IMFSample> sample;
                Check(reader->ReadSample(AudioStream, 0, nullptr, &flags, nullptr, &sample));
                if (flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED)
                    throw std::runtime_error(
                        "Changing PCM formats are not supported by the editor analyzer.");
                if (sample)
                {
                    ComPtr<IMFMediaBuffer> buffer;
                    Check(sample->ConvertToContiguousBuffer(&buffer));
                    DWORD size{};
                    Check(buffer->GetCurrentLength(&size));
                    const std::size_t count = size / (sizeof(float) * channels);
                    if (decoded.totalFrames + count > static_cast<std::size_t>(rate) * 60 * 60 * 2)
                        throw std::runtime_error(
                            "Audio exceeds the two-hour editor analysis limit.");
                    BYTE *data{};
                    Check(buffer->Lock(&data, nullptr, nullptr));
                    const auto *samples = reinterpret_cast<const float *>(data);
                    decoded.samples.insert(decoded.samples.end(), samples,
                                           samples + count * channels);
                    Check(buffer->Unlock());
                    decoded.totalFrames += count;
                    consume(decoded);
                }
                if (flags & MF_SOURCE_READERF_ENDOFSTREAM)
                    break;
            }
            return decoded;
        }

        SpectrumFrame AnalyzeFrame(const DecodedAudio &audio, const std::size_t start,
                                   const std::size_t hop, const std::array<float, Window> &hann,
                                   const float windowSum,
                                   const std::array<std::size_t, SpectrumBandCount + 1> &bins)
        {
            SpectrumFrame frame;
            const auto sampleCount = audio.samples.size() / audio.channels;
            frame.minimum = 1;
            frame.maximum = -1;
            for (std::size_t sample = start; sample < std::min(start + hop, sampleCount); ++sample)
                for (UINT32 channel = 0; channel < audio.channels; ++channel)
                {
                    const float value = audio.samples[sample * audio.channels + channel];
                    frame.minimum = std::min(frame.minimum, value);
                    frame.maximum = std::max(frame.maximum, value);
                    frame.peak = std::max(frame.peak, std::abs(value));
                }

            // Average channel power, rather than summing their amplitudes:
            // opposite stereo phases must not cancel the visible spectrum.
            std::array<float, Window / 2 + 1> power{};
            for (UINT32 channel = 0; channel < audio.channels; ++channel)
            {
                std::array<std::complex<float>, Window> values{};
                for (std::size_t i = 0; i < Window; ++i)
                {
                    const auto sample = static_cast<std::int64_t>(start) +
                                        static_cast<std::int64_t>(i) -
                                        static_cast<std::int64_t>(Window / 2);
                    if (sample >= 0 && static_cast<std::size_t>(sample) < sampleCount)
                        values[i] =
                            audio.samples[static_cast<std::size_t>(sample) * audio.channels +
                                          channel] *
                            hann[i];
                }
                FFT(values);
                for (std::size_t bin = 1; bin < power.size(); ++bin)
                    power[bin] += std::norm(values[bin]);
            }
            const float normalization =
                4.0F / (windowSum * windowSum * static_cast<float>(audio.channels));
            for (std::size_t band = 0; band < frame.bands.size(); ++band)
            {
                float maximumPower = 0;
                const auto last = std::min(power.size(), std::max(bins[band] + 1, bins[band + 1]));
                for (auto bin = bins[band]; bin < last; ++bin)
                    maximumPower = std::max(maximumPower, power[bin]);
                const float db = 10 * std::log10(std::max(maximumPower * normalization, 1e-12F));
                frame.bands[band] =
                    std::clamp((db - SpectrumFloorDb) / -SpectrumFloorDb, 0.0F, 1.0F);
            }
            return frame;
        }
    } // namespace

    AudioAnalysis AnalyzeAudio(const std::filesystem::path &path, const std::stop_token stop)
    {
        MediaRuntime runtime;
        AudioAnalysis result;
        std::array<float, Window> hann{};
        float windowSum = 0;
        for (std::size_t i = 0; i < Window; ++i)
        {
            hann[i] = .5F - .5F * std::cos(2 * std::numbers::pi_v<float> * static_cast<float>(i) /
                                           static_cast<float>(Window - 1));
            windowSum += hann[i];
        }
        std::array<std::size_t, SpectrumBandCount + 1> bins{};
        std::size_t nextFrame = 0, hop = 1;
        const auto initialize = [&](const DecodedAudio &audio) {
            // Metadata follows each source's rate; only the FFT window is retained.
            result.sampleRate = audio.rate;
            result.maximumFrequencyHz = std::min(20'000.0, audio.rate * .5);
            result.minimumFrequencyHz = std::min(20.0, result.maximumFrequencyHz * .5);
            hop = std::max<std::size_t>(1, audio.rate / 100);
            result.secondsPerFrame = static_cast<double>(hop) / audio.rate;
            for (std::size_t band = 0; band < bins.size(); ++band)
            {
                const double frequency = result.minimumFrequencyHz *
                    std::pow(result.maximumFrequencyHz / result.minimumFrequencyHz,
                             static_cast<double>(band) / SpectrumBandCount);
                bins[band] = std::clamp(static_cast<std::size_t>(frequency * Window / audio.rate),
                                       std::size_t{1}, Window / 2);
            }
        };
        const auto consume = [&](DecodedAudio &audio, bool end) {
            if (!result.sampleRate)
                initialize(audio);
            while (nextFrame < audio.totalFrames && !stop.stop_requested() &&
                   (end || nextFrame + std::max(Window / 2, hop) <= audio.totalFrames))
            {
                result.frames.push_back(AnalyzeFrame(audio, nextFrame - audio.firstFrame, hop, hann, windowSum, bins));
                nextFrame += hop;
            }
            // Keep the preceding half-window for the next centered FFT.
            const auto keepFrom = std::min(audio.totalFrames,
                nextFrame > Window / 2 ? nextFrame - Window / 2 : 0);
            if (keepFrom > audio.firstFrame)
            {
                audio.samples.erase(audio.samples.begin(), audio.samples.begin() +
                    static_cast<std::ptrdiff_t>((keepFrom - audio.firstFrame) * audio.channels));
                audio.firstFrame = keepFrom;
            }
        };
        auto audio = DecodeAudio(path, stop, [&](DecodedAudio &decoded) { consume(decoded, false); });
        if (stop.stop_requested())
            return {};
        consume(audio, true);
        result.durationSeconds = static_cast<double>(audio.totalFrames) / audio.rate;
        return result;
    }
} // namespace finger_drum::editor
