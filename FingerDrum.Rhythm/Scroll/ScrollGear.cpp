#include "Scroll/ScrollGear.h"

#include <algorithm>
#include <stdexcept>

namespace finger_drum::rhythm
{
    Lane& ScrollGear::CreateLane()
    {
        auto lane = std::make_unique<Lane>(lanes_.size());
        Lane& result = *lane;
        lanes_.push_back(std::move(lane));
        return result;
    }

    Lane* ScrollGear::FindLane(const std::size_t laneIndex) noexcept
    {
        return laneIndex < lanes_.size() ? lanes_[laneIndex].get() : nullptr;
    }

    const Lane* ScrollGear::FindLane(
        const std::size_t laneIndex) const noexcept
    {
        return const_cast<ScrollGear*>(this)->FindLane(laneIndex);
    }

    std::size_t ScrollGear::LaneCount() const noexcept
    {
        return lanes_.size();
    }

    std::vector<std::unique_ptr<Lane>>& ScrollGear::Lanes() noexcept
    {
        return lanes_;
    }

    const std::vector<std::unique_ptr<Lane>>&
    ScrollGear::Lanes() const noexcept
    {
        return lanes_;
    }

    void ScrollGear::Finalize()
    {
        for (const std::unique_ptr<Lane>& lane : lanes_)
        {
            lane->Finalize();
        }
    }

    void ScrollGear::Reset() noexcept
    {
        for (const std::unique_ptr<Lane>& lane : lanes_)
        {
            lane->Reset();
        }
    }

    NoteProcessResult ScrollGear::Update(
        const RhythmTime time,
        const std::span<const NoteAction> heldActions)
    {
        NoteProcessResult result;
        for (const std::unique_ptr<Lane>& lane : lanes_)
        {
            result.Append(lane->Update(time, heldActions));
        }
        return result;
    }

    ScrollGearSnapshot ScrollGear::BuildSnapshot(
        const RhythmTime time,
        const RhythmDuration approachDuration,
        const RhythmDuration pastDuration) const
    {
        if (approachDuration <= RhythmDuration::zero())
        {
            throw std::invalid_argument(
                "Scroll approach duration must be positive.");
        }

        ScrollGearSnapshot snapshot;
        snapshot.time = time;
        for (std::size_t laneIndex = 0;
             laneIndex < lanes_.size();
             ++laneIndex)
        {
            const Lane& lane = *lanes_[laneIndex];
            const auto& notes = lane.Notes();
            std::size_t firstIndex = std::min(lane.CurrentIndex(), notes.size());
            while (firstIndex > 0 &&
                time <= lane.latestExpireThrough_[firstIndex - 1] + pastDuration)
            {
                --firstIndex;
            }
            const RhythmTime latestTiming = time + approachDuration;
            const auto last = std::ranges::upper_bound(
                notes.begin() + static_cast<std::ptrdiff_t>(firstIndex),
                notes.end(),
                latestTiming,
                {},
                [](const std::unique_ptr<INote>& note)
                {
                    return note->Timing();
                });
            for (auto iterator = notes.begin() +
                    static_cast<std::ptrdiff_t>(firstIndex);
                 iterator != last;
                 ++iterator)
            {
                const std::unique_ptr<INote>& note = *iterator;
                const RhythmDuration delta = note->Timing() - time;
                const RhythmTime expireTime = note->ExpireTime();
                if (time > expireTime + pastDuration)
                {
                    continue;
                }
                snapshot.notes.push_back(ScrollNoteSnapshot{
                    note->Id(),
                    laneIndex,
                    note->Timing(),
                    expireTime,
                    delta,
                    std::clamp(
                        static_cast<float>(delta.count()) /
                        static_cast<float>(approachDuration.count()),
                        -1.0F,
                        1.0F),
                    note->State(),
                    note->Progress()});
            }
        }
        return snapshot;
    }
}
