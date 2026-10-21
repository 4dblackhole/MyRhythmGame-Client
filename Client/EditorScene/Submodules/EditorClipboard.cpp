#include "EditorWorkspace.h"
#include "Utility/IntervalIndex.h"
#include <limits>

namespace
{
    bool Intersects(MusicalPosition a, MusicalPosition b, MusicalPosition c,
                    MusicalPosition d)
    {
        return a <= d && c <= b;
    }
} // namespace

void EditorWorkspace::PasteNotes(const Clipboard &copied, MusicalPosition anchor, bool includeBlanks)
{
    const auto &timeline = Document().Timeline();
    const auto pasteEnd = timeline.AddWholeNotes(anchor, copied.length);
    std::vector<PatternNote> added;
    std::vector<std::pair<MusicalPosition, MusicalPosition>> intervals;
    auto pattern = Document().Pattern();
    std::size_t nextId = 0;
    for (const auto &note : pattern.notes)
    {
        if (note.sourceOrder == std::numeric_limits<std::size_t>::max())
            throw std::overflow_error("Note ID space exhausted.");
        nextId = std::max(nextId, note.sourceOrder + 1);
    }
    for (const auto &note : copied.notes)
    {
        if (nextId > std::numeric_limits<std::size_t>::max() - 2)
            throw std::overflow_error("Note ID space exhausted.");
        auto head = note.head;
        head.position = timeline.AddWholeNotes(anchor, note.headOffset);
        head.sourceOrder = nextId++;
        const auto end = note.tail ? timeline.AddWholeNotes(anchor, note.tailOffset) : head.position;
        intervals.emplace_back(head.position, end);
        added.push_back(std::move(head));
        if (note.tail)
        {
            auto tail = *note.tail;
            tail.position = end;
            tail.sourceOrder = nextId++;
            added.push_back(std::move(tail));
        }
    }
    std::set<std::size_t> removed, visited;
    std::ranges::sort(intervals);
    std::vector<MusicalPosition> maximumEnds;
    for (const auto &interval : intervals)
        maximumEnds.push_back(maximumEnds.empty() ? interval.second : std::max(maximumEnds.back(), interval.second));
    for (const auto &source : pattern.notes)
    {
        const auto note = Document().FindNote(source.sourceOrder);
        const auto headId = note ? note->head.note.sourceOrder : source.sourceOrder;
        if (!visited.insert(headId).second)
            continue;
        const auto begin = note ? note->head.note.position : source.position;
        const auto end = note && note->tail ? note->tail->note.position : begin;
        const auto next = std::ranges::upper_bound(intervals, end, {}, &decltype(intervals)::value_type::first);
        const auto count = static_cast<std::size_t>(next - intervals.begin());
        const bool replaced = (includeBlanks && Intersects(begin, end, anchor, pasteEnd)) ||
                              (count != 0 && maximumEnds[count - 1] >= begin);
        if (replaced)
        {
            removed.insert(headId);
            if (note && note->tail)
                removed.insert(note->tail->note.sourceOrder);
        }
    }
    std::erase_if(pattern.notes, [&removed](const auto &note) { return removed.contains(note.sourceOrder); });
    pattern.notes.insert(pattern.notes.end(), added.begin(), added.end());
    // One replacement is one history action, including everything overwritten.
    Replace(std::move(pattern), Document().Effects());
}

void EditorWorkspace::PasteTiming(const Clipboard &copied, MusicalPosition anchor, bool includeBlanks)
{
    const auto &timeline = Document().Timeline();
    auto timing = Document().Timing();
    const auto end = timeline.AddWholeNotes(anchor, copied.length);
    std::set<std::pair<MusicalPosition, TimingDirectiveType>> replaced;
    std::vector<TimingDirective> added;
    for (const auto &item : copied.timing)
    {
        auto event = item.value;
        event.position = timeline.AddWholeNotes(anchor, item.offset);
        if (event.type == TimingDirectiveType::MeasureLength && event.position.fraction != Rational{})
            throw std::invalid_argument("A measure-length change must be pasted at a measure start.");
        replaced.emplace(event.position, event.type);
        added.push_back(std::move(event));
    }
    std::erase_if(timing,
                  [&](const auto &event)
                  {
                      return (includeBlanks && event.position >= anchor && event.position <= end) ||
                             replaced.contains({event.position, event.type});
                  });
    timing.insert(timing.end(), added.begin(), added.end());
    editor_->ReplaceTiming(std::move(timing));
    DocumentChanged();
}

void EditorWorkspace::PasteEffects(const Clipboard &copied, MusicalPosition anchor, bool includeBlanks)
{
    const auto &timeline = Document().Timeline();
    auto effects = Document().Effects();
    const auto end = timeline.AddWholeNotes(anchor, copied.length);
    using Index = IntervalIndex<MusicalPosition, bool>;
    using Track = std::pair<EffectCommandType, std::string>;
    std::map<Track, std::vector<Index::Entry>> intervals;
    std::map<Track, Index> indices;
    std::vector<EffectCommand> addedCommands;
    std::vector<HitSoundChange> addedSounds;
    std::set<std::pair<int, MusicalPosition>> replacedSounds;
    for (const auto &item : copied.effects)
    {
        auto event = item.value;
        event.position = timeline.AddWholeNotes(anchor, item.offset);
        if (item.endOffset)
            event.endPosition = timeline.AddWholeNotes(anchor, *item.endOffset);
        intervals[{event.type, event.target}].push_back(
            {event.position, event.endPosition.value_or(event.position), true});
        addedCommands.push_back(std::move(event));
    }
    for (const auto &item : copied.sounds)
    {
        auto event = item.value;
        event.position = timeline.AddWholeNotes(anchor, item.offset);
        replacedSounds.emplace(event.keyType, event.position);
        addedSounds.push_back(std::move(event));
    }
    for (auto &[track, entries] : intervals)
        indices[track].Assign(std::move(entries));
    // Query copied intervals once per existing event instead of erasing the
    // whole list repeatedly for each copied effect.
    std::erase_if(effects.commands,
                  [&](const auto &event)
                  {
                      const auto finish = event.endPosition.value_or(event.position);
                      if (includeBlanks && Intersects(event.position, finish, anchor, end))
                          return true;
                      bool replaced = false;
                      if (const auto track = indices.find({event.type, event.target}); track != indices.end())
                          track->second.Visit(event.position, finish, [&](bool) { replaced = true; });
                      return replaced;
                  });
    std::erase_if(effects.hitSoundChanges,
                  [&](const auto &event)
                  {
                      return (includeBlanks && event.position >= anchor && event.position <= end) ||
                             replacedSounds.contains({event.keyType, event.position});
                  });
    effects.commands.insert(effects.commands.end(), addedCommands.begin(), addedCommands.end());
    effects.hitSoundChanges.insert(effects.hitSoundChanges.end(), addedSounds.begin(), addedSounds.end());
    editor_->ReplaceEffects(std::move(effects));
    DocumentChanged();
}
