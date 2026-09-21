#pragma once
#include "EditorAnalysisController.h"
#include <array>
#include <memory>

class EditorWorkspace final
{
  public:
    explicit EditorWorkspace(finger_drum::GameplayLaunchRequest selected)
        : request(std::move(selected))
    {
    }
    void Initialize();
    void UpdateAnalysis();
    void SelectTool(int value);
    void Save();
    void PlaceNote(finger_drum::chart::MusicalPosition position);
    finger_drum::GameplayLaunchRequest request;
    std::unique_ptr<finger_drum::chart::ChartEditor> editor;
    EditorAnalysisController analysis;
    std::optional<finger_drum::chart::MusicalPosition> pending;
    int tab{}, tool{1}, popup{-1}, division{16};
    int smallTool{1}, bigTool{3}, rollTool{11}, focusTool{15};
    std::int64_t firstMeasure{};
    bool realtime{}, rebuild{true};
    double timeMs{}, audioWindow{8.0};
    std::string status;
    std::array<std::string, 4> timingFields{"1", "0/4", "120", "4/4"};
    std::array<std::string, 7> effectFields{"1", "0/4", "", "", "1", "1", "HitSound"};
    int effectType{}, curve{};
    std::size_t listOffset{};
};
