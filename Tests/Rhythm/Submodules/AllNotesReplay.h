#pragma once
#include "TestSupport.h"
#include <set>

namespace finger_drum::tests
{
    // Test-only timestamped replay. No autoplay path is added to the game.
    struct ReplayInput
    {
        rhythm::RhythmTime time;
        rhythm::PhysicalKey key;
        rhythm::InputEdge edge;
    };

    class AllNotesReplay final
    {
      public:
        explicit AllNotesReplay(const mode::PlaySession &session)
        {
            const auto &notes = session.Gear().Lanes().front()->Notes();
            Require(notes.size() == 19, "All-notes chart must contain 19 logical notes.");
            std::set<mode::NoteVisualKind> kinds;
            for (const auto &note : notes)
            {
                const auto &view = *session.FindNotePresentation(note->Id());
                kinds.insert(view.visualKind);
                const rhythm::PhysicalKey key = view.visualKind == mode::NoteVisualKind::Kat ||
                                                        view.visualKind == mode::NoteVisualKind::BigKat ||
                                                        view.visualKind == mode::NoteVisualKind::KatBuzz
                                                    ? 'D'
                                                    : 'F';
                const auto &target = note->Accuracy().target;
                if (target.kind == rhythm::NoteAccuracyKind::Hold)
                {
                    const bool dense = view.tickTimes.size() > 7;
                    const auto press = note->Timing() + rhythm::RhythmDuration{dense ? 50'000 : 0};
                    inputs_.push_back({press, key, rhythm::InputEdge::Pressed});
                    inputs_.push_back({view.endTime + rhythm::RhythmDuration{1}, key, rhythm::InputEdge::Released});
                }
                else if (target.kind == rhythm::NoteAccuracyKind::Ticks)
                {
                    for (const auto tick : view.tickTimes)
                        AddTap(tick, key);
                }
                else if (target.kind == rhythm::NoteAccuracyKind::HitCount)
                {
                    for (std::size_t hit = 0; hit < target.hits; ++hit)
                    {
                        const auto action =
                            view.visualKind == mode::NoteVisualKind::DengDeng && hit % 2 != 0 ? 'D' : 'F';
                        AddTap(note->Timing() + rhythm::RhythmDuration{static_cast<long long>(hit) * 80'000}, action);
                    }
                }
                else
                {
                    AddTap(note->Timing(), key);
                    if (target.hits == 2)
                        AddTap(note->Timing(), view.visualKind == mode::NoteVisualKind::Purple ? 'D'
                                               : key == 'F'                                    ? 'J'
                                                                                               : 'K');
                }
                end_ = std::max(end_, view.hasEndTime ? view.endTime : note->Timing());
            }
            // TickRoll uses the same skin appearance as Roll (also for BigRoll).
            Require(kinds.size() == 11, "Chart must exercise every Taiko visual kind.");
            std::stable_sort(inputs_.begin(), inputs_.end(),
                             [](const auto &a, const auto &b) { return a.time < b.time; });
        }

        [[nodiscard]] rhythm::RhythmTime End() const
        {
            return end_ + rhythm::RhythmDuration{500'000};
        }

        rhythm::NoteProcessResult Advance(mode::PlaySession &session, rhythm::RhythmTime now)
        {
            rhythm::NoteProcessResult result;
            while (next_ < inputs_.size() && inputs_[next_].time <= now)
            {
                const auto &input = inputs_[next_++];
                // Resolve expiry before an input, without processing its exact-time ticks early.
                result.Append(session.Update(input.time - rhythm::RhythmDuration{1}, held_));
                result.Append(session.ProcessInput(input.key, input.edge, input.time));
                if (input.edge == rhythm::InputEdge::Pressed)
                    held_.push_back(input.key);
                else
                    std::erase(held_, input.key);
            }
            result.Append(session.Update(now, held_));
            log_.Append(result);
            return result;
        }

        void Verify(const mode::PlaySession &session) const
        {
            Require(session.FinalizedNoteCount() == 19 && log_.finalizedAccuracies.size() == 19,
                    "Every logical note must finalize exactly once.");
            for (const auto &note : session.Gear().Lanes().front()->Notes())
            {
                Require(note->State() == rhythm::NoteState::Completed,
                        "Scripted inputs must complete every note type.");
                const auto &accuracy = note->Accuracy();
                if (accuracy.target.kind != rhythm::NoteAccuracyKind::Hold)
                {
                    Require(accuracy.ScoreRate() == 1.0, "Normal replay notes must score 100%.");
                    continue;
                }
                const auto &view = *session.FindNotePresentation(note->Id());
                const auto press = note->Timing() + rhythm::RhythmDuration{view.tickTimes.size() > 7 ? 50'000 : 0};
                std::size_t index = 0, missed = 0;
                for (const auto &event : log_.events)
                {
                    if (event.noteId != note->Id() || (event.type != rhythm::NoteEventType::TickAccepted &&
                                                       event.type != rhythm::NoteEventType::TickMissed))
                        continue;
                    Require(index < view.tickTimes.size() && event.tickIndex == index &&
                                event.eventTime == view.tickTimes[index],
                            "Buzz ticks must appear once in compiled order, at their own time.");
                    const bool overdue = view.tickTimes[index++] < press;
                    Require((event.type == rhythm::NoteEventType::TickMissed) == overdue,
                            "Only ticks before the late head press must be missed.");
                    if (overdue)
                        ++missed;
                }
                Require(index == view.tickTimes.size() && accuracy.acceptedTicks == index - missed &&
                            accuracy.acceptedHits == 1,
                        "Late GOOD head must retain all missed and accepted body ticks.");
                const auto head = note->Profile().Evaluate(note->Timing(), press);
                const double expected = (head.scoreRate + double(index - missed) / index) * 0.5;
                Require(std::abs(accuracy.ScoreRate() - expected) < 1e-9,
                        "Buzz score must combine head and complete tick denominator.");
                std::cout << "Buzz " << note->Id() << ": " << missed << " missed / " << index << " ticks; accepted "
                          << accuracy.acceptedTicks << '\n';
            }
            std::cout << "All-notes replay verified: 19 completed; accuracy " << *session.AccuracyRate() * 100 << "%\n";
        }

      private:
        void AddTap(rhythm::RhythmTime time, rhythm::PhysicalKey key)
        {
            inputs_.push_back({time, key, rhythm::InputEdge::Pressed});
            inputs_.push_back({time + rhythm::RhythmDuration{1}, key, rhythm::InputEdge::Released});
        }
        std::vector<ReplayInput> inputs_;
        std::vector<rhythm::PhysicalKey> held_;
        std::size_t next_{};
        rhythm::RhythmTime end_{};
        rhythm::NoteProcessResult log_;
    };
} // namespace finger_drum::tests
