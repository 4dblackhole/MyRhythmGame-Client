#include "Editing/ChartEditor.h"
#include "Automation/EffectValidation.h"
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
    if (destination < 0 || destination >= MaximumMeasureCount)
        throw std::invalid_argument("A chart may contain at most 100,000 measures.");
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

void WritePosition(std::ostream &out, MusicalPosition p)
{
    if (p.measure < 0 || p.measure >= MaximumMeasureCount)
        throw std::invalid_argument("An effect must use a measure in 0..99,999.");
    out << p.measure + 1 << ", " << Fraction(p.fraction);
}
std::string_view EffectSection(EffectCommandType type)
{
        if (type == EffectCommandType::ScrollSpeed || type == EffectCommandType::NoteSpeed) return "Speed";
        if (type == EffectCommandType::SyncopationZone || type == EffectCommandType::Kiai || type == EffectCommandType::MeasureLineVisible) return "Zone";
        return "Sounds";
}

void WriteEffect(std::ostream &out, const EffectCommand &c)
{
    WritePosition(out, c.position);
    if (c.endPosition) { out << ", Area, "; WritePosition(out, *c.endPosition); }
    out << ", #";
    switch (c.type)
    {
    case EffectCommandType::ScrollSpeed: out << "ScrollSpeed Whole"; break;
    case EffectCommandType::NoteSpeed: out << "ScrollSpeed Separate"; break;
    case EffectCommandType::BusVolume: out << "Volume " << c.target; break;
    case EffectCommandType::ReverbSend: out << "ReverbSend " << c.target; break;
    case EffectCommandType::LowPassCutoff: out << "LowPassCutoff " << c.target; break;
    case EffectCommandType::HighPassCutoff: out << "HighPassCutoff " << c.target; break;
    case EffectCommandType::MeasureLineVisible: out << "MeasureLineVisible"; break;
    case EffectCommandType::SyncopationZone: out << "Syncopation"; break;
    case EffectCommandType::Kiai: out << "Kiai"; break;
    default: throw std::invalid_argument("Unsupported YME effect.");
    }
    if (c.type == EffectCommandType::SyncopationZone || c.type == EffectCommandType::Kiai)
    {
        out << (c.endValue >= .5 ? " ON" : " OFF");
        if (!c.excludedDivisions.empty())
        {
            out << ", Exclude=";
            for (std::size_t i = 0; i < c.excludedDivisions.size(); ++i) out << (i ? "|" : "") << c.excludedDivisions[i];
        }
    }
    else if (c.endPosition)
    {
        const auto curve = !c.curveName.empty() ? c.curveName :
            c.curve == AutomationCurve::Linear ? "Linear" : c.curve == AutomationCurve::Exponential ? "Exponential" :
            c.curve == AutomationCurve::Harmonic ? "Harmonic" : "";
        if (curve.empty()) throw std::invalid_argument("Area requires a registered or built-in curve.");
        out << ", From=" << c.beginValue << ", To=" << c.endValue << ", Curve=" << curve;
    }
    else out << ", Value=" << c.endValue;
    out << '\n';
}

double MinimumEndpointSpeed(const auto &commands, const MusicalTimeline &timeline)
{
    double minimum = 1;
    for (const auto &[position, command] : commands)
        if (command.type == EffectCommandType::NoteSpeed && timeline.IsEffectActive(command))
            minimum = std::min({minimum, command.beginValue, command.endValue});
    return minimum;
}

double IndexedEffectValue(const auto &index, const MusicalTimeline &timeline, EffectCommandType type,
                          MusicalPosition position, double fallback = 1)
{
    const auto next = index.upper_bound({type, timeline.Compile(position)});
    if (next == index.begin() || std::prev(next)->first.first != type)
        return fallback;
    return timeline.EffectValueAt(*std::prev(next)->second, position);
}

double ValidatedNoteSpeed(double speed)
{
    if (!std::isfinite(speed) || speed <= 0)
        throw std::invalid_argument("Combined note speed must be positive and finite.");
    return speed;
}

bool IsValidPosition(MusicalPosition position, const MusicalTimeline &timeline)
{
    return position.measure >= 0 && position.measure < MaximumMeasureCount && position.fraction >= Rational{} &&
        position.fraction < timeline.MeasureLength(position.measure);
}

void ValidateNote(const PatternNote &note, const EffectDocument &effects, const MusicalTimeline &timeline)
{
    if (!IsValidPosition(note.position, timeline))
        throw std::invalid_argument("Note is outside its measure.");
    if (!note.hitSound.empty() && !effects.hitSounds.contains(note.hitSound))
        throw std::invalid_argument("Note references an undefined YME hit sound index.");
}

