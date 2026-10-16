#include "EditorMetronomeTests.h"
#include "Editing/ChartEditor.h"
#include "EditorScene/Submodules/EditorMetronome.h"
#include <algorithm>
#include <stdexcept>

void TestEditorMetronome()
{
    namespace chart = finger_drum::chart;
    const auto check = [](bool ok, const char *message)
    {
        if (!ok)
            throw std::runtime_error(message);
    };
    chart::PatternDocument pattern;
    pattern.patternOffsetMilliseconds = 100;
    pattern.timing = {{{0, {1, 2}}, chart::TimingDirectiveType::Bpm, 240},
                      {{0, {3, 4}}, chart::TimingDirectiveType::DelayMilliseconds, 250}};
    chart::ChartEditor document(pattern, {}, finger_drum::rhythm::RhythmDuration{100'000});
    const std::map<std::string, std::filesystem::path> files{{"Music", "music"}, {"Don", "don"}};
    const std::vector<AudioMarker> hits{{.25, "Don"}};
    EditorMetronome metronome;
    metronome.Prepare(document, 4, files, 1, hits);
    check(metronome.Cues().size() == 1 && metronome.Files().size() == 4,
          "Disabled metronome adds sources, but no playback or analysis hits.");
    check(metronome.Beats().size() >= 5 && metronome.Beats()[0].seconds == .2 && metronome.Beats()[1].seconds == .7 &&
              metronome.Beats()[2].seconds == 1.2 && metronome.Beats()[3].seconds == 1.7 &&
              metronome.Beats()[4].seconds == 1.95 && metronome.Beats()[0].accent && !metronome.Beats()[1].accent &&
              metronome.Beats()[4].accent,
          "Click timing must reflect BPM, delay, chart/global offsets and measure accents.");
    const auto revision = metronome.CueRevision();
    const auto *cached = metronome.Beats().data();
    metronome.Prepare(document, 4, files, 1, hits);
    check(metronome.CueRevision() == revision && cached == metronome.Beats().data(), "Reuse unchanged beat cache.");
    metronome.SetEnabled(true);
    metronome.Prepare(document, 4, files, 1, hits);
    check(metronome.CueRevision() != revision && metronome.Cues().size() == metronome.Beats().size() + 1 &&
              hits.size() == 1,
          "Toggle adds playback cues without altering analysis hits.");
    pattern.timing = {{{0, {3, 4}}, chart::TimingDirectiveType::DelayMilliseconds, -750}};
    document.Replace(pattern, {});
    metronome.Prepare(document, 4, files, 1, hits);
    check(metronome.Beats()[0].seconds == .2 && metronome.Beats()[1].seconds == .7 &&
              metronome.Beats()[2].seconds == .95 && metronome.Beats()[3].seconds == 1.45,
          "Negative delay must remove invaded ticks and advance surviving beats.");
    document.SetMeasureLength(0, chart::Rational{3, 4});
    metronome.Prepare(document, 4, files, 1, hits);
    check(std::ranges::any_of(metronome.Beats(), [](const auto &beat) { return beat.accent && beat.seconds > .2; }),
          "Measure edits must rebuild accents.");
}
