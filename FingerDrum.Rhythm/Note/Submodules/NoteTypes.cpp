#include "Note/Submodules/NoteTypes.h"
#include "Note/Submodules/RuleHelpers.h"
#include <algorithm>
#include <format>
#include <iterator>
#include <numeric>
#include <stdexcept>
#include <utility>

namespace finger_drum::rhythm
{
    using namespace detail;

    void NoteProcessResult::Append(NoteProcessResult other)
    {
        events.insert(events.end(), std::make_move_iterator(other.events.begin()),
                      std::make_move_iterator(other.events.end()));
        audioCues.insert(audioCues.end(), std::make_move_iterator(other.audioCues.begin()),
                         std::make_move_iterator(other.audioCues.end()));
        finalizedAccuracies.insert(finalizedAccuracies.end(),
                                   std::make_move_iterator(other.finalizedAccuracies.begin()),
                                   std::make_move_iterator(other.finalizedAccuracies.end()));
    }

    double NoteAccuracy::ScoreRate() const noexcept
    {
        const auto ratio = [](std::size_t accepted, std::size_t required) {
            return required == 0 ? 0.0
                                 : std::min(1.0, static_cast<double>(accepted) /
                                                     static_cast<double>(required));
        };
        const double timing = hitScoreRates.empty() ? 0.0
                                                    : std::accumulate(hitScoreRates.begin(),
                                                                      hitScoreRates.end(), 0.0) /
                                                          static_cast<double>(hitScoreRates.size());
        switch (target.kind)
        {
        case NoteAccuracyKind::TimingHits:
            return timing;
        case NoteAccuracyKind::HitCount:
            return ratio(acceptedHits, target.hits);
        case NoteAccuracyKind::Ticks:
            return ratio(acceptedTicks, target.ticks);
        case NoteAccuracyKind::Hold:
            return target.ticks == 0 ? timing
                                     : 0.5 * timing + 0.5 * ratio(acceptedTicks, target.ticks);
        }
        return 0.0;
    }

#if defined(_DEBUG)
    std::wstring NoteAccuracy::DebugText() const
    {
        std::wstring result;
        for (std::size_t index = 0; index < hitScoreRates.size(); ++index)
        {
            result += std::format(L"Hit{}={:.2f}%{}  ", index + 1, hitScoreRates[index] * 100.0,
                                  index < acceptedHits ? L"" : L" (unfilled)");
        }
        if (target.kind == NoteAccuracyKind::HitCount)
        {
            result += std::format(L"Hits={}/{}  ", acceptedHits, target.hits);
        }
        if (target.kind == NoteAccuracyKind::Hold || target.kind == NoteAccuracyKind::Ticks)
        {
            const double ratio =
                target.ticks == 0 ? 0.0 : 100.0 * static_cast<double>(acceptedTicks) / target.ticks;
            result +=
                std::format(L"Ticks={}/{} ({:.2f}%){}  ", acceptedTicks, target.ticks, ratio,
                            target.kind == NoteAccuracyKind::Hold ? L" [head/ticks 50:50]" : L"");
        }
        return result +
               std::format(L"{}={:.2f}%", finalized ? L"Final" : L"Current", ScoreRate() * 100.0);
    }
#endif
} // namespace finger_drum::rhythm
