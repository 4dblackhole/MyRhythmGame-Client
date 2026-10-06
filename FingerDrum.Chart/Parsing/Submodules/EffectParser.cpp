#include "Automation/InterpolationExpression.h"
#include "Parsing/Submodules/ParserSupport.h"
#include <set>

namespace finger_drum::chart
{
using namespace parsing;
namespace
{
MusicalPosition Position(std::string_view measureText, std::string_view fraction)
{
    std::int64_t measure{};
    MusicalPosition position;
    if (!ParseInteger(measureText, measure) || measure < 1 || !TryParseMusicalPosition(fraction, measure - 1, position))
        throw std::invalid_argument("Expected a one-based measure and N/D position.");
    return position;
}
void SetCommandKind(EffectCommand &c, std::string_view section, std::string_view commandText)
{
    std::istringstream tokens{std::string(commandText)};
    std::string name, extra;
    tokens >> name >> c.target;
    if (tokens >> extra)
        throw std::invalid_argument("Unexpected effect command argument.");
    if (section == "Speed" && name == "#ScrollSpeed" && (c.target == "Whole" || c.target == "Separate"))
        c.type = c.target == "Whole" ? EffectCommandType::ScrollSpeed : EffectCommandType::NoteSpeed;
    else if (section == "Sounds" && name == "#Volume")
        c.type = EffectCommandType::BusVolume;
    else if (section == "Sounds" && name == "#ReverbSend")
        c.type = EffectCommandType::ReverbSend;
    else if (section == "Sounds" && name == "#LowPassCutoff")
        c.type = EffectCommandType::LowPassCutoff;
    else if (section == "Sounds" && name == "#HighPassCutoff")
        c.type = EffectCommandType::HighPassCutoff;
    else if (section == "Zone" && name == "#Syncopation")
        c.type = EffectCommandType::SyncopationZone;
    else if (section == "Zone" && name == "#Kiai")
        c.type = EffectCommandType::Kiai;
    else if (section == "Zone" && name == "#MeasureLineVisible")
        c.type = EffectCommandType::MeasureLineVisible;
    else
        throw std::invalid_argument("Unknown command or wrong effect section.");
}
EffectCommand ParseCommand(std::string_view section, const std::vector<std::string_view> &fields)
{
    if (fields.size() < 3)
        throw std::invalid_argument("An effect requires measure, N/D and command.");
    EffectCommand c;
    c.position = Position(fields[0], fields[1]);
    std::size_t commandIndex = 2;
    if (fields[2] == "Area")
    {
        if (fields.size() < 6)
            throw std::invalid_argument("Area requires end measure, N/D and command.");
        c.endPosition = Position(fields[3], fields[4]);
        if (*c.endPosition <= c.position)
            throw std::invalid_argument("Area end must follow its start.");
        commandIndex = 5;
    }
    SetCommandKind(c, section, fields[commandIndex]);
    const bool flag = c.type == EffectCommandType::SyncopationZone || c.type == EffectCommandType::Kiai;
    if (flag)
    {
        if (c.endPosition || (c.target != "ON" && c.target != "OFF"))
            throw std::invalid_argument("Zone state requires a point and ON/OFF.");
        c.beginValue = c.endValue = c.target == "ON" ? 1 : 0;
        c.target.clear();
    }
    else if (c.type == EffectCommandType::BusVolume || c.type == EffectCommandType::ReverbSend ||
             c.type == EffectCommandType::LowPassCutoff || c.type == EffectCommandType::HighPassCutoff)
    {
        if (c.type == EffectCommandType::BusVolume && c.target != "HitSound" && c.target != "TickSound" &&
            c.target != "UserInputFeedback")
            throw std::invalid_argument("Volume targets hit sounds: HitSound, "
                                        "TickSound or UserInputFeedback.");
        if (c.target != "Music" && c.target != "HitSound" && c.target != "TickSound" &&
            c.target != "UserInputFeedback" && c.target != "UI")
            throw std::invalid_argument("Unknown audio bus.");
    }
    else if (c.type == EffectCommandType::MeasureLineVisible && !c.target.empty())
        throw std::invalid_argument("MeasureLineVisible has no target.");
    std::set<std::string> seen;
    for (std::size_t i = commandIndex + 1; i < fields.size(); ++i)
    {
        const auto equal = fields[i].find('=');
        if (equal == std::string_view::npos)
            throw std::invalid_argument("Effect options require Name=Value.");
        const auto key = std::string(Trim(fields[i].substr(0, equal)));
        const auto value = Trim(fields[i].substr(equal + 1));
        if (!seen.insert(key).second)
            throw std::invalid_argument("Duplicate effect option.");
        if (key == "Exclude" && c.type == EffectCommandType::SyncopationZone && c.endValue == 1)
        {
            for (const auto item : Split(value, '|'))
            {
                std::size_t division{};
                if (!ParseInteger(item, division) || !division || division > 1024)
                    throw std::invalid_argument("Exclude requires beat divisors 1..1024 separated by |.");
                c.excludedDivisions.push_back(division);
            }
        }
        else if (key == "Curve" && c.endPosition)
            c.curveName = std::string(value);
        else if (!flag && ((key == "Value" && !c.endPosition) || ((key == "From" || key == "To") && c.endPosition)))
        {
            double number{};
            if (!ParseDouble(value, number) || !std::isfinite(number))
                throw std::invalid_argument("Effect value must be finite.");
            if (key == "To")
                c.endValue = number;
            else
                c.beginValue = number;
            if (key == "Value")
                c.endValue = number;
        }
        else
            throw std::invalid_argument("Unsupported option for this effect.");
    }
    if (!flag && ((!c.endPosition && !seen.contains("Value")) ||
                  (c.endPosition && (!seen.contains("From") || !seen.contains("To") || !seen.contains("Curve")))))
        throw std::invalid_argument("Point requires Value; Area requires From, To and Curve.");
    if ((c.type == EffectCommandType::ScrollSpeed || c.type == EffectCommandType::NoteSpeed) &&
        (c.beginValue <= 0 || c.endValue <= 0))
        throw std::invalid_argument("Scroll speed must be positive.");
    if (c.type == EffectCommandType::BusVolume && (c.beginValue < 0 || c.endValue < 0))
        throw std::invalid_argument("Volume cannot be negative.");
    return c;
}
void ResolveCurves(EffectDocument &document, const CompiledInterpolations &expressions,
                   std::vector<Diagnostic> &diagnostics, const std::filesystem::path &source)
{
    // Resolve curves after all sections, allowing definitions after uses.
    for (auto &c : document.commands)
    {
        if (!c.endPosition)
            continue;
        try
        {
            ResolveInterpolation(c, expressions);
            if ((c.curve == AutomationCurve::Exponential || c.curve == AutomationCurve::Harmonic) &&
                (c.beginValue <= 0 || c.endValue <= 0))
                throw std::invalid_argument("Exponential/Harmonic endpoints must be positive.");
            for (int i = 0; i <= 128; ++i)
            {
                const double value = EvaluateInterpolation(c, i / 128.0);
                if (!std::isfinite(value) ||
                    ((c.type == EffectCommandType::ScrollSpeed || c.type == EffectCommandType::NoteSpeed) &&
                     value <= 0) ||
                    (c.type == EffectCommandType::BusVolume && value < 0))
                    throw std::invalid_argument("Interpolation produces an invalid effect value.");
            }
        }
        catch (const std::exception &error)
        {
            AddDiagnostic(diagnostics, source, c.source.line, error.what());
        }
    }
}

} // namespace
ParseResult<EffectDocument> ChartParser::ParseEffect(std::string_view utf8, std::filesystem::path source) const
{
    ParseResult<EffectDocument> result;
    auto &document = result.document;
    document.sourcePath = std::move(source);
    const auto &sourcePath = result.document.sourcePath;
    CompiledInterpolations expressions;
    std::string section;
    for (const auto &[lineNumber, raw] : EnumerateLines(utf8))
    {
        const auto line = Trim(raw);
        if (line.empty() || StartsWithInsensitive(line, "//"))
            continue;
        try
        {
            if (line.front() == '[' && line.back() == ']')
            {
                section = std::string(Trim(line.substr(1, line.size() - 2)));
                if (section != "Interpolation" && section != "HitSounds" && section != "Speed" && section != "Sounds" &&
                    section != "Zone")
                    throw std::invalid_argument("Unknown YME section.");
                continue;
            }
            const auto [key, value] = SplitKeyValue(line);
            if (section.empty() && key == "Version")
            {
                if (!ParseInteger(value, document.version) || document.version != 1)
                    throw std::invalid_argument("YME Version must be 1.");
            }
            else if (section == "HitSounds")
            {
                if (key.empty() || value.empty() ||
                    !document.hitSounds.emplace(std::string(key), PathFromUtf8(value)).second)
                    throw std::invalid_argument("A hit sound requires a unique index and path.");
            }
            else if (section == "Interpolation")
            {
                if (key.empty() || key == "Linear" || key == "Exponential" || key == "Harmonic" || value.empty() ||
                    !document.interpolations.emplace(std::string(key), std::string(value)).second)
                    throw std::invalid_argument("An interpolation requires a unique "
                                                "non-reserved name and expression.");
                expressions.emplace(std::string(key), std::make_shared<const InterpolationExpression>(value));
            }
            else if (section == "Speed" || section == "Sounds" || section == "Zone")
            {
                const auto fields = Split(line, ',');
                if (section == "Sounds" && fields.size() == 3 && fields[2].starts_with("#HitSound "))
                {
                    HitSoundChange c;
                    c.position = Position(fields[0], fields[1]);
                    std::istringstream tokens{std::string(fields[2])};
                    std::string command, target, extra;
                    tokens >> command >> target >> c.soundIndex;
                    if (command != "#HitSound" || (target != "Don" && target != "Kat") || c.soundIndex.empty() ||
                        (tokens >> extra))
                        throw std::invalid_argument("HitSound requires Don/Kat and a table index.");
                    c.keyType = target == "Don" ? 1 : 2;
                    c.source = {sourcePath, lineNumber, 1};
                    document.hitSoundChanges.push_back(std::move(c));
                }
                else
                {
                    auto c = ParseCommand(section, fields);
                    c.source = {sourcePath, lineNumber, 1};
                    document.commands.push_back(std::move(c));
                }
            }
            else
                throw std::invalid_argument("Expected a YME section.");
        }
        catch (const std::exception &error)
        {
            AddDiagnostic(result.diagnostics, sourcePath, lineNumber, error.what());
        }
    }
    ResolveCurves(document, expressions, result.diagnostics, sourcePath);
    for (const auto &c : document.hitSoundChanges)
        if (!document.hitSounds.contains(c.soundIndex))
            AddDiagnostic(result.diagnostics, sourcePath, c.source.line, "Undefined hit sound table index.");
    return result;
}
} // namespace finger_drum::chart
