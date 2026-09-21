#include "EditorAnalysisController.h"
#include "EditorSupport.h"
#include "App/AssetPaths.h"
#include <algorithm>
#include <array>
using namespace editor_ui;

bool EditorAnalysisController::Update(const chart::ChartEditor &document,
                                      finger_drum::GameplayLaunchRequest &request,
                                      std::string &status)
{
    const auto *editor = &document;
    bool changed = false;
    // Asset resolution/worker restart happens only after metadata changes.
    // Ordinary update ticks only poll the asynchronous result.
    if (analysisDirty)
    {
        analysisDirty = false;
        const auto metadataPath =
            editor->Pattern().sourcePath.parent_path() / editor->Pattern().musicMetadataFile;
        const auto music = chart::ChartParser{}.ParseMusicFile(metadataPath);
        if (!music.Succeeded())
            throw std::runtime_error("Audio analysis: the selected YMM could not be parsed.");
        request.musicPath = metadataPath.parent_path() / music.document.audioFile;
        std::map<std::string, std::filesystem::path> files{{"Music", request.musicPath}};
        for (const auto &[id, file] :
             std::array<std::pair<const char *, const wchar_t *>, 4>{{{"Don", L"don.wav"},
                                                                      {"Kat", L"kat.wav"},
                                                                      {"BigDon", L"bigdon.wav"},
                                                                      {"BigKat", L"bigkat.wav"}}})
            files[id] = mrg_client::asset_paths::default_skin::TaikoHitSound(file);
        for (const auto &[id, path] : editor->Pattern().hitSounds)
            files["Table." + id] = editor->Pattern().sourcePath.parent_path() / path;
        if (files != analysisFiles)
        {
            analysisStop.request_stop();
            if (analysisJob.valid())
                analysisJob.wait();
            analysisStop = std::stop_source{};
            analysisFiles = files;
            analysisJob = std::async(std::launch::async, [files = std::move(files),
                                                          stop = analysisStop.get_token()] {
                AnalysisBatch result;
                for (const auto &[id, path] : files)
                {
                    if (stop.stop_requested())
                        break;
                    try
                    {
                        result.sounds.emplace(id, finger_drum::editor::AnalyzeAudio(path, stop));
                    }
                    catch (const std::exception &error)
                    {
                        result.errors += id + ": " + error.what() + "; ";
                    }
                }
                return result;
            });
        }
    }
    if (analysisJob.valid() &&
        analysisJob.wait_for(std::chrono::seconds{0}) == std::future_status::ready)
    {
        analysis = analysisJob.get();
        if (!analysis.errors.empty())
            status = analysis.errors;
        changed = true;
    }
    return changed;
}

void EditorAnalysisController::CacheAudioMarkers(const chart::ChartEditor &document)
{
    const auto *editor = &document;
    if (audioMarkerRevision == editor->Revision())
        return;
    audioMarkers.clear();
    const auto append = [&](const chart::PatternNote &note, chart::MusicalPosition position,
                            finger_drum::rhythm::RhythmTime time) {
        bool kat = note.keyType == 2 || note.keyType == 4;
        for (auto option : note.extraData)
        {
            std::ranges::transform(option, option.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            if (option == "action=kat")
                kat = true;
        }
        std::string sound =
            kat ? (note.keyType == 4 ? "BigKat" : "Kat") : (note.keyType == 3 ? "BigDon" : "Don");
        const chart::HitSoundChange *last = nullptr;
        for (const auto &c : editor->Effects().hitSoundChanges)
            if (c.keyType == (kat ? 2 : 1) && c.position <= position &&
                (!last || last->position <= c.position))
                last = &c;
        if (last)
            sound = "Table." + last->soundIndex;
        if (!note.hitSound.empty())
            sound = "Table." + note.hitSound;
        audioMarkers.push_back({time.count() / 1e6, std::move(sound)});
    };
    std::optional<chart::PatternNote> head;
    const auto &timeline = editor->Timeline();
    for (const auto &n : editor->Notes())
    {
        if (n.note.actionType == 1 && !head)
            head = n.note;
        if (n.note.actionType != 2)
            append(n.note, n.note.position, n.timing);
        else if (head && head->keyType == n.note.keyType)
        {
            if (head->keyType == 12 || head->keyType == 14 || head->keyType == 17)
            {
                std::int64_t tickDivision = 16;
                for (auto option : head->extraData)
                {
                    std::ranges::transform(option, option.begin(), [](unsigned char c) {
                        return static_cast<char>(std::tolower(c));
                    });
                    if (option.starts_with("tickdivision="))
                        tickDivision = Integer(option.substr(13));
                }
                if (tickDivision <= 0 || tickDivision > 1024)
                    throw std::invalid_argument("TickDivision must be 1..1024.");
                const auto ticks = timeline.CompileSubdivisions(
                    head->position, n.note.position, static_cast<std::size_t>(tickDivision));
                const auto first = timeline.PositionToWholeNotes(head->position);
                // The head was already added. Remaining markers correspond to
                // successful TickRoll taps or a continuously held Buzz.
                for (std::size_t i = 1; i < ticks.size(); ++i)
                    append(*head,
                           timeline.PositionAtWholeNotes(
                               first + chart::Rational{static_cast<std::int64_t>(i), tickDivision}),
                           ticks[i]);
            }
            head.reset();
        }
    }
    std::ranges::stable_sort(audioMarkers, {}, &AudioMarker::seconds);
    audioMarkerRevision = editor->Revision();
}
