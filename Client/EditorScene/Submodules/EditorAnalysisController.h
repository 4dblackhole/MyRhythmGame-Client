#pragma once
#include "Audio/EditorAudioAnalysis.h"
#include "GameFlow/GameplayLaunchRequest.h"
#include "Modes/IEditorMode.h"
#include <future>
#include <map>
#include <string>
struct AnalysisBatch
{
    std::map<std::string, finger_drum::editor::AudioAnalysis> sounds;
    std::string errors;
};
class EditorAnalysisController final
{
  public:
    ~EditorAnalysisController()
    {
        Stop();
    }
    void Invalidate() noexcept
    {
        analysisDirty = true;
    }
    void Stop() noexcept
    {
        analysisStop.request_stop();
    }
    bool Update(const finger_drum::chart::IEditorDocument &, const IEditorMode &, finger_drum::GameplayLaunchRequest &,
                std::string &status);
    void CacheAudioMarkers(const finger_drum::chart::IEditorDocument &, const IEditorMode &);
    const AnalysisBatch &Data() const noexcept
    {
        return analysis;
    }
    const std::vector<AudioMarker> &Markers() const noexcept
    {
        return audioMarkers;
    }
    bool Running() const noexcept
    {
        return analysisJob.valid();
    }
    void ClearMarkers(std::uint64_t revision)
    {
        audioMarkers.clear();
        audioMarkerRevision = revision;
        audioMarkerDocument = nullptr;
        audioMarkerMode = nullptr;
    }

  private:
    bool analysisDirty{true};
    std::stop_source analysisStop;
    std::future<AnalysisBatch> analysisJob;
    AnalysisBatch analysis;
    std::map<std::string, std::filesystem::path> analysisFiles;
    std::vector<AudioMarker> audioMarkers;
    std::uint64_t audioMarkerRevision{};
    const finger_drum::chart::IEditorDocument *audioMarkerDocument{};
    const IEditorMode *audioMarkerMode{};
};
