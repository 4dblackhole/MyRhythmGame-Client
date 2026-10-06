#pragma once
#include "Model/Submodules/EffectDocument.h"

namespace finger_drum::chart
{
// Parsed once when loading a YME; evaluation never reparses or allocates.
class InterpolationExpression final
{
  public:
    explicit InterpolationExpression(std::string_view expression);
    double Evaluate(double progress, double from, double to) const;

  private:
    enum class Op
    {
        Number,
        Progress,
        From,
        To,
        Add,
        Subtract,
        Multiply,
        Divide,
        Power,
        Negate,
        Bezier
    };
    struct Instruction
    {
        Op op;
        double number{};
    };
    void Sum();
    void Product();
    void Unary();
    void Power();
    void Primary();
    bool Take(char character);
    void Emit(Op op, double value = 0);
    std::string source_;
    std::size_t cursor_{}, depth_{};
    std::vector<Instruction> code_;
};
double EvaluateInterpolation(const EffectCommand &command, double progress);
using CompiledInterpolations = std::map<std::string, std::shared_ptr<const InterpolationExpression>, std::less<>>;
void ResolveInterpolation(EffectCommand &command, const CompiledInterpolations &expressions);
} // namespace finger_drum::chart
