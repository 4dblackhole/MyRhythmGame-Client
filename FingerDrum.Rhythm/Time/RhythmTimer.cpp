#include "Time/RhythmTimer.h"

#include <algorithm>
#include <limits>

namespace finger_drum::rhythm
{
    void RhythmTimer::Start(
        const std::int64_t performanceCounterTicks,
        const std::int64_t performanceCounterFrequency,
        const RhythmTime timelinePosition)
    {
        counterFrequency_ = std::max<std::int64_t>(
            performanceCounterFrequency,
            1);
        counterAnchor_ = performanceCounterTicks;
        timelineAnchor_ = timelinePosition;
        state_ = State::Running;
    }

    void RhythmTimer::Stop() noexcept
    {
        state_ = State::Stopped;
        timelineAnchor_ = RhythmTime::zero();
        counterAnchor_ = 0;
    }

    void RhythmTimer::Pause(const std::int64_t performanceCounterTicks)
    {
        if (state_ != State::Running)
        {
            return;
        }
        timelineAnchor_ = Now(performanceCounterTicks);
        counterAnchor_ = performanceCounterTicks;
        state_ = State::Paused;
    }

    void RhythmTimer::Resume(const std::int64_t performanceCounterTicks)
    {
        if (state_ != State::Paused)
        {
            return;
        }
        counterAnchor_ = performanceCounterTicks;
        state_ = State::Running;
    }

    void RhythmTimer::Seek(
        const RhythmTime timelinePosition,
        const std::int64_t performanceCounterTicks) noexcept
    {
        timelineAnchor_ = timelinePosition;
        counterAnchor_ = performanceCounterTicks;
    }

    RhythmTimer::State RhythmTimer::CurrentState() const noexcept
    {
        return state_;
    }

    RhythmTime RhythmTimer::Now(
        const std::int64_t performanceCounterTicks) const noexcept
    {
        if (state_ == State::Stopped)
        {
            return RhythmTime::zero();
        }
        if (state_ == State::Paused)
        {
            return timelineAnchor_;
        }
        return timelineAnchor_ + ElapsedSinceAnchor(performanceCounterTicks);
    }

    void RhythmTimer::AnchorDspClock(
        const RhythmTime timelinePosition,
        const std::uint64_t dspClock,
        const int sampleRate) noexcept
    {
        dspTimelineAnchor_ = timelinePosition;
        dspClockAnchor_ = dspClock;
        dspSampleRate_ = std::max(sampleRate, 0);
    }

    bool RhythmTimer::HasDspAnchor() const noexcept
    {
        return dspSampleRate_ > 0;
    }

    std::uint64_t RhythmTimer::ToDspClock(
        const RhythmTime timelinePosition) const noexcept
    {
        if (!HasDspAnchor())
        {
            return 0;
        }

        const long double samples =
            static_cast<long double>(
                (timelinePosition - dspTimelineAnchor_).count()) *
            static_cast<long double>(dspSampleRate_) / 1'000'000.0L;
        if (samples < 0.0L &&
            static_cast<long double>(dspClockAnchor_) < -samples)
        {
            return 0;
        }
        const long double clock =
            static_cast<long double>(dspClockAnchor_) + samples;
        return static_cast<std::uint64_t>(std::clamp<long double>(
            clock,
            0.0L,
            static_cast<long double>(
                std::numeric_limits<std::uint64_t>::max())));
    }

    RhythmTime RhythmTimer::FromDspClock(
        const std::uint64_t dspClock) const noexcept
    {
        if (!HasDspAnchor())
        {
            return RhythmTime::zero();
        }
        const long double sampleDelta =
            static_cast<long double>(dspClock) -
            static_cast<long double>(dspClockAnchor_);
        const auto microseconds = static_cast<RhythmTime::rep>(
            sampleDelta * 1'000'000.0L /
            static_cast<long double>(dspSampleRate_));
        return dspTimelineAnchor_ + RhythmTime{microseconds};
    }

    RhythmDuration RhythmTimer::ElapsedSinceAnchor(
        const std::int64_t performanceCounterTicks) const noexcept
    {
        if (counterFrequency_ <= 0)
        {
            return RhythmDuration::zero();
        }
        const long double seconds =
            static_cast<long double>(
                performanceCounterTicks - counterAnchor_) /
            static_cast<long double>(counterFrequency_);
        return RhythmDuration{static_cast<RhythmDuration::rep>(
            seconds * 1'000'000.0L)};
    }
}
