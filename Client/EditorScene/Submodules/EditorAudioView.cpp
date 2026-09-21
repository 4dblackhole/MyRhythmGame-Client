#include "EditorView.h"
#include <algorithm>
#include <sstream>
using namespace editor_ui;

void EditorView::DrawAudio()
{
    const auto &text = Texts();
    try
    {
        state_.analysis.CacheAudioMarkers(*state_.editor);
    }
    catch (const std::exception &error)
    {
        state_.analysis.ClearMarkers(state_.editor->Revision());
        state_.status = error.what();
    }
    Box({112, 806, 1784, 250}, Paper);
    Text({132, 813, 400, 28}, std::wstring(text.audioAnalysis), 22);
    Box({205, 845, 1399, 60}, {.07F, .15F, .20F, 1}, 4);
    Box({205, 915, 1399, 68}, {.07F, .15F, .20F, 1}, 4);
    const double begin = state_.timeMs / 1000.0 - state_.audioWindow * .25;
    const auto drawSpectrum = [&](const finger_drum::editor::AudioAnalysis &data, double offset,
                                  bool hit) {
        if (data.frames.empty() || data.secondsPerFrame <= 0)
            return;
        constexpr int columns = 350;
        for (int col = 0; col < columns; ++col)
        {
            const double left = begin + state_.audioWindow * col / columns - offset;
            const double right = left + state_.audioWindow / columns;
            if (right <= 0 || left >= data.durationSeconds)
                continue;
            const double seconds = std::max(0.0, left);
            const auto index = std::min(data.frames.size() - 1,
                                        static_cast<std::size_t>(seconds / data.secondsPerFrame));
            auto frame = data.frames[index];
            const auto lastIndex =
                std::min(data.frames.size(),
                         static_cast<std::size_t>(std::ceil(std::min(data.durationSeconds, right) /
                                                            data.secondsPerFrame)));
            // Max-pool rather than skip short transients when zoomed out.
            for (auto i = index + 1; i < lastIndex; ++i)
            {
                frame.peak = std::max(frame.peak, data.frames[i].peak);
                for (std::size_t band = 0; band < frame.bands.size(); ++band)
                    frame.bands[band] = std::max(frame.bands[band], data.frames[i].bands[band]);
            }
            frame.peak = std::min(1.0F, frame.peak);
            const float x = 205 + 1399.0F * col / columns;
            if (!hit)
                Box({x, 875 - frame.peak * 27, 3, std::max(1.0F, frame.peak * 54)}, Kat);
            for (std::size_t band = 0; band < frame.bands.size(); ++band)
            {
                const float level = frame.bands[band];
                if (level < .06F)
                    continue;
                Box({x, 980 - static_cast<float>(band) * 2.65F, 4, 2.7F},
                    hit ? v::Color{1, .83F, .3F, level * .85F}
                        : v::Color{.10F + .3F * level, .2F + .6F * level, .3F + .6F * level, 1});
            }
        }
    };
    if (const auto music = state_.analysis.Data().sounds.find("Music");
        music != state_.analysis.Data().sounds.end())
        drawSpectrum(music->second, 0, false);
    double longestSound = 0;
    for (const auto &[id, sound] : state_.analysis.Data().sounds)
        if (id != "Music")
            longestSound = std::max(longestSound, sound.durationSeconds);
    const auto firstMarker = std::ranges::lower_bound(
        state_.analysis.Markers(), begin - longestSound, {}, &AudioMarker::seconds);
    for (auto marker = firstMarker;
         marker != state_.analysis.Markers().end() && marker->seconds <= begin + state_.audioWindow;
         ++marker)
    {
        const auto seconds = marker->seconds;
        if (const auto found = state_.analysis.Data().sounds.find(marker->sound);
            found != state_.analysis.Data().sounds.end())
            drawSpectrum(found->second, seconds, true);
        const float x = 205 + static_cast<float>((seconds - begin) / state_.audioWindow) * 1399;
        if (x >= 205 && x <= 1604)
            Box({x, 915, 2, 68}, Gold);
    }
    Box({205 + 1399 * .25F, 843, 2, 140}, White);
    Button({132, 855, 54, 42}, L"+", [this] {
        state_.audioWindow = std::max(.25, state_.audioWindow / 2);
        state_.rebuild = true;
    });
    Button({132, 915, 54, 42}, L"−", [this] {
        state_.audioWindow = std::min(120.0, state_.audioWindow * 2);
        state_.rebuild = true;
    });
    Button({1620, 845, 260, 42}, std::wstring(text.currentTime), [this] {
        if (auto s = EditText(std::wstring(Texts().currentTime), std::to_string(state_.timeMs),
                              Texts()))
        {
            state_.timeMs = Number(*s);
            state_.rebuild = true;
        }
    });
    Text({1620, 903, 260, 40}, Wide(std::to_string(state_.timeMs)), 20);
    Text({205, 995, 1500, 38}, std::wstring(text.audioHelp), 18);
    if (state_.analysis.Running())
        Text({620, 813, 850, 28}, std::wstring(text.analyzingAudio), 18, Blue);
}
