#include "EditorView.h"
#include <algorithm>
#include <iomanip>
#include <sstream>
using namespace editor_ui;

namespace
{

    constexpr std::size_t Columns = 640;
    constexpr std::size_t TileColumns = 128;
    constexpr std::array<Color, 16> SpectrumColors{{{0, 0, 0, 1},
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

    using SourceBands = std::array<std::size_t, SpectrumBandCount>;
    SourceBands MapFrequencyBands(const AudioAnalysis &data, double minimumHz, double maximumHz)
    {
        SourceBands sourceBands{};
        if (data.frames.empty())
            return sourceBands;
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
        return sourceBands;
    }

    void AccumulateColumns(std::vector<SpectrumFrame> &columns, const AudioAnalysis &data, const double offset,
                           const double begin, const double window, const SourceBands &sourceBands)
    {
        if (data.frames.empty() || data.secondsPerFrame <= 0)
            return;
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

    std::vector<SpectrumFrame> BuildSpectrumColumns(const AnalysisBatch &data, const std::vector<AudioMarker> &markers,
                                                    std::size_t track, double start, double tileSeconds,
                                                    double minimumHz, double maximumHz, double longestSound)
    {
        const auto &sounds = data.sounds;
        std::vector<SpectrumFrame> columns(TileColumns);
        if (track == 0)
        {
            if (const auto music = sounds.find("Music"); music != sounds.end())
                AccumulateColumns(columns, music->second, 0, start, tileSeconds,
                                  MapFrequencyBands(music->second, minimumHz, maximumHz));
        }
        else
        {
            std::map<std::string, SourceBands> bands;
            for (auto marker = std::ranges::lower_bound(markers, start - longestSound, {}, &AudioMarker::seconds);
                 marker != markers.end() && marker->seconds <= start + tileSeconds; ++marker)
                if (const auto sound = sounds.find(marker->sound); sound != sounds.end())
                {
                    auto [mapping, inserted] = bands.try_emplace(marker->sound);
                    if (inserted)
                        mapping->second = MapFrequencyBands(sound->second, minimumHz, maximumHz);
                    AccumulateColumns(columns, sound->second, marker->seconds, start, tileSeconds, mapping->second);
                }
        }
        return columns;
    }

    void BuildSpectrumGeometry(EditorVisual &geometry, const std::vector<SpectrumFrame> &columns, const Rect rect)
    {
        const float columnWidth = rect.width / Columns;
        const float bandHeight = rect.height / SpectrumBandCount;
        for (std::size_t band = 0; band < SpectrumBandCount; ++band)
            for (std::size_t column = 0; column < columns.size();)
            {
                const auto colorAt = [&](std::size_t c)
                { return static_cast<std::size_t>(std::lround(columns[c].bands[band] * (SpectrumColors.size() - 1))); };
                const auto color = colorAt(column);
                auto end = column + 1;
                while (end < columns.size() && colorAt(end) == color)
                    ++end;
                if (color != 0)
                    geometry.AddRectangle({static_cast<float>(column) * columnWidth,
                                           static_cast<float>(band) * bandHeight,
                                           static_cast<float>(end - column) * columnWidth + .1F, bandHeight + .1F},
                                          SpectrumColors[color], 0);
                column = end;
            }
        geometry.Finish();
    }
} // namespace

void EditorView::PrepareAudioTrack(std::size_t track, Rect rect, std::int64_t first, std::int64_t last,
                                   bool invalidate)
{
    if (!audioRoots_[track])
    {
        auto &root = node->CreateChild(track == 0 ? "Audio.MusicSpectrum" : "Audio.HitSpectrum");
        root.SetBounds({rect.x - 960, 540 - rect.y - rect.height, rect.width, rect.height});
        root.SetClipRect({0, 0, rect.width, rect.height});
        audioRoots_[track] = &root;
        auto &overlay = root.CreateChild("Ruler");
        overlay.SetZIndex(1);
        audioOverlays_[track] = &overlay.AddComponent<EditorVisual>();
    }
    audioRoots_[track]->SetVisible(true);
    audioOverlays_[track]->Clear();
    auto &tiles = audioTiles_[track];
    for (auto it = tiles.begin(); it != tiles.end();)
    {
        // Retain one neighbouring tile on each side for short back-seeks.
        if (invalidate || it->first < first - 1 || it->first > last + 1)
        {
            static_cast<void>(audioRoots_[track]->RemoveChild(it->second.node->Id()));
            it = tiles.erase(it);
        }
        else
        {
            it->second.node->SetVisible(false);
            ++it;
        }
    }
}

void EditorView::UpdateWaveformColumns(double begin, double columnSeconds)
{
    // The waveform only needs min/max, not a copy of all 96 FFT bands.
    musicColumns_.resize(Columns);
    for (std::size_t column = 0; column < Columns; ++column)
    {
        const auto absoluteColumn =
            static_cast<std::int64_t>(std::floor(begin / columnSeconds)) + static_cast<std::int64_t>(column);
        const auto tileIndex = static_cast<std::int64_t>(std::floor(static_cast<double>(absoluteColumn) / TileColumns));
        const auto local =
            static_cast<std::size_t>(absoluteColumn - tileIndex * static_cast<std::int64_t>(TileColumns));
        const auto &source = audioTiles_[0].at(tileIndex).columns[local];
        musicColumns_[column].minimum = source.minimum;
        musicColumns_[column].maximum = source.maximum;
    }
}

void EditorView::UpdateAudioTiles(double begin, double minimumHz, double maximumHz, double longestSound)
{
    const double columnSeconds = state_.audioWindow / Columns;
    const double tileSeconds = columnSeconds * TileColumns;
    const auto first = static_cast<std::int64_t>(std::floor(begin / tileSeconds));
    const auto last = static_cast<std::int64_t>(std::floor((begin + state_.audioWindow) / tileSeconds));
    const AudioColumnsKey key{state_.audioWindow, minimumHz, maximumHz, state_.Analysis().Revision()};
    const bool invalidate = audioColumnsKey_ != key || audioColumnsDocument_ != &state_.Document() ||
                            audioColumnsDocumentRevision_ != state_.Document().Revision();
    const auto &markers = state_.Analysis().Markers();
    const std::array rectangles{MusicSpectrum, HitSpectrum};
    for (std::size_t track = 0; track < rectangles.size(); ++track)
    {
        const auto rect = rectangles[track];
        PrepareAudioTrack(track, rect, first, last, invalidate);
        auto &tiles = audioTiles_[track];
        for (auto index = first; index <= last; ++index)
        {
            auto tile = tiles.find(index);
            if (tile == tiles.end())
            {
                auto columns = BuildSpectrumColumns(state_.Analysis().Data(), markers, track,
                                                    static_cast<double>(index) * tileSeconds, tileSeconds, minimumHz,
                                                    maximumHz, longestSound);
                auto &tileNode = audioRoots_[track]->CreateChild("Spectrum.Tile");
                auto &geometry = tileNode.AddComponent<EditorVisual>();
                BuildSpectrumGeometry(geometry, columns, rect);
                tile = tiles.emplace(index, AudioTile{&tileNode, std::move(columns)}).first;
            }
            tile->second.node->SetVisible(true);
            tile->second.node->SetPosition(
                {static_cast<float>((index * tileSeconds - begin) / state_.audioWindow * rect.width), 0});
        }
    }
    UpdateWaveformColumns(begin, columnSeconds);
    audioColumnsKey_ = key;
    audioColumnsDocument_ = &state_.Document();
    audioColumnsDocumentRevision_ = state_.Document().Revision();
}

void EditorView::DrawAudioSpectrum(const Rect rect, const double minimumHz, const double maximumHz)
{
    Box(rect, {0, 0, 0, 1});
    for (const double frequency : {20.0, 100.0, 1000.0, 5000.0, maximumHz})
    {
        if (frequency < minimumHz || frequency > maximumHz)
            continue;
        const float y = rect.y + rect.height * static_cast<float>(1 - std::log(frequency / minimumHz) /
                                                                          std::log(maximumHz / minimumHz));
        audioOverlays_[rect.y == MusicSpectrum.y ? 0 : 1]->AddRectangle({0, rect.height - (y - rect.y), rect.width, 1},
                                                                        {1, 1, 1, .10F}, 0);
        Text({125, std::clamp(y - 10, rect.y, rect.y + rect.height - 20), 78, 20},
             frequency >= 1000 ? Decimal(frequency / 1000, 0) + std::wstring(texts_.Get("Common.kilohertzSuffix"))
                               : Decimal(frequency, 0) + std::wstring(texts_.Get("Common.hertzSuffix")),
             14);
    }
    for (std::size_t color = 0; color < SpectrumColors.size(); ++color)
        Box({1795, rect.y + rect.height * static_cast<float>(SpectrumColors.size() - 1 - color) / SpectrumColors.size(),
             18, rect.height / SpectrumColors.size() + .1F},
            SpectrumColors[color]);
    for (int tick = 0; tick <= 3; ++tick)
        Text({1820, rect.y + (rect.height - 20) * tick / 3, 70, 20},
             std::to_wstring(-30 * tick) + std::wstring(texts_.Get("Common.decibelsSuffix")), 14);
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
             Decimal(seconds, step < 1 ? 2 : 0) + std::wstring(texts_.Get("Common.secondsSuffix")), 14);
        Box({x, Waveform.y, 1, Waveform.height}, {1, 1, 1, .08F});
        for (std::size_t track = 0; track < audioOverlays_.size(); ++track)
            audioOverlays_[track]->AddRectangle({x - Waveform.x, 0, 1, MusicSpectrum.height}, {1, 1, 1, .08F}, 0);
    }
    Box({Waveform.x + Waveform.width * .25F, Waveform.y, 2, Waveform.height}, White);
    for (auto *overlay : audioOverlays_)
    {
        overlay->AddRectangle({MusicSpectrum.width * .25F, 0, 2, MusicSpectrum.height}, White, 0);
        overlay->Finish();
    }
}

void EditorView::DrawAudio()
{
    const auto &text = Texts();
    try
    {
        state_.PrepareAudioPreview();
    }
    catch (const std::exception &error)
    {
        state_.Analysis().ClearMarkers(state_.Document().Revision());
        state_.SetStatus(error.what());
    }

    // Pool each source into the same visible time and frequency grid.
    const double begin = state_.TimeMilliseconds() / 1000.0 - state_.audioWindow * .25;
    double minimumHz = 20, maximumHz = 20'000, longestSound = 0;
    const auto &sounds = state_.Analysis().Data().sounds;
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
    UpdateAudioTiles(begin, minimumHz, maximumHz, longestSound);

    // Draw distinct tracks before their shared ruler/cursor and controls.
    Box({112, 76, 1784, 797}, Paper, 8);
    Text({132, 90, 400, 28}, std::wstring(text.waveform), 22);
    Text({132, 340, 1500, 28}, std::wstring(text.musicSpectrum), 22);
    Text({132, 606, 1500, 28}, std::wstring(text.hitSoundSpectrum), 22);
    DrawAudioWaveform(musicColumns_);
    DrawAudioSpectrum(MusicSpectrum, minimumHz, maximumHz);
    DrawAudioSpectrum(HitSpectrum, minimumHz, maximumHz);
    DrawAudioAlignment(begin);
    DrawAudioSelection(begin);
    DrawAudioTimeRuler(begin);
    DrawPlaybackRateControl();
    Button({1620, 85, 54, 32}, std::wstring(texts_.Get("Common.zoomIn")),
           [this]
           {
               state_.audioWindow = std::max(.25, state_.audioWindow / 2);
               state_.RequestRebuild();
           });
    Button({1684, 85, 54, 32}, std::wstring(texts_.Get("Common.zoomOut")),
           [this]
           {
               state_.audioWindow = std::min(120.0, state_.audioWindow * 2);
               state_.RequestRebuild();
           });
    Text({1748, 88, 140, 24}, Decimal(state_.audioWindow, 2) + std::wstring(texts_.Get("Common.secondsSuffix")), 16);
    if (state_.Analysis().Running())
        Text({570, 90, 490, 28}, std::wstring(text.analyzingAudio), 18, Blue);
    DrawTimeline();
    DrawAudioControls();
}
