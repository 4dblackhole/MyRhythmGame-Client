#pragma once
#include "Mode/Results/IPlayResult.h"
#include <memory>

class IPlayResultCollector
{
  public:
    virtual ~IPlayResultCollector() = default;
    virtual void RecordInput(const RhythmInputEvent &input) = 0;
    virtual void RecordControl(PlayControlEvent event) = 0;
    virtual void Consume(const NoteProcessResult &result, bool keyPressed) = 0;
    virtual void RecordRetirement(const NoteAccuracy &accuracy) = 0;
    virtual void SetInitialTime(RhythmTime time) noexcept = 0;
    virtual void Reset() noexcept = 0;
    // Rewind replaces judgement data but retains the complete replay trace.
    virtual void ResetJudgements() noexcept = 0;
    virtual std::unique_ptr<IPlayResult> TakeResult(PlayEndReason reason, RhythmTime time) = 0;
};
