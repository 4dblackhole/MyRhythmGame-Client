#include "TestSupport.h"

namespace finger_drum::tests
{
    void TestEditorAudioAnalysis(const std::filesystem::path &songsRoot)
    {
        const auto catalog = chart::SongCatalog{}.Load(songsRoot);
        Require(!catalog.songs.empty(), "Audio analysis requires the shipped catalog.");
        const auto analysis = editor::AnalyzeAudio(catalog.songs.front().audioPath, {});
        Require(
            analysis.durationSeconds > 1 && analysis.frames.size() > 100 &&
                std::ranges::any_of(analysis.frames, [](const auto &f) { return f.peak > 0.01F; }),
            "The shipped MP3 must decode into real waveform/spectrum data.");
        const auto soundsRoot = songsRoot.parent_path() / "Skins/Default Skin/HitSounds/TaikoMode";
        if (std::filesystem::is_directory(soundsRoot))
            for (const auto *name : {"don.wav", "kat.wav", "bigdon.wav", "bigkat.wav"})
            {
                const auto hit = editor::AnalyzeAudio(soundsRoot / name, {});
                Require(!hit.frames.empty() &&
                            std::ranges::any_of(hit.frames,
                                                [](const auto &frame) {
                                                    return std::ranges::any_of(
                                                        frame.bands,
                                                        [](float value) { return value > .1F; });
                                                }),
                        "Every default hit sound must produce a non-empty real FFT spectrum.");
            }
    }
} // namespace finger_drum::tests
