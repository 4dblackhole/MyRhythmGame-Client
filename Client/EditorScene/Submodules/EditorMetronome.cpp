#include "EditorMetronome.h"
#include "App/AssetPaths.h"
#include "EditorTime.h"
#include "Timing/MetronomeBeats.h"
#include <algorithm>

namespace
{
    constexpr const char *BeatId = "Editor.Metronome.Beat";
    constexpr const char *AccentId = "Editor.Metronome.Accent";
    std::vector<EditorMetronome::Beat> CompileBeats(const MusicalTimeline &timeline,
                                                    double endSeconds)
    {

        const auto end = Microseconds(endSeconds * 1000);
        const auto last = MeasureNearTime(timeline, endSeconds * 1000);
        std::vector<EditorMetronome::Beat> beats;
        for (const auto &beat : CompileQuarterBeats(timeline, last, end))
            beats.push_back({beat.time.count() / 1'000'000.0, beat.accent});
        return beats;
    }
} // namespace

void EditorMetronome::Prepare(const IEditorDocument &document, double endSeconds,
                              const std::map<std::string, std::filesystem::path> &files, std::uint64_t filesRevision,
                              const std::vector<AudioMarker> &markers)
{
    if (filesRevision_ != filesRevision)
    {
        auto prepared = files;
        prepared[BeatId] = EditorSound(L"MetronomeBeat.wav");
        prepared[AccentId] = EditorSound(L"MetronomeAccent.wav");
        files_ = std::move(prepared);
        filesRevision_ = filesRevision;
    }
    const bool gridChanged =
        document_ != &document || documentRevision_ != document.Revision() || endSeconds_ != endSeconds;
    if (!gridChanged && preparedEnabled_ == enabled_)
        return;
    // Publish complete caches only after compilation succeeds. No tempo integration per frame.
    auto beats = gridChanged ? CompileBeats(document.Timeline(), endSeconds) : beats_;
    auto cues = markers;
    if (enabled_)
        for (const auto &beat : beats)
            cues.push_back({beat.seconds, beat.accent ? AccentId : BeatId});
    std::ranges::stable_sort(cues, {}, &AudioMarker::seconds);
    beats_ = std::move(beats);
    cues_ = std::move(cues);
    document_ = &document;
    documentRevision_ = document.Revision();
    endSeconds_ = endSeconds;
    preparedEnabled_ = enabled_;
    ++cueRevision_;
}
