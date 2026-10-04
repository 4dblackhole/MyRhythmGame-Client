#include "EditorView.h"
#include <algorithm>
#include <iomanip>
#include <sstream>
using namespace editor_ui;

namespace
{
    using finger_drum::editor::AudioAnalysis;
    using finger_drum::editor::SpectrumFrame;
    constexpr std::size_t Columns = 640;
    constexpr v::Rect Waveform{205, 153, 1565, 166};
    constexpr v::Rect MusicSpectrum{205, 375, 1565, 212};
    constexpr v::Rect HitSpectrum{205, 640, 1565, 212};
    constexpr std::array<v::Color, 16> SpectrumColors{{{0, 0, 0, 1},
                                                       {.08F, 0, .16F, 1},
                                                       {.18F, .01F, .32F, 1},
                                                       {.32F, .02F, .44F, 1},
                                                       {.48F, .02F, .46F, 1},
                                                       {.63F, .02F, .38F, 1},
                                                       {.77F, .03F, .27F, 1},
                                                       {.88F, .06F, .15F, 1},
                                                       {.97F, .14F, .05F, 1},
                                                       {1, .27F, .02F, 1},
                                                       {1, .41F, .01F, 1},
                                                       {1, .56F, .02F, 1},
                                                       {1, .71F, .07F, 1},
                                                       {1, .84F, .20F, 1},
                                                       {1, .94F, .50F, 1},
                                                       {1, 1, .86F, 1}}};

    std::wstring Decimal(const double value, const int precision)
    {
        std::wostringstream text;
        text << std::fixed << std::setprecision(precision) << value;
        return text.str();
    }

    void AccumulateColumns(std::vector<SpectrumFrame> &columns, const AudioAnalysis &data, const double offset,
                           const double begin, const double window, const double minimumHz, const double maximumHz)
    {
        if (data.frames.empty() || data.secondsPerFrame <= 0)
            return;
        std::array<std::size_t, finger_drum::editor::SpectrumBandCount> sourceBands{};
        for (std::size_t band = 0; band < sourceBands.size(); ++band)
        {
            const double frequency =
                minimumHz * std::pow(maximumHz / minimumHz, (static_cast<double>(band) + .5) / sourceBands.size());
            sourceBands[band] = frequency > data.maximumFrequencyHz
                                    ? data.frames.front().bands.size()
                                    : static_cast<std::size_t>(
                                          std::clamp(std::log(frequency / data.minimumFrequencyHz) /
                                                         std::log(data.maximumFrequencyHz / data.minimumFrequencyHz) *
                                                         data.frames.front().bands.size(),
                                                     0.0, static_cast<double>(data.frames.front().bands.size() - 1)));
        }
        const int firstColumn = static_cast<int>(std::clamp(std::floor((offset - begin) / window * columns.size()), 0.0,
                                                            static_cast<double>(columns.size())));
        const int lastColumn =
            static_cast<int>(std::clamp(std::ceil((offset + data.durationSeconds - begin) / window * columns.size()),
                                        0.0, static_cast<double>(columns.size())));
        for (int column = firstColumn; column < lastColumn; ++column)
        {
            const double left = std::max(0.0, begin + window * column / columns.size() - offset);
            const double right =
                std::min(data.durationSeconds, begin + window * (column + 1) / columns.size() - offset);
            if (right <= left)
                continue;
            const auto first = std::min(data.frames.size() - 1, static_cast<std::size_t>(left / data.secondsPerFrame));
            const auto last =
                std::min(data.frames.size(), static_cast<std::size_t>(std::ceil(right / data.secondsPerFrame)));
            auto &output = columns[static_cast<std::size_t>(column)];
            // Max-pooling retains short hits at every zoom level and combines
            // overlapping expected hits without hiding the music track.
            for (auto index = first; index < last; ++index)
            {
                const auto &frame = data.frames[index];
                output.minimum = std::min(output.minimum, frame.minimum);
                output.maximum = std::max(output.maximum, frame.maximum);
                for (std::size_t band = 0; band < sourceBands.size(); ++band)
                    if (sourceBands[band] < frame.bands.size())
                        output.bands[band] = std::max(output.bands[band], frame.bands[sourceBands[band]]);
            }
        }
    }
} // namespace