void Validate(const PatternDocument &pattern, const EffectDocument &effects, const MusicalTimeline &timeline)
{
    const auto validPosition = [&timeline](MusicalPosition p) {
        return IsValidPosition(p, timeline);
    };
    for (const auto &note : pattern.notes)
        ValidateNote(note, effects, timeline);
    for (const auto &directive : pattern.timing)
        if (!validPosition(directive.position) &&
            !(directive.type == TimingDirectiveType::DelayMilliseconds && directive.position.measure >= 0 &&
              directive.position.measure < MaximumMeasureCount &&
              directive.position.fraction == timeline.MeasureLength(directive.position.measure)))
            throw std::invalid_argument("Move the out-of-measure timing directive before changing this signature.");
    ValidateEffectDocument(effects);
    ValidateEffectPositions(effects, timeline);
}
} // namespace

ChartEditor::ChartEditor(PatternDocument pattern, EffectDocument effects)
    : pattern_(std::move(pattern)), effects_(std::move(effects)), timeline_(pattern_)
{
    ResolveEffectInterpolations(effects_);
    if (effects_.sourcePath.empty() && !pattern_.sourcePath.empty())
        effects_.sourcePath = pattern_.sourcePath.parent_path() / (pattern_.effectFile.empty()
            ? std::filesystem::path(pattern_.sourcePath.stem().wstring() + L".yme") : pattern_.effectFile);
    Validate(pattern_, effects_, timeline_);
    scroll_ = ScrollAutomation(timeline_.CompileEffects(effects_));
    InitializeEventTrees();
}

