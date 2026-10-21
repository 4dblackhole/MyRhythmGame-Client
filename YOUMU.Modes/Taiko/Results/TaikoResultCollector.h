#pragma once
#include "Mode/Results/IPlayResultCollector.h"
#include "Taiko/Results/TaikoPlayResult.h"
#include <unordered_map>

class TaikoResultCollector final : public IPlayResultCollector
{
  public:
    explicit TaikoResultCollector(PlayResultSummary summary, std::size_t reserveNotes);
    void RegisterNote(NoteId id, TaikoNoteType type, RhythmTime time);
    void RecordInput(const RhythmInputEvent &input) override;
    void RecordControl(PlayControlEvent event) override;
    void Consume(const NoteProcessResult &result, bool keyPressed) override;
    void RecordRetirement(const NoteAccuracy &accuracy) override;
    void SetInputMapping(std::map<PhysicalKey, NoteAction> mapping);
    void SetInitialTime(RhythmTime time) noexcept override;
    void Reset() noexcept override;
    void ResetJudgements() noexcept override;
    std::unique_ptr<IPlayResult> TakeResult(PlayEndReason reason, RhythmTime time) override;

  private:
    void ConsumeEvent(const NoteEvent &event, bool keyPressed);
    void FinalizeNote(const NoteAccuracy &accuracy, bool retired = false);
    void Summarize();
    std::unique_ptr<TaikoPlayResult> result_;
    std::unordered_map<NoteId, std::size_t> noteIndices_;
};
