#include "EditorAudioAnalysis.h"
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <complex>
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
        throw std::runtime_error("Audio analysis failed (Media Foundation): " + std::to_string(result));
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
constexpr std::size_t Window = 512;
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
        const auto root = std::polar(1.0F, -2 * std::numbers::pi_v<float> / static_cast<float>(length));
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
} // namespace
AudioAnalysis AnalyzeAudio(const std::filesystem::path &path, std::stop_token stop)
{
    MediaRuntime runtime;
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
    std::vector<float> mono;
    while (!stop.stop_requested())
    {
        DWORD flags{};
        ComPtr<IMFSample> sample;
        Check(reader->ReadSample(AudioStream, 0, nullptr, &flags, nullptr, &sample));
        if (flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED)
            throw std::runtime_error("Changing PCM formats are not supported by the editor analyzer.");
        if (sample)
        {
            ComPtr<IMFMediaBuffer> buffer;
            Check(sample->ConvertToContiguousBuffer(&buffer));
            DWORD size{};
            Check(buffer->GetCurrentLength(&size));
            const std::size_t count = size / (sizeof(float) * channels), oldSize = mono.size();
            if (oldSize + count > static_cast<std::size_t>(rate) * 60 * 60 * 2)
                throw std::runtime_error("Audio exceeds the two-hour editor analysis limit.");
            mono.resize(oldSize + count);
            BYTE *data{};
            Check(buffer->Lock(&data, nullptr, nullptr));
            const auto *samples = reinterpret_cast<const float *>(data);
            for (std::size_t i = 0; i < count; ++i)
            {
                float sum = 0;
                for (UINT32 c = 0; c < channels; ++c)
                    sum += samples[i * channels + c];
                mono[oldSize + i] = sum / static_cast<float>(channels);
            }
            Check(buffer->Unlock());
        }
        if (flags & MF_SOURCE_READERF_ENDOFSTREAM)
            break;
    }
    if (stop.stop_requested())
        return {};
    AudioAnalysis result;
    const auto hop = std::max<std::size_t>(1, rate / 100);
    result.secondsPerFrame = static_cast<double>(hop) / rate;
    result.durationSeconds = static_cast<double>(mono.size()) / rate;
    result.frames.reserve((mono.size() + hop - 1) / hop);
    for (std::size_t start = 0; start < mono.size() && !stop.stop_requested(); start += hop)
    {
        std::array<std::complex<float>, Window> values{};
        SpectrumFrame frame;
        for (std::size_t i = 0; i < Window; ++i)
        {
            const float sample = start + i < mono.size() ? mono[start + i] : 0;
            values[i] = sample * (.5F - .5F * std::cos(2 * std::numbers::pi_v<float> * static_cast<float>(i) /
                                                       static_cast<float>(Window - 1)));
        }
        for (std::size_t i = start; i < std::min(start + hop, mono.size()); ++i)
            frame.peak = std::max(frame.peak, std::abs(mono[i]));
        FFT(values);
        for (std::size_t band = 0; band < frame.bands.size(); ++band)
        {
            const auto first = static_cast<std::size_t>(std::pow(256.0, static_cast<double>(band) / 24));
            const auto last = std::min<std::size_t>(
                256,
                std::max(first + 1, static_cast<std::size_t>(std::pow(256.0, static_cast<double>(band + 1) / 24))));
            float amplitude = 0;
            for (auto bin = first; bin < last; ++bin)
                amplitude = std::max(amplitude, std::abs(values[bin]) / 128);
            frame.bands[band] = std::clamp((20 * std::log10(std::max(amplitude, 1e-6F)) + 80) / 80, 0.0F, 1.0F);
        }
        result.frames.push_back(frame);
    }
    return result;
}
} // namespace finger_drum::editor
