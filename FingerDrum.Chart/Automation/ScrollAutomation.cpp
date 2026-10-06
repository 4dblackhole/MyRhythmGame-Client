#include "Automation/ScrollAutomation.h"
#include "Automation/InterpolationExpression.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace finger_drum::chart
{
long double ScrollAutomation::Integral(const Piece &p, long double x)
{
    // Integrate the quadratic through start/mid/end velocity samples.
    const long double a = 2 * (p.first - 2 * p.middle + p.last);
    const long double b = 4 * p.middle - 3 * p.first - p.last;
    return (p.end - p.begin) * x * (p.first + x * (b / 2 + x * a / 3));
}
ScrollAutomation::ScrollAutomation(const std::vector<CompiledEffectCommand> &commands)
{
    std::vector<CompiledEffectCommand> track;
    for (const auto &c : commands)
        if (c.command.type == EffectCommandType::ScrollSpeed)
            track.push_back(c);
    std::ranges::stable_sort(track, {}, &CompiledEffectCommand::timing);
    if (track.empty())
        return;
    firstTime_ = static_cast<long double>(track.front().timing.count());
    cumulative_ = firstTime_;
    for (std::size_t i = 0; i < track.size(); ++i)
    {
        const auto &c = track[i];
        minimum_ = std::min({minimum_, c.command.beginValue, c.command.endValue});
        const long double start = static_cast<long double>(c.timing.count());
        const long double duration = static_cast<long double>(c.duration.count());
        const long double next = i + 1 < track.size()
            ? static_cast<long double>(track[i + 1].timing.count()) : start + duration;
        const long double rampEnd = std::min(start + duration, next);
        if (rampEnd > start)
            Subdivide(c, start, rampEnd, 0);
        const long double holdBegin = std::max(start, rampEnd);
        if (next > holdBegin)
        {
            const double value = c.command.endValue;
            Piece p{holdBegin, next, cumulative_, value, value, value};
            pieces_.push_back(p);
            cumulative_ += Integral(p, 1);
            minimum_ = std::min(minimum_, value);
        }
        lastTime_ = next;
        lastValue_ = c.command.endValue;
    }
}
void ScrollAutomation::Subdivide(const CompiledEffectCommand &c, long double begin, long double end, unsigned depth)
{
    const auto value = [&c](long double time) {
        const double v =
            EvaluateInterpolation(c.command, static_cast<double>((time - c.timing.count()) / c.duration.count()));
        if (!std::isfinite(v) || v <= 0)
            throw std::invalid_argument("Whole scroll interpolation must remain positive and finite.");
        return v;
    };
    const long double mid = (begin + end) / 2;
    Piece p{begin, end, cumulative_, value(begin), value(mid), value(end)};
    Piece left{begin, mid, 0, p.first, value((begin + mid) / 2), p.middle};
    Piece right{mid, end, 0, p.middle, value((mid + end) / 2), p.last};
    const long double error = std::abs(Integral(p, 1) - Integral(left, 1) - Integral(right, 1));
    if (error > .001L && depth < 20)
    {
        Subdivide(c, begin, mid, depth + 1);
        Subdivide(c, mid, end, depth + 1);
        return;
    }
    if (pieces_.size() >= 65536)
        throw std::invalid_argument("Whole scroll curve exceeds the compilation limit.");
    pieces_.push_back(p);
    cumulative_ += Integral(p, 1);
    minimum_ = std::min({minimum_, p.first, p.middle, p.last, left.middle, right.middle});
}
long double ScrollAutomation::At(rhythm::RhythmTime time) const
{
    const long double t = static_cast<long double>(time.count());
    if (t <= firstTime_)
        return t;
    if (t >= lastTime_)
        return cumulative_ + (t - lastTime_) * lastValue_;
    const auto next = std::ranges::upper_bound(pieces_, t, {}, &Piece::begin);
    if (next == pieces_.begin())
        return t;
    const auto &p = *std::prev(next);
    return p.cumulative + Integral(p, (t - p.begin) / (p.end - p.begin));
}
long double ScrollAutomation::Distance(rhythm::RhythmTime target, rhythm::RhythmTime current) const
{
    return At(target) - At(current);
}
} // namespace finger_drum::chart
