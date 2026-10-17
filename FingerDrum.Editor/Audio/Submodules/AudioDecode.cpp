#include "Audio/Submodules/AudioDecode.h"
#include <Windows.h>
#include <array>
#include <fstream>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <stdexcept>
#include <string>
#include <wrl/client.h>

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

    namespace
    {
        void Check(const HRESULT result)
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
        struct LockedBuffer
        {
            explicit LockedBuffer(IMFMediaBuffer &source) : buffer(source)
            {
                Check(buffer.Lock(&data, nullptr, nullptr));
            }
            ~LockedBuffer()
            {
                if (locked)
                    static_cast<void>(buffer.Unlock());
            }
            void Unlock()
            {
                Check(buffer.Unlock());
                locked = false;
            }
            LockedBuffer(const LockedBuffer &) = delete;
            LockedBuffer &operator=(const LockedBuffer &) = delete;
            IMFMediaBuffer &buffer;
            BYTE *data{};
            bool locked{true};
        };

        DecodedAudio DecodeMediaFoundation(const std::filesystem::path &path, const std::stop_token stop,
                                           const PcmConsumer &consume)
        {
            MediaRuntime runtime;
            using Microsoft::WRL::ComPtr;
            constexpr DWORD audioStream = static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM);
            constexpr DWORD allStreams = static_cast<DWORD>(MF_SOURCE_READER_ALL_STREAMS);
            ComPtr<IMFSourceReader> reader;
            Check(MFCreateSourceReaderFromURL(path.c_str(), nullptr, &reader));
            Check(reader->SetStreamSelection(allStreams, FALSE));
            Check(reader->SetStreamSelection(audioStream, TRUE));
            ComPtr<IMFMediaType> type;
            Check(MFCreateMediaType(&type));
            Check(type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio));
            Check(type->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_Float));
            Check(reader->SetCurrentMediaType(audioStream, nullptr, type.Get()));
            type.Reset();
            Check(reader->GetCurrentMediaType(audioStream, &type));
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
                Check(reader->ReadSample(audioStream, 0, nullptr, &flags, nullptr, &sample));
                if (flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED)
                    throw std::runtime_error("Changing PCM formats are not supported by the editor analyzer.");
                if (sample)
                {
                    ComPtr<IMFMediaBuffer> buffer;
                    Check(sample->ConvertToContiguousBuffer(&buffer));
                    DWORD size{};
                    Check(buffer->GetCurrentLength(&size));
                    if (size % sizeof(float) != 0)
                        throw std::runtime_error("Audio analysis returned an incomplete PCM sample.");
                    LockedBuffer locked(*buffer.Get());
                    AppendPcm(decoded, {reinterpret_cast<const float *>(locked.data), size / sizeof(float)}, consume);
                    locked.Unlock();
                }
                if (flags & MF_SOURCE_READERF_ENDOFSTREAM)
                    break;
            }
            return decoded;
        }
    } // namespace

    DecodedAudio DecodeAudio(const std::filesystem::path &path, const std::stop_token stop, const PcmConsumer &consume)
    {
        std::array<char, 4> magic{};
        {
            std::ifstream input(path, std::ios::binary);
            if (!input)
                throw std::runtime_error("Could not open the audio file for analysis.");
            input.read(magic.data(), static_cast<std::streamsize>(magic.size()));
        }
        // Detect the container itself, including uppercase/renamed extensions.
        if (magic == std::array{'O', 'g', 'g', 'S'})
            return DecodeVorbis(path, stop, consume);
        return DecodeMediaFoundation(path, stop, consume);
    }
} // namespace finger_drum::editor::detail
