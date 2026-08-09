#pragma once

#include "Common/RhythmTypes.h"

#include <cstdint>

namespace finger_drum::rhythm
{
    class RhythmTimer final
    {
    public:
        enum class State : std::uint8_t
        {
            Stopped,
            Running,
            Paused,
        };

        void Start(
            std::int64_t performanceCounterTicks,
            std::int64_t performanceCounterFrequency,
            RhythmTime timelinePosition = RhythmTime::zero());
        void Stop() noexcept;
        void Pause(std::int64_t performanceCounterTicks);
        void Resume(std::int64_t performanceCounterTicks);
        void Seek(
            RhythmTime timelinePosition,
            std::int64_t performanceCounterTicks) noexcept;

        [[nodiscard]] State CurrentState() const noexcept;
        [[nodiscard]] RhythmTime Now(
            std::int64_t performanceCounterTicks) const noexcept;

        // Establishes the single mapping between rhythm time and the audio
        // backend's DSP clock. Music, BMS sounds and hitsounds all use it.
        void AnchorDspClock(
            RhythmTime timelinePosition,
            std::uint64_t dspClock,
            int sampleRate) noexcept;
        [[nodiscard]] bool HasDspAnchor() const noexcept;
        [[nodiscard]] std::uint64_t ToDspClock(
            RhythmTime timelinePosition) const noexcept;
        [[nodiscard]] RhythmTime FromDspClock(
            std::uint64_t dspClock) const noexcept;

    private:
        [[nodiscard]] RhythmDuration ElapsedSinceAnchor(
            std::int64_t performanceCounterTicks) const noexcept;

        State state_{State::Stopped};
        RhythmTime timelineAnchor_{};
        std::int64_t counterAnchor_{};
        std::int64_t counterFrequency_{};

        RhythmTime dspTimelineAnchor_{};
        std::uint64_t dspClockAnchor_{};
        int dspSampleRate_{};
    };
}
