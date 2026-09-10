#pragma once

#include "Note/Note.h"

#include <cstddef>
#include <memory>
#include <span>
#include <vector>

namespace finger_drum::rhythm
{
    class Lane final
    {
    public:
        explicit Lane(std::size_t id = 0) noexcept;

        [[nodiscard]] std::size_t Id() const noexcept;
        void AddNote(std::unique_ptr<INote> note);
        void Finalize();
        void Reset() noexcept;

        [[nodiscard]] NoteProcessResult ProcessInput(
            const RhythmInputEvent& input);
        [[nodiscard]] NoteProcessResult Update(
            RhythmTime time,
            std::span<const NoteAction> heldActions = {});

        [[nodiscard]] bool Empty() const noexcept;
        [[nodiscard]] std::size_t CurrentIndex() const noexcept;
        [[nodiscard]] INote* CurrentNote() noexcept;
        [[nodiscard]] const INote* CurrentNote() const noexcept;
        [[nodiscard]] const std::vector<std::unique_ptr<INote>>&
            Notes() const noexcept;

    private:
        friend class ScrollGear;

        void AdvancePastTerminalNotes() noexcept;
        [[nodiscard]] NoteEvent MakeEarlyBadEvent(
            const INote& note,
            const JudgementResult& judgement,
            RhythmTime time) const noexcept;

        std::size_t id_{};
        std::vector<std::unique_ptr<INote>> notes_;
        // Prefix maximum lets ScrollGear skip terminal history without
        // assuming that note expiry is ordered by head timing. Overlapping
        // long notes can otherwise hide an older, still-visible tail.
        std::vector<RhythmTime> latestExpireThrough_;
        std::size_t currentIndex_{};
        bool finalized_{};
    };
}
