#include "EditorTime.h"
#include "EditorWorkspace.h"
#include <limits>

namespace chart = finger_drum::chart;

void EditorWorkspace::SelectNotes(std::span<const std::size_t> ids)
{
    std::set<std::size_t> selected;
    std::optional<std::pair<chart::MusicalPosition, chart::MusicalPosition>> range;
    for (const auto id : ids)
        if (const auto note = Document().FindNote(id))
        {
            selected.insert(note->head.note.sourceOrder);
            const auto begin = note->head.note.position;
            const auto end = note->tail ? note->tail->note.position : begin;
            if (note->tail)
                selected.insert(note->tail->note.sourceOrder);
            if (!range)
                range = {begin, end};
            else
            {
                range->first = std::min(range->first, begin);
                range->second = std::max(range->second, end);
            }
        }
    selectedNotes_.swap(selected);
    noteSelectionRange_ = range;
    RequestRebuild();
}

void EditorWorkspace::SelectNoteRange(chart::MusicalPosition begin, chart::MusicalPosition end)
{
    if (end < begin)
        std::swap(begin, end);
    std::set<std::size_t> ids, visited;
    for (const auto &candidate : Document().NotesInMeasures(begin.measure, end.measure + 1))
        if (const auto note = Document().FindNote(candidate.note.sourceOrder))
        {
            if (!visited.insert(note->head.note.sourceOrder).second)
                continue;
            const auto head = note->head.note.position;
            const auto tail = note->tail ? note->tail->note.position : head;
            if (head > end || tail < begin)
                continue;
            ids.insert(note->head.note.sourceOrder);
            if (note->tail)
                ids.insert(note->tail->note.sourceOrder);
        }
    selectedNotes_.swap(ids);
    noteSelectionRange_ = {begin, end}; // Preserve blank margins for paste-with-blanks.
    RequestRebuild();
}

void EditorWorkspace::SelectListRow(std::size_t row, bool toggle, bool range)
{
    if (tab_ != EditorTab::Timing && tab_ != EditorTab::Effects)
        return;
    auto &selected = tab_ == EditorTab::Timing ? selectedTiming_ : selectedEffects_;
    auto &anchor = tab_ == EditorTab::Timing ? timingSelectionAnchor_ : effectSelectionAnchor_;
    const auto count = tab_ == EditorTab::Timing
                           ? Document().Timing().size()
                           : Document().Effects().commands.size() + Document().Effects().hitSoundChanges.size();
    if (row >= count)
        throw std::out_of_range("Selected editor row no longer exists.");
    if (range && anchor)
    {
        if (!toggle)
            selected.clear();
        for (auto i = std::min(row, *anchor); i <= std::max(row, *anchor); ++i)
            selected.insert(i);
    }
    else
    {
        if (!toggle)
            selected.clear();
        if (toggle && selected.contains(row))
            selected.erase(row);
        else
            selected.insert(row);
        anchor = row;
    }
    RequestRebuild();
}

bool EditorWorkspace::IsListRowSelected(std::size_t row) const noexcept
{
    return tab_ == EditorTab::Timing ? selectedTiming_.contains(row)
                                     : tab_ == EditorTab::Effects && selectedEffects_.contains(row);
}

void EditorWorkspace::ClearEditSelection()
{
    selectedNotes_.clear();
    selectedTiming_.clear();
    selectedEffects_.clear();
    noteSelectionRange_.reset();
    timingSelectionAnchor_.reset();
    effectSelectionAnchor_.reset();
    mode_->CancelInteraction();
}

void EditorWorkspace::Undo()
{
    if (!Document().CanUndo())
        return;
    editor_->Undo();
    ClearEditSelection();
    DocumentChanged();
}

void EditorWorkspace::Redo()
{
    if (!Document().CanRedo())
        return;
    editor_->Redo();
    ClearEditSelection();
    DocumentChanged();
}

