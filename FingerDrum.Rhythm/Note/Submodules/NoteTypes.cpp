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

    namespace
    {
        template <typename Value> void AppendMoved(std::vector<Value> &target, std::vector<Value> &source)
        {
            if (target.empty() && target.capacity() == 0)
                target.swap(source); // First contribution transfers its allocation through Lane/Gear aggregation.
            else
                target.insert(target.end(), std::make_move_iterator(source.begin()), std::make_move_iterator(source.end()));
        }
    }

    void NoteProcessResult::Append(NoteProcessResult other)
    {
        AppendMoved(events, other.events);
        AppendMoved(audioCues, other.audioCues);
        AppendMoved(finalizedAccuracies, other.finalizedAccuracies);
    }

    AccuracyScore NoteAccuracy::Score() const noexcept
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
            return AccuracyScore::FromRate(timing);
        case NoteAccuracyKind::HitCount:
            return AccuracyScore::FromRate(ratio(acceptedHits, target.hits));
        case NoteAccuracyKind::Ticks:
            return AccuracyScore::FromRate(ratio(acceptedTicks, target.ticks));
        case NoteAccuracyKind::Hold:
            return AccuracyScore::FromRate(target.ticks == 0 ? timing
                                     : 0.5 * timing + 0.5 * ratio(acceptedTicks, target.ticks));
        }
        return {};
    }

#if defined(_DEBUG)
    std::wstring NoteAccuracy::DebugText() const
    {
        std::wstring result;
        for (std::size_t index = 0; index < hitScoreRates.size(); ++index)
        {
            result += std::format(L"Hit{}={:.6f}%{}  ", index + 1, hitScoreRates[index] * 100.0,
                                  index < acceptedHits ? L"" : L" (unfilled)");
        }
        if (target.kind == NoteAccuracyKind::HitCount)
        {
            result += std::format(L"Hits={}/{}  ", acceptedHits, target.hits);
        }
        if (target.kind == NoteAccuracyKind::Hold || target.kind == NoteAccuracyKind::Ticks)
        {
            const double ratio =
                target.ticks == 0 ? 0.0 : 100.0 * static_cast<double>(acceptedTicks) / static_cast<double>(target.ticks);
            result +=
                std::format(L"Ticks={}/{} ({:.6f}%){}  ", acceptedTicks, target.ticks, ratio,
                            target.kind == NoteAccuracyKind::Hold ? L" [head/ticks 50:50]" : L"");
        }
        return result +
               std::format(L"{}={:.6f}%", finalized ? L"Final" : L"Current", ScoreRate() * 100.0);
    }
#endif
} // namespace finger_drum::rhythm
