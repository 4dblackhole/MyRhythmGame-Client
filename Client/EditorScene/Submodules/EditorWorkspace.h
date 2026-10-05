#pragma once
#include "EditorAnalysisController.h"
#include "EditorForms.h"
#include "Modes/IEditorMode.h"

class EditorWorkspace final : public IEditorContext
{
  public:
    explicit EditorWorkspace(finger_drum::GameplayLaunchRequest selected, std::unique_ptr<IEditorMode> mode = {})
        : request_(std::move(selected)), mode_(std::move(mode))
    {
    }
    // Injected modes/documents use the same owner and editing boundary.
    EditorWorkspace(std::unique_ptr<IEditorMode> mode, std::unique_ptr<finger_drum::chart::IEditorDocument> document)
        : editor_(std::move(document)), mode_(std::move(mode))
    {
    }
    void Initialize();
    void UpdateAnalysis();
    void SelectTool(int value) override;
    void Save();
    void PlaceNote(finger_drum::chart::MusicalPosition position);
    [[nodiscard]] std::pair<double, double> TimelineRangeMilliseconds() const;
    void Seek(double milliseconds) override;
    double TimeMilliseconds() const noexcept override
    {
        return timeMs_;
    }
    const finger_drum::chart::IEditorDocument &Document() const noexcept override
    {
        return *editor_;
    }
    IEditorMode &Mode() noexcept
    {
        return *mode_;
    }
    EditorAnalysisController &Analysis() noexcept
    {
        return analysis_;
    }
    const EditorScoreState &Score() const noexcept override
    {
        return score_;
    }
    void SetFirstMeasure(std::int64_t) override;
    void SetDivision(std::int64_t);
    void SetRealtime(bool);
    void RequestRebuild() noexcept override
    {
        rebuild_ = true;
    }
    bool NeedsRebuild() const noexcept
    {
        return rebuild_;
    }
    void FinishBuild() noexcept
    {
        rebuild_ = false;
    }
    EditorTab Tab() const noexcept
    {
        return tab_;
    }
    const std::string &Status() const noexcept
    {
        return status_;
    }
    void SetStatus(std::string value)
    {
        status_ = std::move(value);
        RequestRebuild();
    }
    void SelectTab(EditorTab value);
    void SelectTimingPosition(finger_drum::chart::MusicalPosition) override;
    void Replace(finger_drum::chart::PatternDocument, finger_drum::chart::EffectDocument) override;
    void AddNote(finger_drum::chart::MusicalPosition, int keyType,
                 std::optional<finger_drum::chart::MusicalPosition> end = {},
                 std::vector<std::string> extra = {}) override;
    void DeleteNote(std::size_t sourceOrder) override;
    void SetMeasureLength(std::int64_t, finger_drum::chart::Rational);
    void RemoveTiming(std::size_t);
    void ApplyBpm();
    void ApplyMeasureLength();
    void RemoveEffect(std::size_t);
    void ApplyEffect();
    double audioWindow{8.0};
    EditorTimingForm timingForm;
    EditorEffectForm effectForm;
    std::size_t listOffset{};

  private:
    void DocumentChanged();
    EditorTab tab_{EditorTab::Pattern};
    bool rebuild_{true};
    std::string status_;
    finger_drum::GameplayLaunchRequest request_;
    std::unique_ptr<finger_drum::chart::IEditorDocument> editor_;
    std::unique_ptr<IEditorMode> mode_;
    EditorAnalysisController analysis_;
    EditorScoreState score_;
    double timeMs_{};
    std::uint64_t audioSourceRevision_{};
};