void EditorWorkspace::CopySelection()
{
    Clipboard result;
    result.tab = tab_;
    std::optional<std::pair<chart::MusicalPosition, chart::MusicalPosition>> bounds;
    const auto extend = [&bounds](auto first, auto last)
    {
        if (!bounds)
            bounds = {first, last};
        else
        {
            bounds->first = std::min(bounds->first, first);
            bounds->second = std::max(bounds->second, last);
        }
    };
    if (tab_ == EditorTab::Pattern)
    {
        if (noteSelectionRange_)
            bounds = noteSelectionRange_;
        std::set<std::size_t> visited;
        for (const auto id : selectedNotes_)
            if (const auto note = Document().FindNote(id); note && visited.insert(note->head.note.sourceOrder).second)
            {
                result.notes.push_back({note->head.note, note->tail ? std::optional{note->tail->note} : std::nullopt});
                // Whole long notes are copied even when only one endpoint was dragged.
                extend(note->head.note.position, note->tail ? note->tail->note.position : note->head.note.position);
            }
    }
    else if (tab_ == EditorTab::Timing)
        for (auto row : selectedTiming_)
        {
            const auto &event = Document().Timing().at(row);
            result.timing.push_back({event, {}});
            extend(event.position, event.position);
        }
    else if (tab_ == EditorTab::Effects)
        for (auto row : selectedEffects_)
        {
            const auto &effects = Document().Effects();
            if (row < effects.commands.size())
            {
                const auto &event = effects.commands[row];
                result.effects.push_back({event, {}});
                extend(event.position, event.endPosition.value_or(event.position));
            }
            else
            {
                const auto &event = effects.hitSoundChanges.at(row - effects.commands.size());
                result.sounds.push_back({event, {}});
                extend(event.position, event.position);
            }
        }
    if (!bounds)
        return;
    const auto &timeline = Document().Timeline();
    const auto offset = [&](auto position) { return timeline.WholeNotesBetween(bounds->first, position); };
    result.length = offset(bounds->second);
    for (auto &note : result.notes)
    {
        note.headOffset = offset(note.head.position);
        note.tailOffset = note.tail ? offset(note.tail->position) : note.headOffset;
    }
    for (auto &item : result.timing)
        item.offset = offset(item.value.position);
    for (auto &item : result.sounds)
        item.offset = offset(item.value.position);
    for (auto &item : result.effects)
    {
        item.offset = offset(item.value.position);
        if (item.value.endPosition)
            item.endOffset = offset(*item.value.endPosition);
    }
    clipboard_ = std::move(result);
}

void EditorWorkspace::PasteSelection(bool includeBlanks)
{
    if (!clipboard_)
        return;
    if (clipboard_->tab != tab_)
        throw std::invalid_argument("Paste into the same editor tab as the copied items.");
    if (!includeBlanks && clipboard_->notes.empty() && clipboard_->timing.empty() && clipboard_->effects.empty() &&
        clipboard_->sounds.empty())
        return;
    const auto &timeline = Document().Timeline();
    const auto divisions = static_cast<std::int64_t>(score_.division) * 4;
    const auto whole = std::max(0.0L, timeline.WholeNotesAtTime(editor_time::Microseconds(timeMs_)));
    const auto value = std::round(whole * divisions);
    if (!std::isfinite(value) || value >= std::numeric_limits<std::int64_t>::max())
        throw std::out_of_range("Paste position exceeds the supported musical range.");
    const auto anchor = timeline.PositionAtWholeNotes({static_cast<std::int64_t>(value), divisions});
    if (tab_ == EditorTab::Pattern)
        PasteNotes(*clipboard_, anchor, includeBlanks);
    else if (tab_ == EditorTab::Timing)
        PasteTiming(*clipboard_, anchor, includeBlanks);
    else if (tab_ == EditorTab::Effects)
        PasteEffects(*clipboard_, anchor, includeBlanks);
    ClearEditSelection();
}

void EditorWorkspace::DeleteSelection()
{
    auto pattern = Document().Pattern();
    auto effects = Document().Effects();
    if (tab_ == EditorTab::Pattern)
    {
        if (selectedNotes_.empty())
            return;
        std::erase_if(pattern.notes, [this](const auto &note) { return selectedNotes_.contains(note.sourceOrder); });
    }
    else if (tab_ == EditorTab::Timing)
    {
        if (selectedTiming_.empty())
            return;
        std::size_t row = 0;
        std::erase_if(pattern.timing, [this, &row](const auto &) { return selectedTiming_.contains(row++); });
    }
    else if (tab_ == EditorTab::Effects)
    {
        if (selectedEffects_.empty())
            return;
        std::size_t row = 0;
        std::erase_if(effects.commands, [this, &row](const auto &) { return selectedEffects_.contains(row++); });
        std::erase_if(effects.hitSoundChanges, [this, &row](const auto &) { return selectedEffects_.contains(row++); });
    }
    else
        return;
    Replace(std::move(pattern), std::move(effects));
    ClearEditSelection();
}