void ChartEditor::InitializeEventTrees()
{
    for (auto &directive : pattern_.timing)
        timingTree_.emplace(directive.position, std::move(directive));
    for (auto &command : effects_.commands)
    {
        const auto entry = effectTree_.emplace(command.position, std::move(command));
        if (timeline_.IsEffectActive(entry->second))
            effectIndex_.insert_or_assign({entry->second.type, timeline_.Compile(entry->first)}, &entry->second);
    }
    for (auto &change : effects_.hitSoundChanges)
        soundChangeTree_.emplace(change.position, std::move(change));
    // Endpoints bound built-in curves; the speed index also covers custom curves.
    minimumScrollMultiplier_ = MinimumEndpointSpeed(effectTree_, timeline_);
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
        noteSpeedIndex_.emplace(entry->second.scrollMultiplier, id);
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
    std::optional<std::size_t> excluded, std::optional<std::size_t> excludedPartner,
    std::span<const CompiledPatternNote> replacements) const
{
    std::map<std::size_t, std::size_t> pairs;
    std::optional<std::size_t> head;
    // Property edits stage only long endpoints. The existing add/delete paths
    // traverse their source tree directly and allocate no endpoint copy.
    decltype(longEndpoints_) changedEndpoints;
    const auto *endpoints = &longEndpoints_;
    if (!replacements.empty())
    {
        changedEndpoints = longEndpoints_;
        std::erase_if(changedEndpoints,
                      [&](const auto &entry) { return entry.second == excluded || entry.second == excludedPartner; });
        for (const auto &note : replacements)
            if (note.note.actionType != 0)
                changedEndpoints.emplace(NoteKey{note.note.position, note.note.sourceOrder}, note.note.sourceOrder);
        endpoints = &changedEndpoints;
    }
    const auto source = [&](std::size_t id) -> const PatternNote &
    {
        const auto changed = std::ranges::find(replacements, id, [](const auto &n) { return n.note.sourceOrder; });
        return changed != replacements.end() ? changed->note : noteTree_.at(noteKeys_.at(id)).note;
    };
    // Match the serialized/playback endpoint order, including overlapping edits.
    // Ordinary notes never require this traversal.
    for (const auto &[key, id] : *endpoints)
    {
        if (replacements.empty() && (id == excluded || id == excludedPartner))
            continue;
        const auto &note = source(id);
        if (note.actionType == 1 && !head)
            head = id;
        else if (note.actionType == 2 && head && source(*head).keyType == note.keyType)
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
    if (note.position.measure < 0 || note.position.measure >= MaximumMeasureCount || note.position.fraction < Rational{} ||
        note.position.fraction >= timeline_.MeasureLength(note.position.measure))
        throw std::invalid_argument("Note is outside its measure.");
    const auto time = timeline_.Compile(note.position);
    const auto speed = ValidatedNoteSpeed(EffectValueAt(EffectCommandType::NoteSpeed, note.position));
    const bool active = timeline_.IsPositionActive(note.position);
    return {std::move(note), time, speed, active};
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
    return IndexedEffectValue(effectIndex_, timeline_, type, position, fallback);
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
        replacement.pattern_.sourcePath != pattern_.sourcePath ||
        replacement.effects_.sourcePath != effects_.sourcePath || replacement.effects_.hitSounds != effects_.hitSounds;
    replacement.audioSourceRevision_ = audioSourceRevision_ + sourcesChanged;
    replacement.nextNoteId_ = std::max(nextNoteId_, replacement.nextNoteId_);
    replacement.dirty_ = true;
    replacement.revision_ = revision_ + 1;
    *this = std::move(replacement);
}

void ChartEditor::ReplaceTiming(std::vector<TimingDirective> timing)
{
    // Only the timing map and timestamp-dependent caches change. Note IDs,
    // musical-position trees, long-note pairs and sound sources keep their ownership.
    PatternDocument timingPattern;
    timingPattern.baseBpm = pattern_.baseBpm;
    timingPattern.patternOffsetMilliseconds = pattern_.patternOffsetMilliseconds;
    timingPattern.timing = std::move(timing);
    // Only the final source position is needed to bound the measure cache.
    if (!noteTree_.empty())
        timingPattern.notes.push_back({noteTree_.rbegin()->second.note.position});
    MusicalTimeline timeline(timingPattern);
    const auto &effects = Effects();
    Validate(timingPattern, effects, timeline);
    ScrollAutomation scroll(timeline.CompileEffects(effects));
    decltype(timingTree_) timingTree;
    for (const auto &directive : timingPattern.timing)
        timingTree.emplace(directive.position, directive);
    decltype(effectIndex_) effectsByTime;
    for (const auto &[position, command] : effectTree_)
        if (timeline.IsEffectActive(command))
            effectsByTime.insert_or_assign({command.type, timeline.Compile(position)}, &command);

    struct NoteUpdate
    {
        CompiledPatternNote *note;
        rhythm::RhythmTime time;
        double speed;
        bool active;
    };
    std::vector<NoteUpdate> updates;
    updates.reserve(noteTree_.size());
    decltype(timeIndex_) times;
    decltype(noteSpeedIndex_) speedIndex;
    for (auto &[key, note] : noteTree_)
    {
        ValidateNote(note.note, effects, timeline);
        const auto time = timeline.Compile(note.note.position);
        const auto speed = ValidatedNoteSpeed(
            IndexedEffectValue(effectsByTime, timeline, EffectCommandType::NoteSpeed, note.note.position));
        times.emplace(TimeKey{time, key.second}, key);
        speedIndex.emplace(speed, key.second);
        updates.push_back({&note, time, speed, timeline.IsPositionActive(note.note.position)});
    }

    // Publication performs no allocation; failed preparation leaves the edit untouched.
    timingTree_.swap(timingTree);
    timeIndex_.swap(times);
    noteSpeedIndex_.swap(speedIndex);
    effectIndex_.swap(effectsByTime);
    timeline_ = std::move(timeline);
    scroll_ = std::move(scroll);
    minimumScrollMultiplier_ = MinimumEndpointSpeed(effectTree_, timeline_);
    for (const auto &update : updates)
    {
        update.note->timing = update.time;
        update.note->scrollMultiplier = update.speed;
        update.note->positionActive = update.active;
    }
    timingDirty_ = notesDirty_ = dirty_ = true;
    ++revision_;
}

void ChartEditor::ReplaceEffects(EffectDocument effects)
{
    ResolveEffectInterpolations(effects);
    if (effects.sourcePath.empty())
        effects.sourcePath = effects_.sourcePath;
    Validate(Pattern(), effects, timeline_);
    ScrollAutomation scroll(timeline_.CompileEffects(effects));
    decltype(effectTree_) commands;
    decltype(effectIndex_) index;
    decltype(soundChangeTree_) changes;
    for (auto &command : effects.commands)
    {
        const auto entry = commands.emplace(command.position, std::move(command));
        if (timeline_.IsEffectActive(entry->second))
            index.insert_or_assign({entry->second.type, timeline_.Compile(entry->first)}, &entry->second);
    }
    for (auto &change : effects.hitSoundChanges)
        changes.emplace(change.position, std::move(change));
    std::vector<std::pair<CompiledPatternNote *, double>> speeds;
    decltype(noteSpeedIndex_) speedIndex;
    speeds.reserve(noteTree_.size());
    for (auto &[key, note] : noteTree_)
    {
        const auto speed = ValidatedNoteSpeed(IndexedEffectValue(index, timeline_, EffectCommandType::NoteSpeed, key.first));
        speeds.emplace_back(&note, speed);
        speedIndex.emplace(speed, key.second);
    }
    const bool sourcesChanged = effects.sourcePath != effects_.sourcePath || effects.hitSounds != effects_.hitSounds;

    // Swap the owning trees with their lookup index as one transaction. std::map
    // node addresses survive swap, so no pointers reference the old command tree.
    effectTree_.swap(commands);
    effectIndex_.swap(index);
    noteSpeedIndex_.swap(speedIndex);
    soundChangeTree_.swap(changes);
    effects_.version = effects.version;
    effects_.sourcePath.swap(effects.sourcePath);
    effects_.hitSounds.swap(effects.hitSounds);
    effects_.interpolations.swap(effects.interpolations);
    effects_.commands.clear();
    effects_.hitSoundChanges.clear();
    scroll_ = std::move(scroll);
    minimumScrollMultiplier_ = MinimumEndpointSpeed(effectTree_, timeline_);
    for (const auto &[note, speed] : speeds)
        note->scrollMultiplier = speed;
    effectsDirty_ = notesDirty_ = dirty_ = true;
    audioSourceRevision_ += sourcesChanged;
    ++revision_;
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
        noteSpeedIndex_.emplace(noteTree_.at(key).scrollMultiplier, id);
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
        noteSpeedIndex_.erase({note->second.scrollMultiplier, id});
        noteTree_.erase(note);
    }
    longEndpoints_.erase(key->second);
    noteKeys_.erase(key);
    longPartners_.erase(id);
}

