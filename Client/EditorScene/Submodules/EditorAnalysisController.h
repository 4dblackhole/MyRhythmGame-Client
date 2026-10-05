#pragma once
#include "Audio/EditorAudioAnalysis.h"
#include "GameFlow/GameplayLaunchRequest.h"
#include "Modes/IEditorAudioSource.h"
#include <memory>
#include <map>
#include <string>
struct AnalysisBatch
{
    std::map<std::string, finger_drum::editor::AudioAnalysis> sounds;
    std::string errors;
};
struct EditorAnalysisJob;
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
    void Stop() noexcept;
    bool Update(const finger_drum::chart::IEditorDocument &, const IEditorAudioSource &,
                const finger_drum::GameplayLaunchRequest &, std::string &status);
    void CacheAudioMarkers(const finger_drum::chart::IEditorDocument &, const IEditorAudioSource &);
    const AnalysisBatch &Data() const noexcept
    {
        static const AnalysisBatch empty;
        return analysis_ ? *analysis_ : empty;
    }
    const std::vector<AudioMarker> &Markers() const noexcept
    {
        return audioMarkers;
    }
    bool Running() const noexcept
    {
        return job_ != nullptr;
    }
    std::uint64_t Revision() const noexcept { return analysisRevision_; }
    void ClearMarkers(std::uint64_t revision)
    {
        audioMarkers.clear();
        audioMarkerRevision = revision;
        audioMarkerDocument = nullptr;
        audioMarkerMode = nullptr;
    }

  private:
    bool analysisDirty{true};
    std::shared_ptr<EditorAnalysisJob> job_;
    std::shared_ptr<const AnalysisBatch> analysis_;
    std::uint64_t analysisRevision_{};
    std::map<std::string, std::filesystem::path> analysisFiles;
    std::vector<AudioMarker> audioMarkers;
    std::uint64_t audioMarkerRevision{};
    const finger_drum::chart::IEditorDocument *audioMarkerDocument{};
    const IEditorAudioSource *audioMarkerMode{};
};
