#pragma once
#include "Model/Submodules/CompiledChart.h"

namespace finger_drum::chart
{
// Cumulative Whole-scroll distance, compiled once; queries are binary searches.
class ScrollAutomation final
{
  public:
    explicit ScrollAutomation(const std::vector<CompiledEffectCommand> &commands = {});
    long double Distance(rhythm::RhythmTime target, rhythm::RhythmTime current) const;
    double MinimumMultiplier() const noexcept
    {
        return minimum_;
    }

  private:
    struct Piece
    {
        long double begin{}, end{}, cumulative{};
        double first{1}, middle{1}, last{1};
    };
    static long double Integral(const Piece &piece, long double progress);
    void Subdivide(const CompiledEffectCommand &, long double begin, long double end, unsigned depth);
    long double At(rhythm::RhythmTime) const;
    std::vector<Piece> pieces_;
    long double cumulative_{};
    long double firstTime_{}, lastTime_{};
    double lastValue_{1}, minimum_{1};
};
} // namespace finger_drum::chart
