#include "Editing/ChartEditor.h"
#include "Parsing/ChartParser.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>

namespace finger_drum::chart
{
namespace
{
std::string Fraction(const Rational &value)
{
    return std::to_string(value.Numerator()) + "/" + std::to_string(value.Denominator());
}

std::string Utf8(const std::filesystem::path &path)
{
    const auto value = path.generic_u8string();
    return {reinterpret_cast<const char *>(value.data()), value.size()};
}

void Advance(std::ostream &out, std::int64_t &measure, std::int64_t destination,
             const std::vector<std::int64_t> &breaks = {})
{
    while (measure < destination)
    {
        ++measure;
        out << (std::ranges::find(breaks, measure) != breaks.end() ? "---\n" : "--\n");
    }
}

void WriteFile(const std::filesystem::path &path, const std::string &text)
{
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.exceptions(std::ios::badbit | std::ios::failbit);
    file.write(text.data(), static_cast<std::streamsize>(text.size()));
    file.close();
}

void Validate(const PatternDocument &pattern, const EffectDocument &effects)
{
    MusicalTimeline timeline(pattern);
    const auto validPosition = [&timeline](MusicalPosition p) {
        return p.measure >= 0 && p.fraction >= Rational{} && p.fraction < timeline.MeasureLength(p.measure);
    };
    for (const auto &note : pattern.notes)
        if (!validPosition(note.position))
            throw std::invalid_argument("Note is outside its measure.");
    for (const auto &directive : pattern.timing)
        if (!validPosition(directive.position))
            throw std::invalid_argument("Move the out-of-measure timing directive before changing this signature.");
    for (const auto &change : effects.hitSoundChanges)
        if (!validPosition(change.position) || !pattern.hitSounds.contains(change.soundIndex) ||
            (change.keyType != 1 && change.keyType != 2))
            throw std::invalid_argument("Invalid hit sound change position, index or key ID.");
    for (const auto &command : effects.commands)
    {
        if (!validPosition(command.position) ||
            (command.endPosition && (!validPosition(*command.endPosition) || *command.endPosition <= command.position)))
            throw std::invalid_argument("Effect starts outside its measure.");
        if (!std::isfinite(command.beginValue) || !std::isfinite(command.endValue) ||
            !std::isfinite(command.durationMilliseconds) || command.durationMilliseconds < 0)
            throw std::invalid_argument("Effect values must be finite and duration must be non-negative.");
        if ((command.type == EffectCommandType::NoteSpeed || command.type == EffectCommandType::ScrollSpeed) &&
            (command.beginValue <= 0 || command.endValue <= 0))
            throw std::invalid_argument("Speed must be positive.");
    }
}
} // namespace

ChartEditor::ChartEditor(PatternDocument pattern, EffectDocument effects)
    : pattern_(std::move(pattern)), effects_(std::move(effects)), timeline_(pattern_)
{
    Validate(pattern_, effects_);
    Rebuild();
}

void ChartEditor::Rebuild()
{
    std::ranges::stable_sort(pattern_.notes, {}, &PatternNote::position);
    for (std::size_t i = 0; i < pattern_.notes.size(); ++i)
        pattern_.notes[i].sourceOrder = i;
    timeline_ = MusicalTimeline(pattern_);
    notes_ = timeline_.CompileNotes(pattern_);
    for (auto &note : notes_)
    {
        note.scrollMultiplier = timeline_.EffectValueAt(effects_, EffectCommandType::NoteSpeed, note.note.position) *
                                timeline_.EffectValueAt(effects_, EffectCommandType::ScrollSpeed, note.note.position);
        if (!std::isfinite(note.scrollMultiplier) || note.scrollMultiplier <= 0)
            throw std::invalid_argument("Combined note speed must be positive and finite.");
    }
}

void ChartEditor::Replace(PatternDocument pattern, EffectDocument effects)
{
    Validate(pattern, effects);
    // Construct caches before publishing the edit, preserving the old chart on error.
    ChartEditor replacement(std::move(pattern), std::move(effects));
    replacement.dirty_ = true;
    replacement.revision_ = revision_ + 1;
    *this = std::move(replacement);
}

void ChartEditor::AddNote(MusicalPosition position, int keyType, std::optional<MusicalPosition> end,
                          std::vector<std::string> extra)
{
    if (end && *end <= position)
        throw std::invalid_argument("Long note end must follow its start.");
    auto pattern = pattern_;
    pattern.notes.push_back({position, keyType, end ? 1 : 0, {}, std::move(extra)});
    if (end)
        pattern.notes.push_back({*end, keyType, 2});
    Replace(std::move(pattern), effects_);
}

void ChartEditor::DeleteNote(std::size_t sourceOrder)
{
    if (sourceOrder >= pattern_.notes.size())
        return;
    auto pattern = pattern_;
    std::optional<std::size_t> head;
    std::optional<std::size_t> partner;
    for (std::size_t i = 0; i < pattern.notes.size(); ++i)
    {
        const auto &note = pattern.notes[i];
        if (note.actionType == 1 && !head)
            head = i;
        else if (note.actionType == 2 && head && pattern.notes[*head].keyType == note.keyType)
        {
            if (*head == sourceOrder)
                partner = i;
            if (i == sourceOrder)
                partner = *head;
            head.reset();
        }
    }
    std::erase_if(pattern.notes, [&](const PatternNote &n) {
        return n.sourceOrder == sourceOrder || (partner && n.sourceOrder == *partner);
    });
    Replace(std::move(pattern), effects_);
}

void ChartEditor::SetMeasureLength(std::int64_t measure, Rational length)
{
    if (measure < 0 || length <= Rational{})
        throw std::invalid_argument("Invalid measure length.");
    auto pattern = pattern_;
    std::erase_if(pattern.timing, [measure](const TimingDirective &d) {
        return d.type == TimingDirectiveType::MeasureLength && d.position.measure == measure;
    });
    pattern.timing.push_back({{measure, {}}, TimingDirectiveType::MeasureLength, 0, length});
    const MusicalTimeline changed(pattern);
    for (auto &note : pattern.notes)
    {
        if (note.position.fraction >= changed.MeasureLength(note.position.measure))
            note.position = changed.PositionAtWholeNotes(changed.PositionToWholeNotes(note.position));
    }
    // Carry endpoints independently, but never silently save a reversed or
    // zero-length pair. Keep the original document when the edit is invalid.
    std::optional<std::size_t> head;
    for (std::size_t i = 0; i < pattern.notes.size(); ++i)
    {
        const auto &note = pattern.notes[i];
        if (note.actionType == 1 && !head)
            head = i;
        else if (note.actionType == 2 && head && pattern.notes[*head].keyType == note.keyType)
        {
            if (note.position <= pattern.notes[*head].position)
                throw std::invalid_argument(
                    "Measure change would reverse/collapse a long note. Move its endpoints first.");
            head.reset();
        }
    }
    Replace(std::move(pattern), effects_);
}

std::string ChartEditor::WritePattern(const PatternDocument &pattern)
{
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(17) << "Version: " << pattern.version << "\n[Metadata]\n"
        << "Music metadata: " << Utf8(pattern.musicMetadataFile) << '\n'
        << "Pattern Name: " << pattern.name << '\n';
    for (std::size_t i = 0; i < pattern.makers.size(); ++i)
        out << "Pattern Maker " << i << ": " << pattern.makers[i] << '\n';
    out << "Tags: ";
    for (std::size_t i = 0; i < pattern.tags.size(); ++i)
        out << (i ? ", " : "") << pattern.tags[i];
    out << "\nPattern Offset: " << pattern.patternOffsetMilliseconds << "\nBase BPM: " << pattern.baseBpm
        << "\n[Difficulty]\nMode: " << pattern.mode << "\nJudgeLevel: " << pattern.judgementLevel << "\n[HitSounds]\n";
    for (const auto &[index, path] : pattern.hitSounds)
        out << index << ": " << Utf8(path) << '\n';
    out << "[Time Signature]\n";
    auto timing = pattern.timing;
    std::ranges::stable_sort(timing, {}, &TimingDirective::position);
    std::int64_t measure = 0;
    for (const auto &d : timing)
    {
        Advance(out, measure, d.position.measure);
        if (d.type == TimingDirectiveType::MeasureLength)
            out << "#measure " << Fraction(d.ratio) << '\n';
        else
            out << Fraction(d.position.fraction) << ", " << (d.type == TimingDirectiveType::Bpm ? "#bpm " : "#delay ")
                << d.value << '\n';
    }
    out << "[Pattern]\n";
    measure = 0;
    auto notes = pattern.notes;
    std::ranges::stable_sort(notes, {}, &PatternNote::position);
    for (const auto &n : notes)
    {
        Advance(out, measure, n.position.measure, pattern.systemBreakMeasures);
        out << Fraction(n.position.fraction) << ", " << n.keyType << ", " << n.actionType << ", " << n.hitSound;
        for (const auto &extra : n.extraData)
            out << ", " << extra;
        out << '\n';
    }
    if (!pattern.systemBreakMeasures.empty())
        Advance(out, measure, *std::ranges::max_element(pattern.systemBreakMeasures), pattern.systemBreakMeasures);
    return out.str();
}

std::string ChartEditor::WriteEffects(const EffectDocument &effects)
{
    static constexpr const char *Names[]{"ScrollSpeed",     "NoteSpeed",     "BusVolume",
                                         "ReverbSend",      "LowPassCutoff", "HighPassCutoff",
                                         "SyncopationZone", "Custom",        "MeasureLineVisible"};
    static constexpr const char *Curves[]{"Step", "Linear", "Smoothstep", "Exponential"};
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(17) << "Version: " << effects.version << "\n[Effects]\n";
    auto commands = effects.commands;
    std::ranges::stable_sort(commands, {}, &EffectCommand::position);
    std::int64_t measure = 0;
    for (const auto &c : commands)
    {
        Advance(out, measure, c.position.measure);
        const std::string name = c.type == EffectCommandType::Custom && !c.customCommand.empty()
                                     ? c.customCommand
                                     : "#" + std::string(Names[static_cast<unsigned>(c.type)]);
        out << Fraction(c.position.fraction) << ", " << name << ", " << c.target << ", " << c.beginValue << ", "
            << c.endValue << ", " << c.durationMilliseconds << ", " << Curves[static_cast<unsigned>(c.curve)];
        for (const auto &argument : c.arguments)
            out << ", " << argument;
        if (c.endPosition)
            out << ", End=" << c.endPosition->measure + 1 << ':' << Fraction(c.endPosition->fraction);
        out << '\n';
    }
    out << "[HitSound Changes]\n";
    for (const auto &c : effects.hitSoundChanges)
        out << c.position.measure + 1 << ", " << Fraction(c.position.fraction) << ", " << c.soundIndex << ", "
            << c.keyType << '\n';
    return out.str();
}

void ChartEditor::Save()
{
    if (pattern_.sourcePath.empty())
        throw std::runtime_error("No YMP save path.");
    auto effectPath = pattern_.sourcePath;
    effectPath.replace_extension(L".yme");
    const std::string patternText = WritePattern(pattern_);
    const std::string effectText = WriteEffects(effects_);
    ChartParser parser;
    const auto parsedPattern = parser.ParsePattern(patternText);
    const auto parsedEffects = parser.ParseEffect(effectText);
    if (!parsedPattern.Succeeded() || !parsedEffects.Succeeded() ||
        WritePattern(parsedPattern.document) != patternText || WriteEffects(parsedEffects.document) != effectText)
        throw std::runtime_error("Serialized chart failed validation; originals were not modified.");

    // Stage both files first and retain recoverable backups if any replace
    // fails. Existing backups are never overwritten.
    auto patternTemp = pattern_.sourcePath;
    patternTemp += L".editor.tmp";
    auto effectTemp = effectPath;
    effectTemp += L".editor.tmp";
    auto patternBackup = pattern_.sourcePath;
    patternBackup += L".editor.bak";
    auto effectBackup = effectPath;
    effectBackup += L".editor.bak";
    for (const auto &path : {patternTemp, effectTemp, patternBackup, effectBackup})
        if (std::filesystem::exists(path))
            throw std::runtime_error("Editor recovery files exist; recover/remove them before saving.");
    WriteFile(patternTemp, patternText);
    WriteFile(effectTemp, effectText);
    bool backedPattern = false, backedEffect = false, installedPattern = false, installedEffect = false;
    try
    {
        if (std::filesystem::exists(pattern_.sourcePath))
        {
            std::filesystem::rename(pattern_.sourcePath, patternBackup);
            backedPattern = true;
        }
        if (std::filesystem::exists(effectPath))
        {
            std::filesystem::rename(effectPath, effectBackup);
            backedEffect = true;
        }
        std::filesystem::rename(patternTemp, pattern_.sourcePath);
        installedPattern = true;
        std::filesystem::rename(effectTemp, effectPath);
        installedEffect = true;
    }
    catch (...)
    {
        std::error_code ignored;
        if (installedPattern)
            std::filesystem::remove(pattern_.sourcePath, ignored);
        if (installedEffect)
            std::filesystem::remove(effectPath, ignored);
        if (backedPattern)
            std::filesystem::rename(patternBackup, pattern_.sourcePath, ignored);
        if (backedEffect)
            std::filesystem::rename(effectBackup, effectPath, ignored);
        throw;
    }
    std::error_code ignored;
    if (backedPattern)
        std::filesystem::remove(patternBackup, ignored);
    if (backedEffect)
        std::filesystem::remove(effectBackup, ignored);
    effects_.sourcePath = effectPath;
    dirty_ = false;
}
} // namespace finger_drum::chart