void EditorView::DrawAudioSpectrum(const v::Rect rect, const std::vector<SpectrumFrame> &columns,
                                   const double minimumHz, const double maximumHz)
{
    Box(rect, {0, 0, 0, 1});
    const float columnWidth = rect.width / static_cast<float>(columns.size());
    const float bandHeight = rect.height / finger_drum::editor::SpectrumBandCount;
    for (std::size_t band = 0; band < finger_drum::editor::SpectrumBandCount; ++band)
    {
        const auto colorAt = [&](const std::size_t column) {
            return static_cast<std::size_t>(std::lround(columns[column].bands[band] * (SpectrumColors.size() - 1)));
        };
        // Adjacent cells with the same palette entry share one rectangle.
        for (std::size_t first = 0; first < columns.size();)
        {
            const auto color = colorAt(first);
            auto last = first + 1;
            while (last < columns.size() && colorAt(last) == color)
                ++last;
            if (color != 0)
                Box({rect.x + columnWidth * static_cast<float>(first),
                     rect.y + rect.height - bandHeight * static_cast<float>(band + 1),
                     columnWidth * static_cast<float>(last - first) + .1F, bandHeight + .1F},
                    SpectrumColors[color]);
            first = last;
        }
    }
    for (const double frequency : {20.0, 100.0, 1000.0, 5000.0, maximumHz})
    {
        if (frequency < minimumHz || frequency > maximumHz)
            continue;
        const float y = rect.y + rect.height * static_cast<float>(1 - std::log(frequency / minimumHz) /
                                                                          std::log(maximumHz / minimumHz));
        Box({rect.x, y, rect.width, 1}, {1, 1, 1, .10F});
        Text({125, std::clamp(y - 10, rect.y, rect.y + rect.height - 20), 78, 20},
             frequency >= 1000 ? Decimal(frequency / 1000, 0) + L"k Hz" : Decimal(frequency, 0) + L" Hz", 14);
    }
    for (std::size_t color = 0; color < SpectrumColors.size(); ++color)
        Box({1795, rect.y + rect.height * static_cast<float>(SpectrumColors.size() - 1 - color) / SpectrumColors.size(),
             18, rect.height / SpectrumColors.size() + .1F},
            SpectrumColors[color]);
    for (int tick = 0; tick <= 3; ++tick)
        Text({1820, rect.y + (rect.height - 20) * tick / 3, 70, 20}, std::to_wstring(-30 * tick) + L" dB", 14);
}

void EditorView::DrawAudioWaveform(const std::vector<SpectrumFrame> &columns)
{
    Box(Waveform, {.035F, .045F, .05F, 1});
    const float centerY = Waveform.y + Waveform.height * .5F;
    Box({Waveform.x, centerY, Waveform.width, 1}, {.25F, .35F, .3F, 1});
    for (std::size_t column = 0; column < columns.size(); ++column)
    {
        const float top = centerY - std::clamp(columns[column].maximum, -1.0F, 1.0F) * Waveform.height * .48F;
        const float bottom = centerY - std::clamp(columns[column].minimum, -1.0F, 1.0F) * Waveform.height * .48F;
        if (bottom > top)
            Box({Waveform.x + Waveform.width * static_cast<float>(column) / Columns, top,
                 Waveform.width / Columns + .1F, bottom - top},
                {.24F, .67F, .46F, 1});
    }
    Text({150, Waveform.y, 50, 20}, L"+1", 14);
    Text({150, centerY - 10, 50, 20}, L"0", 14);
    Text({150, Waveform.y + Waveform.height - 20, 50, 20}, L"−1", 14);
}