std::optional<EditorNoteSelection> ChartEditor::FindNote(std::size_t id) const
{
    if (!noteKeys_.contains(id))
        return {};
    auto note = noteTree_.at(noteKeys_.at(id));
    if (note.note.actionType == 2)
    {
        const auto partner = longPartners_.find(id);
        if (partner == longPartners_.end())
            return {};
        id = partner->second;
        note = noteTree_.at(noteKeys_.at(id));
    }
    EditorNoteSelection result{std::move(note), {}};
    if (const auto partner = longPartners_.find(id); partner != longPartners_.end())
        result.tail = noteTree_.at(noteKeys_.at(partner->second));
    return result;
}

void ChartEditor::UpdateNote(std::size_t id, PatternNote head, std::optional<MusicalPosition> end)
{
    const auto selected = FindNote(id);
    if (!selected)
        throw std::out_of_range("Selected note no longer exists.");
    id = selected->head.note.sourceOrder;
    head.sourceOrder = id;
    head.source = selected->head.note.source;
    head.actionType = end ? 1 : 0;
    ValidateNote(head, Effects(), timeline_);
    std::vector<CompiledPatternNote> changed{CompileNote(std::move(head))};
    std::optional<std::size_t> oldTail;
    if (selected->tail)
        oldTail = selected->tail->note.sourceOrder;
    const bool needsId = end && !oldTail;
    if (needsId && nextNoteId_ == std::numeric_limits<std::size_t>::max())
        throw std::overflow_error("Note ID space exhausted.");
    if (end)
    {
        auto tail = selected->tail ? selected->tail->note : PatternNote{};
        tail.position = *end;
        tail.keyType = changed.front().note.keyType;
        tail.actionType = 2;
        tail.sourceOrder = oldTail.value_or(nextNoteId_);
        ValidateNote(tail, Effects(), timeline_);
        changed.push_back(CompileNote(std::move(tail)));
        if (*end <= changed.front().note.position)
            throw std::invalid_argument("Long note end must follow its start in musical position.");
    }
    // All allocation/validation precedes publication. These maps contain only
    // the edited head/tail; map::merge transfers their nodes without allocating.
    decltype(noteTree_) notes;
    decltype(noteKeys_) keys;
    decltype(timeIndex_) times;
    decltype(noteSpeedIndex_) speeds;
    decltype(longEndpoints_) endpoints;
    for (const auto &note : changed)
    {
        const auto noteId = note.note.sourceOrder;
        const NoteKey key{note.note.position, noteId};
        notes.emplace(key, note);
        keys.emplace(noteId, key);
        times.emplace(TimeKey{note.timing, noteId}, key);
        speeds.emplace(note.scrollMultiplier, noteId);
        if (note.note.actionType != 0)
            endpoints.emplace(key, noteId);
    }
    const bool affectsPairs = selected->head.note.actionType != 0 || end.has_value();
    auto pairs = affectsPairs ? BuildLongPartners(id, oldTail, changed) : decltype(longPartners_){};
    EraseNote(id);
    if (oldTail)
        EraseNote(*oldTail);
    noteTree_.merge(notes);
    noteKeys_.merge(keys);
    timeIndex_.merge(times);
    noteSpeedIndex_.merge(speeds);
    longEndpoints_.merge(endpoints);
    if (affectsPairs)
        longPartners_.swap(pairs);
    nextNoteId_ += needsId;
    InvalidateNoteSnapshots();
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
    if (measure < 0 || measure >= MaximumMeasureCount || length <= Rational{})
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
        << "Effect file: " << Utf8(pattern.effectFile) << '\n'
        << "Pattern Name: " << pattern.name << '\n';
    for (std::size_t i = 0; i < pattern.makers.size(); ++i)
        out << "Pattern Maker " << i << ": " << pattern.makers[i] << '\n';
    out << "Tags: ";
    for (std::size_t i = 0; i < pattern.tags.size(); ++i)
        out << (i ? ", " : "") << pattern.tags[i];
    out << "\nPattern Offset: " << pattern.patternOffsetMilliseconds << "\nBase BPM: " << pattern.baseBpm
        << "\n[Difficulty]\nMode: " << pattern.mode << "\nJudgeLevel: " << pattern.judgementLevel << '\n';
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
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(17) << "Version: " << effects.version << "\n[Interpolation]\n";
    for (const auto &[name, expression] : effects.interpolations) out << name << ": " << expression << '\n';
    out << "[HitSounds]\n";
    for (const auto &[index, path] : effects.hitSounds) out << index << ": " << Utf8(path) << '\n';
    auto commands = effects.commands;
    std::ranges::stable_sort(commands, {}, &EffectCommand::position);
    for (const auto group : {"Speed", "Sounds", "Zone"})
    {
        out << '[' << group << "]\n";
        for (const auto &c : commands)
        {
            if (EffectSection(c.type) != group) continue;
            WriteEffect(out, c);
        }
        if (std::string_view(group) == "Sounds")
            for (const auto &c : effects.hitSoundChanges)
            {
                WritePosition(out, c.position);
                out << ", #HitSound " << (c.keyType == 1 ? "Don" : "Kat") << ' ' << c.soundIndex << '\n';
            }
    }
    return out.str();
}

