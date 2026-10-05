#include "Editing/ChartEditor.h"
#include "Parsing/ChartParser.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <locale>
#include <limits>
#include <set>
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

void Validate(const PatternDocument &pattern, const EffectDocument &effects, const MusicalTimeline &timeline)
{
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
    Validate(pattern_, effects_, timeline_);
    InitializeEventTrees();
}

void ChartEditor::InitializeEventTrees()
{
    for (auto &directive : pattern_.timing)
        timingTree_.emplace(directive.position, std::move(directive));
    for (auto &command : effects_.commands)
    {
        const auto entry = effectTree_.emplace(command.position, std::move(command));
        effectIndex_.insert_or_assign({entry->second.type, entry->first}, &entry->second);
    }
    for (auto &change : effects_.hitSoundChanges)
        soundChangeTree_.emplace(change.position, std::move(change));
    // A conservative lower bound also covers grid points with no authored note.
    double noteMinimum = 1, scrollMinimum = 1;
    for (const auto &[position, command] : effectTree_)
    {
        const double value = std::min(command.beginValue, command.endValue);
        if (command.type == EffectCommandType::NoteSpeed)
            noteMinimum = std::min(noteMinimum, value);
        if (command.type == EffectCommandType::ScrollSpeed)
            scrollMinimum = std::min(scrollMinimum, value);
    }
    minimumScrollMultiplier_ = static_cast<double>(std::max(
        static_cast<long double>(noteMinimum) * scrollMinimum,
        static_cast<long double>(std::numeric_limits<double>::denorm_min())));
    pattern_.timing.clear();
    effects_.commands.clear();
    effects_.hitSoundChanges.clear();

    std::ranges::stable_sort(pattern_.notes, {}, &PatternNote::position);
    for (const auto &note : pattern_.notes)
    {
        if (note.sourceOrder == std::numeric_limits<std::size_t>::max())
            throw std::overflow_error("Note ID space exhausted.");
        nextNoteId_ = std::max(nextNoteId_, note.sourceOrder + 1);
    }
    std::set<std::size_t> assigned;
    for (auto &source : pattern_.notes)
    {
        if (!assigned.insert(source.sourceOrder).second)
        {
            if (nextNoteId_ == std::numeric_limits<std::size_t>::max())
                throw std::overflow_error("Note ID space exhausted.");
            source.sourceOrder = nextNoteId_++;
        }
        auto note = CompileNote(std::move(source));
        const auto id = note.note.sourceOrder;
        const NoteKey key{note.note.position, id};
        noteKeys_.emplace(id, key);
        timeIndex_.emplace(TimeKey{note.timing, id}, key);
        auto entry = noteTree_.emplace(key, std::move(note)).first;
        if (entry->second.note.actionType == 1 || entry->second.note.actionType == 2)
            longEndpoints_.emplace(key, id);
    }
    longPartners_ = BuildLongPartners();
    pattern_.notes.clear();
}

ChartEditor::ChartEditor(const ChartEditor &source) : ChartEditor(source.Pattern(), source.Effects())
{
    nextNoteId_ = source.nextNoteId_;
    audioSourceRevision_ = source.audioSourceRevision_;
    dirty_ = source.dirty_;
    revision_ = source.revision_;
}

ChartEditor &ChartEditor::operator=(const ChartEditor &source)
{
    if (this != &source)
    {
        ChartEditor replacement(source);
        *this = std::move(replacement);
    }
    return *this;
}

std::map<std::size_t, std::size_t> ChartEditor::BuildLongPartners(
    std::optional<std::size_t> excluded, std::optional<std::size_t> excludedPartner) const
{
    std::map<std::size_t, std::size_t> pairs;
    std::optional<std::size_t> head;
    // Match the serialized/playback endpoint order, including overlapping edits.
    // Ordinary notes never require this traversal.
    for (const auto &[key, id] : longEndpoints_)
    {
        if (id == excluded || id == excludedPartner)
            continue;
        const auto &note = noteTree_.at(key).note;
        if (note.actionType == 1 && !head)
            head = id;
        else if (note.actionType == 2 && head &&
                 noteTree_.at(noteKeys_.at(*head)).note.keyType == note.keyType)
        {
            pairs.emplace(*head, id);
            pairs.emplace(id, *head);
            head.reset();
        }
    }
    return pairs;
}

CompiledPatternNote ChartEditor::CompileNote(PatternNote note) const
{
    if (note.position.measure < 0 || note.position.fraction < Rational{} ||
        note.position.fraction >= timeline_.MeasureLength(note.position.measure))
        throw std::invalid_argument("Note is outside its measure.");
    const auto time = timeline_.Compile(note.position);
    const auto speed = EffectValueAt(EffectCommandType::NoteSpeed, note.position) *
                       EffectValueAt(EffectCommandType::ScrollSpeed, note.position);
    if (!std::isfinite(speed) || speed <= 0)
        throw std::invalid_argument("Combined note speed must be positive and finite.");
    return {std::move(note), time, speed};
}

