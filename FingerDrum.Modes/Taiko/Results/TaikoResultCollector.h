#pragma once
#include "Mode/Results/IPlayResultCollector.h"
#include "Taiko/Results/TaikoPlayResult.h"
#include <unordered_map>

namespace finger_drum::mode
{
    class TaikoResultCollector final : public IPlayResultCollector
    {
      public:
        explicit TaikoResultCollector(PlayResultSummary summary, std::size_t reserveNotes);
        void RegisterNote(rhythm::NoteId id, TaikoNoteType type, rhythm::RhythmTime time);
        void RecordInput(const rhythm::RhythmInputEvent &input) override;
        void Consume(const rhythm::NoteProcessResult &result, bool keyPressed) override;
        void SetInputMapping(std::map<rhythm::PhysicalKey, rhythm::NoteAction> mapping);
        void SetInitialTime(rhythm::RhythmTime time) noexcept override;
        void Reset() noexcept override;
        std::unique_ptr<IPlayResult> TakeResult(PlayEndReason reason, rhythm::RhythmTime time) override;

      private:
        void ConsumeEvent(const rhythm::NoteEvent &event, bool keyPressed);
        void FinalizeNote(const rhythm::NoteAccuracy &accuracy);
        void Summarize();
        std::unique_ptr<TaikoPlayResult> result_;
        std::unordered_map<rhythm::NoteId, std::size_t> noteIndices_;
    };
} // namespace finger_drum::mode
