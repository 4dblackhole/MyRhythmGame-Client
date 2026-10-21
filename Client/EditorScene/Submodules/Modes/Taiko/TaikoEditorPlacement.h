#pragma once
#include "Editing/IEditorDocument.h"
#include "Taiko/Submodules/TaikoChartNotes.h"
#include <algorithm>
#include <limits>
#include <stdexcept>

inline void ValidatePlacement(const IEditorDocument &document, MusicalPosition position, int keyType,
                              std::optional<MusicalPosition> end = {},
                              std::optional<std::size_t> editedId = {})
{
    const auto beginTime = document.Timeline().Compile(position);
    const auto endTime = end ? document.Timeline().Compile(*end) : beginTime;
    if (end && *end <= position)
        throw std::invalid_argument("Long note end must follow its start in musical position.");

    // The range query includes both endpoints of every intersecting long note.
    // Edits keep their stable IDs; new notes follow existing equal-time notes.
    const auto edited = editedId ? document.FindNote(*editedId) : std::nullopt;
    auto notes = document.NotesInTimeRange(std::min(beginTime, endTime), std::max(beginTime, endTime));
    if (edited)
        std::erase_if(notes,
                      [&](const auto &note)
                      {
                          return note.note.sourceOrder == edited->head.note.sourceOrder ||
                                 (edited->tail && note.note.sourceOrder == edited->tail->note.sourceOrder);
                      });
    notes.push_back({{position,
                      keyType,
                      end ? 1 : 0,
                      {},
                      {},
                      {},
                      edited ? edited->head.note.sourceOrder : std::numeric_limits<std::size_t>::max() - 1},
                     beginTime,
                     1,
                     document.Timeline().IsPositionActive(position)});
    if (end)
        notes.push_back(
            {{*end,
              keyType,
              2,
              {},
              {},
              {},
              edited && edited->tail ? edited->tail->note.sourceOrder : std::numeric_limits<std::size_t>::max()},
             endTime,
             1,
             document.Timeline().IsPositionActive(*end)});
    notes = PlayableInSourceOrder(std::move(notes));
    std::optional<int> activeLongType;
    for (const auto &compiled : notes)
    {
        const auto &note = compiled.note;
        if (note.actionType == 1)
        {
            if (activeLongType)
                throw std::invalid_argument("Taiko long notes cannot overlap another long note.");
            activeLongType = note.keyType;
        }
        else if (note.actionType == 2 && activeLongType == note.keyType)
            activeLongType.reset();
        else if (note.actionType == 0 && activeLongType)
            throw std::invalid_argument("A Taiko ordinary note cannot be placed inside a long note.");
    }
}
