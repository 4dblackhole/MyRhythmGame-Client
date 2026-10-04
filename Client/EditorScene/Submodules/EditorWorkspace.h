#pragma once
#include "EditorAnalysisController.h"
#include <array>
#include <memory>
#include <utility>

class EditorWorkspace final
{
  public:
    explicit EditorWorkspace(finger_drum::GameplayLaunchRequest selected) : request(std::move(selected))
    {
    }
    void Initialize();
    void UpdateAnalysis();
    void SelectTool(int value);
    void Save();
    void PlaceNote(finger_drum::chart::MusicalPosition position);
    [[nodiscard]] std::pair<double, double> TimelineRangeMilliseconds() const;
    finger_drum::GameplayLaunchRequest request;
    std::unique_ptr<finger_drum::chart::IEditorDocument> editor;
    std::unique_ptr<IEditorMode> mode;
    EditorAnalysisController analysis;
    int tab{}, division{4};
    std::int64_t firstMeasure{};
    bool realtime{}, rebuild{true};
    double timeMs{}, audioWindow{8.0};
    std::string status;
    std::array<std::string, 4> timingFields{"1", "0/4", "120", "4/4"};
    std::array<std::string, 7> effectFields{"1", "0/4", "", "", "1", "1", "HitSound"};
    int effectType{}, curve{};
    std::size_t listOffset{};
};