void ChartEditor::Save()
{
    if (pattern_.sourcePath.empty())
        throw std::runtime_error("No YMP save path.");
    auto savedPattern = Pattern();
    if (savedPattern.effectFile.empty())
        savedPattern.effectFile = pattern_.sourcePath.stem().wstring() + L".yme";
    auto effectPath = (pattern_.sourcePath.parent_path() / savedPattern.effectFile).lexically_normal();
    if (effectPath == pattern_.sourcePath.lexically_normal() ||
        (std::filesystem::exists(effectPath) && std::filesystem::exists(pattern_.sourcePath) &&
         std::filesystem::equivalent(effectPath, pattern_.sourcePath)))
        throw std::invalid_argument("YMP and YME must have different save paths.");
    const std::string patternText = WritePattern(savedPattern);
    const std::string effectText = WriteEffects(Effects());
    ChartParser parser;
    const auto parsedPattern = parser.ParsePattern(patternText);
    const auto parsedEffects = parser.ParseEffect(effectText);
    if (!parsedPattern.Succeeded() || !parsedEffects.Succeeded() ||
        WritePattern(parsedPattern.document) != patternText || WriteEffects(parsedEffects.document) != effectText)
        throw std::runtime_error("Serialized chart failed validation; originals were not modified.");
    if (!effectPath.parent_path().empty()) std::filesystem::create_directories(effectPath.parent_path());

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
    bool backedPattern = false, backedEffect = false, installedPattern = false, installedEffect = false;
    try
    {
        // Staging can fail after creating either temporary file. It belongs to
        // the same rollback boundary as installing the two completed files.
        WriteFile(patternTemp, patternText);
        WriteFile(effectTemp, effectText);
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
        std::filesystem::remove(patternTemp, ignored);
        std::filesystem::remove(effectTemp, ignored);
        throw;
    }
    std::error_code ignored;
    if (backedPattern)
        std::filesystem::remove(patternBackup, ignored);
    if (backedEffect)
        std::filesystem::remove(effectBackup, ignored);
    // Publication cannot allocate after the on-disk transaction has committed.
    pattern_.effectFile.swap(savedPattern.effectFile);
    effects_.sourcePath.swap(effectPath);
    dirty_ = false;
}
} // namespace finger_drum::chart
