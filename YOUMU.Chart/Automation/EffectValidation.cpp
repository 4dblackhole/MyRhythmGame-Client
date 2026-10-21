#include "Automation/EffectValidation.h"
#include "Automation/InterpolationExpression.h"
#include "Timing/MusicalTimeline.h"
#include <cmath>
#include <limits>
#include <stdexcept>

void ValidateEffectValue(EffectCommandType type, double value)
{
    if (!std::isfinite(value))
        throw std::invalid_argument("Effect value must be finite.");
    if ((type == EffectCommandType::ScrollSpeed || type == EffectCommandType::NoteSpeed) && value <= 0)
        throw std::invalid_argument("Scroll speed must be positive.");
    if (type == EffectCommandType::BusVolume && value < 0)
        throw std::invalid_argument("Volume cannot be negative.");
}

void ValidateEffectCommand(const EffectCommand &command)
{
    ValidateEffectValue(command.type, command.beginValue);
    ValidateEffectValue(command.type, command.endValue);
    if (!std::isfinite(command.durationMilliseconds) || command.durationMilliseconds < 0)
        throw std::invalid_argument("Effect duration must be finite and non-negative.");
    if (command.durationMilliseconds * 1000 >=
        static_cast<double>(std::numeric_limits<RhythmDuration::rep>::max()))
        throw std::invalid_argument("Effect duration exceeds the microsecond clock range.");
    if (command.type == EffectCommandType::BusVolume && command.target != "HitSound" &&
        command.target != "TickSound" && command.target != "UserInputFeedback")
        throw std::invalid_argument("Volume targets hit sounds, not Music/UI.");
    if (command.endPosition && *command.endPosition <= command.position)
        throw std::invalid_argument("Area end must follow its start.");
    if (command.endPosition &&
        (command.curve == AutomationCurve::Exponential || command.curve == AutomationCurve::Harmonic) &&
        (command.beginValue <= 0 || command.endValue <= 0))
        throw std::invalid_argument("Exponential/Harmonic endpoints must be positive.");
    if (command.endPosition)
    {
        // Sampling catches common authoring errors; runtime evaluation must
        // still guard unsampled singularities in user expressions.
        for (int i = 0; i <= 128; ++i)
            ValidateEffectValue(command.type, EvaluateInterpolation(command, i / 128.0));
    }
}

void ResolveEffectInterpolations(EffectDocument &document)
{
    CompiledInterpolations compiled;
    for (const auto &[name, expression] : document.interpolations)
        compiled.emplace(name, std::make_shared<const InterpolationExpression>(expression));
    for (auto &command : document.commands)
        if (command.endPosition && !command.curveName.empty())
            ResolveInterpolation(command, compiled);
}

void ValidateEffectDocument(const EffectDocument &document)
{
    for (const auto &command : document.commands)
        ValidateEffectCommand(command);
    for (const auto &change : document.hitSoundChanges)
        if (!document.hitSounds.contains(change.soundIndex) || (change.keyType != 1 && change.keyType != 2))
            throw std::invalid_argument("Invalid hit sound change index or key ID.");
}

void ValidateEffectPositions(const EffectDocument &document, const MusicalTimeline &timeline)
{
    const auto valid = [&timeline](MusicalPosition p)
    {
        return p.measure >= 0 && p.measure < MaximumMeasureCount && p.fraction >= Rational{} &&
               p.fraction < timeline.MeasureLength(p.measure);
    };
    for (const auto &command : document.commands)
        if (!valid(command.position) || (command.endPosition && !valid(*command.endPosition)))
            throw std::invalid_argument("Effect starts or ends outside its measure.");
    for (const auto &change : document.hitSoundChanges)
        if (!valid(change.position))
            throw std::invalid_argument("Hit sound change starts outside its measure.");
}
