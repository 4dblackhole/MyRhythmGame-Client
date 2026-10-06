#include "Automation/InterpolationExpression.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <stdexcept>

namespace finger_drum::chart
{
void ResolveInterpolation(EffectCommand &command, const CompiledInterpolations &expressions)
{
    command.interpolation.reset();
    if (command.curveName == "Linear")
        command.curve = AutomationCurve::Linear;
    else if (command.curveName == "Exponential")
        command.curve = AutomationCurve::Exponential;
    else if (command.curveName == "Harmonic")
        command.curve = AutomationCurve::Harmonic;
    else
    {
        const auto definition = expressions.find(command.curveName);
        if (definition == expressions.end())
            throw std::invalid_argument("Undefined interpolation function.");
        command.curve = AutomationCurve::Expression;
        command.interpolation = definition->second;
    }
}
namespace
{
double Cubic(double t, double a, double b)
{
    const double v = 1 - t;
    return 3 * v * v * t * a + 3 * v * t * t * b + t * t * t;
}
double Bezier(double u, double x1, double y1, double x2, double y2)
{
    if (x1 < 0 || x1 > 1 || x2 < 0 || x2 > 1)
        throw std::invalid_argument("Bezier control point X must be in [0,1].");
    if (u <= 0 || u >= 1)
        return u;
    double low = 0, high = 1;
    for (int i = 0; i < 48; ++i)
    {
        const double middle = (low + high) * .5;
        if (Cubic(middle, x1, x2) < u)
            low = middle;
        else
            high = middle;
    }
    return Cubic((low + high) * .5, y1, y2);
}
} // namespace
InterpolationExpression::InterpolationExpression(std::string_view expression) : source_(expression)
{
    if (source_.empty() || source_.size() > 4096)
        throw std::invalid_argument("Interpolation expression is empty or too long.");
    Sum();
    Take(' ');
    if (cursor_ != source_.size())
        throw std::invalid_argument("Unexpected interpolation token.");
}
bool InterpolationExpression::Take(char character)
{
    while (cursor_ < source_.size() && std::isspace(static_cast<unsigned char>(source_[cursor_])))
        ++cursor_;
    if (cursor_ == source_.size() || source_[cursor_] != character)
        return false;
    ++cursor_;
    return true;
}
void InterpolationExpression::Emit(Op op, double value)
{
    if (code_.size() >= 256)
        throw std::invalid_argument("Interpolation expression is too complex.");
    code_.push_back({op, value});
}
void InterpolationExpression::Sum()
{
    Product();
    for (;;)
    {
        if (Take('+'))
        {
            Product();
            Emit(Op::Add);
        }
        else if (Take('-'))
        {
            Product();
            Emit(Op::Subtract);
        }
        else
            break;
    }
}
void InterpolationExpression::Product()
{
    Unary();
    for (;;)
    {
        if (Take('*'))
        {
            Unary();
            Emit(Op::Multiply);
        }
        else if (Take('/'))
        {
            Unary();
            Emit(Op::Divide);
        }
        else
            break;
    }
}
void InterpolationExpression::Unary()
{
    if (++depth_ > 64)
        throw std::invalid_argument("Interpolation nesting is too deep.");
    if (Take('-'))
    {
        Unary();
        Emit(Op::Negate);
    }
    else if (Take('+'))
        Unary();
    else
        Power();
    --depth_;
}
void InterpolationExpression::Power()
{
    Primary();
    if (Take('^'))
    {
        Unary();
        Emit(Op::Power);
    }
}
void InterpolationExpression::Primary()
{
    if (Take('('))
    {
        Sum();
        if (!Take(')'))
            throw std::invalid_argument("Missing interpolation closing parenthesis.");
        return;
    }
    if (cursor_ < source_.size() &&
        (std::isdigit(static_cast<unsigned char>(source_[cursor_])) || source_[cursor_] == '.'))
    {
        char *end{};
        const double value = std::strtod(source_.c_str() + cursor_, &end);
        if (end == source_.c_str() + cursor_ || !std::isfinite(value))
            throw std::invalid_argument("Invalid interpolation number.");
        cursor_ = static_cast<std::size_t>(end - source_.c_str());
        Emit(Op::Number, value);
        return;
    }
    const auto begin = cursor_;
    while (cursor_ < source_.size() &&
           (std::isalnum(static_cast<unsigned char>(source_[cursor_])) || source_[cursor_] == '_'))
        ++cursor_;
    const auto name = source_.substr(begin, cursor_ - begin);
    if (name == "u")
        Emit(Op::Progress);
    else if (name == "From")
        Emit(Op::From);
    else if (name == "To")
        Emit(Op::To);
    else if (name == "Bezier" && Take('('))
    {
        for (int i = 0; i < 5; ++i)
        {
            if (i && !Take(','))
                throw std::invalid_argument("Bezier requires five arguments.");
            Sum();
        }
        if (!Take(')'))
            throw std::invalid_argument("Bezier requires five arguments.");
        Emit(Op::Bezier);
    }
    else
        throw std::invalid_argument("Unknown interpolation variable/function: " + name);
}
double InterpolationExpression::Evaluate(double progress, double from, double to) const
{
    std::array<double, 256> stack{};
    std::size_t count{};
    for (const auto instruction : code_)
    {
        switch (instruction.op)
        {
        case Op::Number:
            stack[count++] = instruction.number;
            break;
        case Op::Progress:
            stack[count++] = progress;
            break;
        case Op::From:
            stack[count++] = from;
            break;
        case Op::To:
            stack[count++] = to;
            break;
        case Op::Negate:
            stack[count - 1] = -stack[count - 1];
            break;
        case Op::Bezier:
            count -= 4;
            stack[count - 1] =
                Bezier(stack[count - 1], stack[count], stack[count + 1], stack[count + 2], stack[count + 3]);
            break;
        default:
            const double right = stack[--count];
            double &left = stack[count - 1];
            switch (instruction.op)
            {
            case Op::Add:
                left += right;
                break;
            case Op::Subtract:
                left -= right;
                break;
            case Op::Multiply:
                left *= right;
                break;
            case Op::Divide:
                left /= right;
                break;
            case Op::Power:
                left = std::pow(left, right);
                break;
            default:
                break;
            }
        }
        if (!count || !std::isfinite(stack[count - 1]))
            throw std::invalid_argument("Interpolation result must be finite.");
    }
    return stack[0];
}
double EvaluateInterpolation(const EffectCommand &c, double u)
{
    u = std::clamp(u, 0.0, 1.0);
    switch (c.curve)
    {
    case AutomationCurve::Step:
        return u >= 1 ? c.endValue : c.beginValue;
    case AutomationCurve::Smoothstep:
        return std::lerp(c.beginValue, c.endValue, u * u * (3 - 2 * u));
    case AutomationCurve::Exponential:
        return std::pow(c.beginValue, 1 - u) * std::pow(c.endValue, u);
    case AutomationCurve::Harmonic:
        return c.beginValue * c.endValue / ((1 - u) * c.endValue + u * c.beginValue);
    case AutomationCurve::Expression:
        if (!c.interpolation)
            throw std::invalid_argument("Unresolved interpolation function.");
        return c.interpolation->Evaluate(u, c.beginValue, c.endValue);
    default:
        return std::lerp(c.beginValue, c.endValue, u);
    }
}
} // namespace finger_drum::chart
