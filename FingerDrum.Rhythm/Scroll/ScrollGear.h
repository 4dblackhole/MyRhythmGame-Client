#pragma once

#include "Lane/Lane.h"

#include <cstddef>
#include <memory>
#include <vector>

namespace finger_drum::rhythm
{
    struct ScrollNoteSnapshot
    {
        NoteId noteId{};
        std::size_t laneIndex{};
        RhythmTime timing{};
        RhythmDuration timeFromJudgement{};
        float normalizedTravel{};
        NoteState state{NoteState::Pending};
    };

    struct ScrollGearSnapshot
    {
        RhythmTime time{};
        std::vector<ScrollNoteSnapshot> notes;
    };

    class IScrollGearPresentation
    {
    public:
        virtual ~IScrollGearPresentation() = default;
        virtual void Present(const ScrollGearSnapshot& snapshot) = 0;
    };

    class ScrollGear final
    {
    public:
        Lane& CreateLane();
        [[nodiscard]] Lane* FindLane(std::size_t laneIndex) noexcept;
        [[nodiscard]] const Lane* FindLane(
            std::size_t laneIndex) const noexcept;
        [[nodiscard]] std::size_t LaneCount() const noexcept;
        [[nodiscard]] std::vector<std::unique_ptr<Lane>>& Lanes() noexcept;
        [[nodiscard]] const std::vector<std::unique_ptr<Lane>>&
            Lanes() const noexcept;

        void Finalize();
        void Reset() noexcept;
        [[nodiscard]] NoteProcessResult Update(
            RhythmTime time,
            std::span<const NoteAction> heldActions = {});
        [[nodiscard]] ScrollGearSnapshot BuildSnapshot(
            RhythmTime time,
            RhythmDuration approachDuration,
            RhythmDuration pastDuration = RhythmDuration::zero()) const;

    private:
        std::vector<std::unique_ptr<Lane>> lanes_;
    };
}
