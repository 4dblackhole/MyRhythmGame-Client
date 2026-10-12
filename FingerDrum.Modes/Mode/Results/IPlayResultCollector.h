#pragma once
#include "Mode/Results/IPlayResult.h"
#include <memory>

namespace finger_drum::mode
{
    class IPlayResultCollector
    {
      public:
        virtual ~IPlayResultCollector() = default;
        virtual void RecordInput(const rhythm::RhythmInputEvent &input) = 0;
        virtual void Consume(const rhythm::NoteProcessResult &result, bool keyPressed) = 0;
        virtual void SetInitialTime(rhythm::RhythmTime time) noexcept = 0;
        virtual void Reset() noexcept = 0;
        virtual std::unique_ptr<IPlayResult> TakeResult(PlayEndReason reason, rhythm::RhythmTime time) = 0;
    };
} // namespace finger_drum::mode
