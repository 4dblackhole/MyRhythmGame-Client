#include "Editing/ChartEditor.h"
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <tuple>

namespace
{
    PatternDocument Metadata(const PatternDocument &source)
    {
        PatternDocument result;
        result.version = source.version;
        result.sourcePath = source.sourcePath;
        result.musicMetadataFile = source.musicMetadataFile;
        result.effectFile = source.effectFile;
        result.backgroundFile = source.backgroundFile;
        result.backgroundOffsetX = source.backgroundOffsetX;
        result.backgroundOffsetY = source.backgroundOffsetY;
        result.makers = source.makers;
        result.tags = source.tags;
        result.name = source.name;
        result.mode = source.mode;
        result.patternOffsetMilliseconds = source.patternOffsetMilliseconds;
        result.baseBpm = source.baseBpm;
        result.judgementLevel = source.judgementLevel;
        result.systemBreakMeasures = source.systemBreakMeasures;
        return result;
    }

    bool SameNote(const PatternNote &a, const PatternNote &b)
    {
        return a.position == b.position && a.keyType == b.keyType && a.actionType == b.actionType &&
               a.hitSound == b.hitSound && a.extraData == b.extraData && a.sourceOrder == b.sourceOrder;
    }

    bool SameTiming(const std::vector<TimingDirective> &a, const std::vector<TimingDirective> &b)
    {
        return std::ranges::equal(a, b,
                                  [](const auto &x, const auto &y)
                                  {
                                      return std::tie(x.position, x.type, x.value, x.ratio) ==
                                             std::tie(y.position, y.type, y.value, y.ratio);
                                  });
    }
} // namespace

ChartEditor::HistoryList ChartEditor::PrepareHistory(HistoryState before, HistoryState after) const
{
    if (nextHistoryState_ == std::numeric_limits<std::uint64_t>::max())
        throw std::overflow_error("Editor history ID space exhausted.");
    HistoryList prepared;
    prepared.push_back({std::move(before), std::move(after), historyState_, nextHistoryState_});
    return prepared;
}

ChartEditor::HistoryList ChartEditor::ReplacementHistory(const ChartEditor &replacement) const
{
    HistoryState before, after;
    auto oldMetadata = Metadata(pattern_), newMetadata = Metadata(replacement.pattern_);
    if (oldMetadata.sourcePath != newMetadata.sourcePath ||
        oldMetadata.systemBreakMeasures != newMetadata.systemBreakMeasures ||
        WritePattern(oldMetadata) != WritePattern(newMetadata))
    {
        before.metadata = std::move(oldMetadata);
        after.metadata = std::move(newMetadata);
    }
    if (!SameTiming(Timing(), replacement.Timing()))
    {
        before.timing = Timing();
        after.timing = replacement.Timing();
    }
    if (Effects().sourcePath != replacement.Effects().sourcePath ||
        WriteEffects(Effects()) != WriteEffects(replacement.Effects()))
    {
        before.effects = Effects();
        after.effects = replacement.Effects();
    }
    for (const auto &[key, note] : noteTree_)
    {
        const auto next = replacement.noteKeys_.find(key.second);
        if (next == replacement.noteKeys_.end() ||
            !SameNote(note.note, replacement.noteTree_.at(next->second).note))
            before.notes.push_back(note.note);
    }
    for (const auto &[key, note] : replacement.noteTree_)
    {
        const auto old = noteKeys_.find(key.second);
        if (old == noteKeys_.end() || !SameNote(note.note, noteTree_.at(old->second).note))
            after.notes.push_back(note.note);
    }
    return PrepareHistory(std::move(before), std::move(after));
}

void ChartEditor::CommitHistory(HistoryList &prepared) noexcept
{
    historyState_ = prepared.front().afterId;
    ++nextHistoryState_;
    redo_.clear();
    undo_.splice(undo_.end(), prepared);
    if (undo_.size() > 32)
        undo_.pop_front();
    dirty_ = historyState_ != savedHistoryState_;
}

ChartEditor::SavedHistoryPaths ChartEditor::PrepareSavedHistoryPaths(const std::filesystem::path &effectFile)
{
    SavedHistoryPaths paths;
    if (!pattern_.effectFile.empty())
        return paths;
    // The first save materializes an implicit default YME reference. It is
    // storage normalization, not a metadata edit for Undo/Redo to reverse.
    // Allocate replacements before file I/O; list-owned targets stay stable
    // until Save commits their paths with noexcept swaps.
    for (auto *history : {&undo_, &redo_})
        for (auto &edit : *history)
            for (auto *state : {&edit.before, &edit.after})
                if (state->metadata && state->metadata->sourcePath == pattern_.sourcePath &&
                    state->metadata->effectFile.empty())
                    paths.emplace_back(&state->metadata->effectFile, effectFile);
    return paths;
}

void ChartEditor::TransferHistory(ChartEditor &replacement) noexcept
{
    replacement.undo_.splice(replacement.undo_.end(), undo_);
    replacement.redo_.splice(replacement.redo_.end(), redo_);
    replacement.historyState_ = historyState_;
    replacement.savedHistoryState_ = savedHistoryState_;
    replacement.nextHistoryState_ = nextHistoryState_;
}

void ChartEditor::RestoreHistory(const HistoryState &target, const HistoryState &removed, std::uint64_t id)
{
    if (!target.metadata && !target.timing && !target.effects)
    {
        RestoreNotes(target, removed);
        historyState_ = id;
        dirty_ = historyState_ != savedHistoryState_;
        return;
    }
    // Recompile once for the entire action. The original trees/history are
    // still intact if validation or allocation fails during preparation.
    auto pattern = target.metadata ? *target.metadata : Metadata(pattern_);
    pattern.timing = target.timing ? *target.timing : Timing();
    std::set<std::size_t> changed;
    for (const auto &note : target.notes)
        changed.insert(note.sourceOrder);
    for (const auto &note : removed.notes)
        changed.insert(note.sourceOrder);
    pattern.notes.reserve(noteTree_.size() + target.notes.size());
    for (const auto &[key, note] : noteTree_)
        if (!changed.contains(key.second))
            pattern.notes.push_back(note.note);
    pattern.notes.insert(pattern.notes.end(), target.notes.begin(), target.notes.end());
    ReplaceData(std::move(pattern), target.effects ? *target.effects : Effects());
    historyState_ = id;
    dirty_ = historyState_ != savedHistoryState_;
}

void ChartEditor::RestoreNotes(const HistoryState &target, const HistoryState &removed)
{
    std::set<std::size_t> changed;
    for (const auto &note : target.notes)
        changed.insert(note.sourceOrder);
    for (const auto &note : removed.notes)
        changed.insert(note.sourceOrder);
    std::vector<std::size_t> excluded(changed.begin(), changed.end());
    std::vector<CompiledPatternNote> compiled;
    for (const auto &source : target.notes)
        compiled.push_back(CompileNote(source));
    ApplyNotePatch(excluded, compiled);
}

void ChartEditor::Undo()
{
    if (undo_.empty())
        return;
    const auto &edit = undo_.back();
    RestoreHistory(edit.before, edit.after, edit.beforeId);
    redo_.splice(redo_.end(), undo_, std::prev(undo_.end()));
}

void ChartEditor::Redo()
{
    if (redo_.empty())
        return;
    const auto &edit = redo_.back();
    RestoreHistory(edit.after, edit.before, edit.afterId);
    undo_.splice(undo_.end(), redo_, std::prev(redo_.end()));
}
