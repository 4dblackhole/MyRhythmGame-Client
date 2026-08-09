#include "Lane/Lane.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace finger_drum::rhythm
{
    Lane::Lane(const std::size_t id) noexcept
        : id_(id)
    {
    }

    std::size_t Lane::Id() const noexcept
    {
        return id_;
    }

    void Lane::AddNote(std::unique_ptr<INote> note)
    {
        if (finalized_)
        {
            throw std::logic_error(
                "A finalized lane cannot accept additional notes.");
        }
        if (note == nullptr)
        {
            throw std::invalid_argument("A lane cannot own a null note.");
        }
        notes_.push_back(std::move(note));
    }

    void Lane::Finalize()
    {
        std::ranges::stable_sort(
            notes_,
            [](const std::unique_ptr<INote>& left,
               const std::unique_ptr<INote>& right)
            {
                if (left->Timing() != right->Timing())
                {
                    return left->Timing() < right->Timing();
                }
                return left->Id() < right->Id();
            });
        finalized_ = true;
        Reset();
    }

    void Lane::Reset() noexcept
    {
        for (const std::unique_ptr<INote>& note : notes_)
        {
            note->Reset();
        }
        currentIndex_ = 0;
    }

    NoteProcessResult Lane::ProcessInput(const RhythmInputEvent& input)
    {
        if (!finalized_)
        {
            throw std::logic_error(
                "Finalize the lane before processing input.");
        }

        NoteProcessResult result;
        AdvancePastTerminalNotes();
        INote* current = CurrentNote();
        if (current == nullptr)
        {
            return result;
        }

        const JudgementResult currentJudgement = current->Preview(input);

        // An early Bad is reported as an attempt, but it deliberately keeps
        // focus on the same note so one premature press cannot start a bad
        // judgement cascade through a dense pattern.
        if (currentJudgement.grade == JudgementGrade::Bad &&
            currentJudgement.signedError < RhythmDuration::zero())
        {
            result.events.push_back(MakeEarlyBadEvent(
                *current,
                currentJudgement,
                input.time));
            return result;
        }

        // If the current note is already in late Bad while the next note can
        // accept this same input within Good, retire the missed note first and
        // apply the input to the next note in one deterministic transaction.
        if (currentJudgement.grade == JudgementGrade::Bad &&
            currentJudgement.signedError > RhythmDuration::zero() &&
            currentIndex_ + 1 < notes_.size())
        {
            INote& next = *notes_[currentIndex_ + 1];
            const JudgementResult nextJudgement = next.Preview(input);
            if (IsAtLeastAsAccurateAs(
                    nextJudgement.grade,
                    JudgementGrade::Good) &&
                next.CanAccept(input))
            {
                result.Append(current->MarkMissed(input.time));
                ++currentIndex_;
                result.Append(next.ProcessInput(input));
                AdvancePastTerminalNotes();
                return result;
            }
        }

        result.Append(current->ProcessInput(input));
        AdvancePastTerminalNotes();
        return result;
    }

    NoteProcessResult Lane::Update(
        const RhythmTime time,
        const std::span<const NoteAction> heldActions)
    {
        if (!finalized_)
        {
            throw std::logic_error("Finalize the lane before updating it.");
        }

        NoteProcessResult result;
        AdvancePastTerminalNotes();
        while (INote* current = CurrentNote())
        {
            result.Append(current->Update({time, heldActions}));
            if (current->State() == NoteState::Completed ||
                current->State() == NoteState::Missed)
            {
                ++currentIndex_;
                continue;
            }
            if (time > current->ExpireTime())
            {
                result.Append(current->MarkMissed(time));
                ++currentIndex_;
                continue;
            }
            break;
        }
        return result;
    }

    bool Lane::Empty() const noexcept
    {
        return notes_.empty();
    }

    std::size_t Lane::CurrentIndex() const noexcept
    {
        return currentIndex_;
    }

    INote* Lane::CurrentNote() noexcept
    {
        return currentIndex_ < notes_.size()
            ? notes_[currentIndex_].get()
            : nullptr;
    }

    const INote* Lane::CurrentNote() const noexcept
    {
        return const_cast<Lane*>(this)->CurrentNote();
    }

    const std::vector<std::unique_ptr<INote>>& Lane::Notes() const noexcept
    {
        return notes_;
    }

    void Lane::AdvancePastTerminalNotes() noexcept
    {
        while (currentIndex_ < notes_.size())
        {
            const NoteState state = notes_[currentIndex_]->State();
            if (state != NoteState::Completed && state != NoteState::Missed)
            {
                break;
            }
            ++currentIndex_;
        }
    }

    NoteEvent Lane::MakeEarlyBadEvent(
        const INote& note,
        const JudgementResult& judgement,
        const RhythmTime time) const noexcept
    {
        return NoteEvent{
            note.Id(),
            NoteEventType::InputRejected,
            note.State(),
            note.State(),
            judgement,
            time};
    }
}
