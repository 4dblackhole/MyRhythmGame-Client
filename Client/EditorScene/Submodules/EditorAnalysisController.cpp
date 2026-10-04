#include "EditorAnalysisController.h"
#include "EditorSupport.h"
#include <algorithm>
#include <array>
using namespace editor_ui;

bool EditorAnalysisController::Update(const chart::IEditorDocument &document, const IEditorMode &mode,
                                      finger_drum::GameplayLaunchRequest &request, std::string &status)
{
    bool changed = false;
    // Asset resolution/worker restart happens only after metadata changes.
    // Ordinary update ticks only poll the asynchronous result.
    if (analysisDirty)
    {
        analysisDirty = false;
        auto files = mode.AudioFiles(document, request);
        if (files != analysisFiles)
        {
            analysisStop.request_stop();
            if (analysisJob.valid())
                analysisJob.wait();
            analysisStop = std::stop_source{};
            analysisFiles = files;
            analysisJob = std::async(std::launch::async, [files = std::move(files), stop = analysisStop.get_token()] {
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
    if (analysisJob.valid() && analysisJob.wait_for(std::chrono::seconds{0}) == std::future_status::ready)
    {
        analysis = analysisJob.get();
        if (!analysis.errors.empty())
            status = analysis.errors;
        changed = true;
    }
    return changed;
}

void EditorAnalysisController::CacheAudioMarkers(const chart::IEditorDocument &document, const IEditorMode &mode)
{
    if (audioMarkerDocument == &document && audioMarkerMode == &mode && audioMarkerRevision == document.Revision())
        return;
    auto markers = mode.AudioMarkers(document);
    audioMarkers = std::move(markers);
    audioMarkerDocument = &document;
    audioMarkerMode = &mode;
    audioMarkerRevision = document.Revision();
}