const std::vector<TimingDirective> &ChartEditor::Timing() const
{
    if (timingDirty_)
    {
        std::vector<TimingDirective> snapshot;
        snapshot.reserve(timingTree_.size());
        for (const auto &[position, directive] : timingTree_)
            snapshot.push_back(directive);
        pattern_.timing = std::move(snapshot);
        timingDirty_ = false;
    }
    return pattern_.timing;
}

const PatternDocument &ChartEditor::Pattern() const
{
    static_cast<void>(Timing());
    if (patternNotesDirty_)
    {
        std::vector<PatternNote> snapshot;
        snapshot.reserve(noteTree_.size());
        for (const auto &[key, note] : noteTree_)
            snapshot.push_back(note.note);
        pattern_.notes = std::move(snapshot);
        patternNotesDirty_ = false;
    }
    return pattern_;
}

const EffectDocument &ChartEditor::Effects() const
{
    if (effectsDirty_)
    {
        std::vector<EffectCommand> commands;
        std::vector<HitSoundChange> changes;
        commands.reserve(effectTree_.size());
        changes.reserve(soundChangeTree_.size());
        for (const auto &[position, command] : effectTree_)
            commands.push_back(command);
        for (const auto &[position, change] : soundChangeTree_)
            changes.push_back(change);
        effects_.commands = std::move(commands);
        effects_.hitSoundChanges = std::move(changes);
        effectsDirty_ = false;
    }
    return effects_;
}

const std::vector<CompiledPatternNote> &ChartEditor::Notes() const
{
    if (notesDirty_)
    {
        std::vector<CompiledPatternNote> snapshot;
        snapshot.reserve(timeIndex_.size());
        for (const auto &[time, key] : timeIndex_)
            snapshot.push_back(noteTree_.at(key));
        notes_ = std::move(snapshot);
        notesDirty_ = false;
    }
    return notes_;
}

double ChartEditor::EffectValueAt(EffectCommandType type, MusicalPosition position, double fallback) const
{
    const auto next = effectIndex_.upper_bound({type, position});
    if (next == effectIndex_.begin() || std::prev(next)->first.first != type)
        return fallback;
    return timeline_.EffectValueAt(*std::prev(next)->second, position);
}

std::pair<rhythm::RhythmTime, rhythm::RhythmTime> ChartEditor::NoteTimeRange() const
{
    return timeIndex_.empty() ? std::pair{rhythm::RhythmTime{}, rhythm::RhythmTime{}}
        : std::pair{timeIndex_.begin()->first.first, timeIndex_.rbegin()->first.first};
}

std::vector<CompiledPatternNote> ChartEditor::IncludeLongNotePartners(
    std::map<std::size_t, const CompiledPatternNote *> selected) const
{
    for (auto it = selected.begin(); it != selected.end(); ++it)
        if (const auto partner = longPartners_.find(it->first); partner != longPartners_.end())
            selected.emplace(partner->second, &noteTree_.at(noteKeys_.at(partner->second)));
    std::vector<CompiledPatternNote> result;
    result.reserve(selected.size());
    for (const auto &[id, note] : selected)
        result.push_back(*note);
    std::ranges::sort(result, [](const auto &a, const auto &b) {
        return TimeKey{a.timing, a.note.sourceOrder} < TimeKey{b.timing, b.note.sourceOrder};
    });
    return result;
}

std::vector<CompiledPatternNote> ChartEditor::NotesInMeasures(std::int64_t begin, std::int64_t end) const
{
    if (end <= begin)
        return {};
    std::map<std::size_t, const CompiledPatternNote *> selected;
    for (auto it = noteTree_.lower_bound({{begin, {}}, 0});
         it != noteTree_.end() && it->first.first.measure < end; ++it)
        selected.emplace(it->first.second, &it->second);
    // Include intervals spanning the viewport even when both endpoints are outside.
    for (const auto &[head, tail] : longPartners_)
        if (noteTree_.at(noteKeys_.at(head)).note.actionType == 1)
        {
            const auto &a = noteTree_.at(noteKeys_.at(head));
            const auto &b = noteTree_.at(noteKeys_.at(tail));
            if (a.note.position.measure < end && b.note.position.measure >= begin)
                selected.emplace(head, &a);
        }
    return IncludeLongNotePartners(std::move(selected));
}