void EditorView::DrawAudioTimeRuler(const double begin)
{
    // One time ruler and cursor align all three tracks to the editor time.
    double step = std::pow(10.0, std::floor(std::log10(state_.audioWindow / 8)));
    const double ratio = state_.audioWindow / (8 * step);
    step *= ratio > 5 ? 10 : ratio > 2 ? 5 : ratio > 1 ? 2 : 1;
    for (double seconds = std::ceil(begin / step) * step; seconds <= begin + state_.audioWindow; seconds += step)
    {
        const float x = Waveform.x + static_cast<float>((seconds - begin) / state_.audioWindow) * Waveform.width;
        Text({std::clamp(x - 30, Waveform.x, Waveform.x + Waveform.width - 70), 126, 70, 22},
             Decimal(seconds, step < 1 ? 2 : 0) + L" s", 14);
        for (const auto rect : {Waveform, MusicSpectrum, HitSpectrum})
            Box({x, rect.y, 1, rect.height}, {1, 1, 1, .08F});
    }
    for (const auto rect : {Waveform, MusicSpectrum, HitSpectrum})
        Box({rect.x + rect.width * .25F, rect.y, 2, rect.height}, White);
}

void EditorView::DrawAudio()
{
    const auto &text = Texts();
    try
    {
        state_.analysis.CacheAudioMarkers(*state_.editor, *state_.mode);
    }
    catch (const std::exception &error)
    {
        state_.analysis.ClearMarkers(state_.editor->Revision());
        state_.status = error.what();
    }

    // Pool each source into the same visible time and frequency grid.
    const double begin = state_.timeMs / 1000.0 - state_.audioWindow * .25;
    double minimumHz = 20, maximumHz = 20'000, longestSound = 0;
    const auto &sounds = state_.analysis.Data().sounds;
    if (!sounds.empty())
    {
        maximumHz = 0;
        for (const auto &[id, sound] : sounds)
        {
            maximumHz = std::max(maximumHz, sound.maximumFrequencyHz);
            minimumHz = std::min(minimumHz, sound.minimumFrequencyHz);
            if (id != "Music")
                longestSound = std::max(longestSound, sound.durationSeconds);
        }
        maximumHz = std::max(maximumHz, minimumHz * 2);
    }
    std::vector<SpectrumFrame> musicColumns(Columns), hitColumns(Columns);
    if (const auto music = sounds.find("Music"); music != sounds.end())
        AccumulateColumns(musicColumns, music->second, 0, begin, state_.audioWindow, minimumHz, maximumHz);
    const auto &markers = state_.analysis.Markers();
    for (auto marker = std::ranges::lower_bound(markers, begin - longestSound, {}, &AudioMarker::seconds);
         marker != markers.end() && marker->seconds <= begin + state_.audioWindow; ++marker)
        if (const auto sound = sounds.find(marker->sound); sound != sounds.end())
            AccumulateColumns(hitColumns, sound->second, marker->seconds, begin, state_.audioWindow, minimumHz,
                              maximumHz);

    // Draw distinct tracks before their shared ruler/cursor and controls.
    Box({112, 76, 1784, 797}, Paper, 8);
    Text({132, 90, 980, 28}, std::wstring(text.waveform), 22);
    Text({132, 340, 1500, 28}, std::wstring(text.musicSpectrum), 22);
    Text({132, 606, 1500, 28}, std::wstring(text.hitSoundSpectrum), 22);
    DrawAudioWaveform(musicColumns);
    DrawAudioSpectrum(MusicSpectrum, musicColumns, minimumHz, maximumHz);
    DrawAudioSpectrum(HitSpectrum, hitColumns, minimumHz, maximumHz);
    DrawAudioTimeRuler(begin);
    Button({1620, 85, 54, 32}, L"+", [this] {
        state_.audioWindow = std::max(.25, state_.audioWindow / 2);
        state_.rebuild = true;
    });
    Button({1684, 85, 54, 32}, L"−", [this] {
        state_.audioWindow = std::min(120.0, state_.audioWindow * 2);
        state_.rebuild = true;
    });
    Text({1748, 88, 140, 24}, Decimal(state_.audioWindow, 2) + L" s", 16);
    if (state_.analysis.Running())
        Text({570, 90, 700, 28}, std::wstring(text.analyzingAudio), 18, Blue);
    DrawTimeline();
}