std::vector<CompiledPatternNote> ChartEditor::NotesInTimeRange(rhythm::RhythmTime begin, rhythm::RhythmTime end) const
{
    if (end < begin)
        return {};
    std::map<std::size_t, const CompiledPatternNote *> selected;
    for (auto it = timeIndex_.lower_bound({begin, 0}); it != timeIndex_.end() && it->first.first <= end; ++it)
        selected.emplace(it->first.second, &noteTree_.at(it->second));
    for (const auto &[head, tail] : longPartners_)
        if (noteTree_.at(noteKeys_.at(head)).note.actionType == 1)
        {
            const auto &a = noteTree_.at(noteKeys_.at(head));
            const auto &b = noteTree_.at(noteKeys_.at(tail));
            if (a.timing <= end && b.timing >= begin)
                selected.emplace(head, &a);
        }
    return IncludeLongNotePartners(std::move(selected));
}

void ChartEditor::InvalidateNoteSnapshots() noexcept
{
    patternNotesDirty_ = notesDirty_ = true;
    dirty_ = true;
    ++revision_;
}

void ChartEditor::Replace(PatternDocument pattern, EffectDocument effects)
{
    // Construct caches before publishing the edit, preserving the old chart on error.
    ChartEditor replacement(std::move(pattern), std::move(effects));
    const bool sourcesChanged = replacement.pattern_.musicMetadataFile != pattern_.musicMetadataFile ||
        replacement.pattern_.sourcePath != pattern_.sourcePath || replacement.pattern_.hitSounds != pattern_.hitSounds;
    replacement.audioSourceRevision_ = audioSourceRevision_ + sourcesChanged;
    replacement.nextNoteId_ = std::max(nextNoteId_, replacement.nextNoteId_);
    replacement.dirty_ = true;
    replacement.revision_ = revision_ + 1;
    *this = std::move(replacement);
}

void ChartEditor::AddNote(MusicalPosition position, int keyType, std::optional<MusicalPosition> end,
                          std::vector<std::string> extra)
{
    if (end && *end <= position)
        throw std::invalid_argument("Long note end must follow its start.");
    if (nextNoteId_ > std::numeric_limits<std::size_t>::max() - 2)
        throw std::overflow_error("Note ID space exhausted.");
    auto head = CompileNote({position, keyType, end ? 1 : 0, {}, std::move(extra), {}, nextNoteId_});
    std::optional<CompiledPatternNote> tail;
    if (end)
        tail = CompileNote({*end, keyType, 2, {}, {}, {}, nextNoteId_ + 1});
    const auto insert = [this](CompiledPatternNote note) {
        const auto id = note.note.sourceOrder;
        const NoteKey key{note.note.position, id};
        noteKeys_.emplace(id, key);
        noteTree_.emplace(key, std::move(note));
        timeIndex_.emplace(TimeKey{noteTree_.at(key).timing, id}, key);
        if (noteTree_.at(key).note.actionType != 0)
            longEndpoints_.emplace(key, id);
    };
    try
    {
        insert(std::move(head));
        if (tail)
        {
            insert(std::move(*tail));
            auto pairs = BuildLongPartners();
            longPartners_.swap(pairs);
        }
    }
    catch (...)
    {
        EraseNote(nextNoteId_);
        EraseNote(nextNoteId_ + 1);
        throw;
    }
    nextNoteId_ += end ? 2 : 1;
    InvalidateNoteSnapshots();
}

void ChartEditor::EraseNote(std::size_t id) noexcept
{
    const auto key = noteKeys_.find(id);
    if (key == noteKeys_.end())
        return;
    const auto note = noteTree_.find(key->second);
    if (note != noteTree_.end())
    {
        timeIndex_.erase({note->second.timing, id});
        noteTree_.erase(note);
    }
    longEndpoints_.erase(key->second);
    noteKeys_.erase(key);
    longPartners_.erase(id);
}

void ChartEditor::DeleteNote(std::size_t sourceOrder)
{
    if (!noteKeys_.contains(sourceOrder))
        return;
    std::optional<std::size_t> partner;
    if (const auto pair = longPartners_.find(sourceOrder); pair != longPartners_.end())
        partner = pair->second;
    const bool longNote = longEndpoints_.contains(noteKeys_.at(sourceOrder));
    auto pairs = longNote ? BuildLongPartners(sourceOrder, partner) : std::map<std::size_t, std::size_t>{};
    if (partner)
        EraseNote(*partner);
    EraseNote(sourceOrder);
    if (longNote)
        longPartners_.swap(pairs);
    InvalidateNoteSnapshots();
}

void ChartEditor::SetMeasureLength(std::int64_t measure, Rational length)
{
    if (measure < 0 || length <= Rational{})
        throw std::invalid_argument("Invalid measure length.");
    auto pattern = Pattern();
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
    Replace(std::move(pattern), Effects());
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
    const std::string patternText = WritePattern(Pattern());
    const std::string effectText = WriteEffects(Effects());
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
