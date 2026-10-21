#include "Taiko/Results/TaikoResultCollector.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

TaikoResultGroup ResultGroup(TaikoNoteType type) noexcept
{
    const auto *definition = FindTaikoNote(static_cast<int>(type));
    if (definition && definition->longNote)
        return TaikoResultGroup::Roll;
    return type == TaikoNoteType::Don || type == TaikoNoteType::Kat ? TaikoResultGroup::Normal
                                                                    : TaikoResultGroup::Big;
}
TaikoResultCollector::TaikoResultCollector(PlayResultSummary summary, std::size_t reserveNotes)
    : result_(std::make_unique<TaikoPlayResult>())
{
    result_->summary = std::move(summary);
    result_->notes.reserve(reserveNotes);
    result_->timingErrors.reserve(reserveNotes);
    result_->summary.inputs.reserve(reserveNotes);
    noteIndices_.reserve(reserveNotes);
}
void TaikoResultCollector::RegisterNote(NoteId id, TaikoNoteType type, RhythmTime time)
{
    if (!result_ || !noteIndices_.emplace(id, result_->notes.size()).second)
        throw std::logic_error("Result note IDs must be unique.");
    result_->notes.push_back({id, type, time});
}
void TaikoResultCollector::RecordInput(const RhythmInputEvent &input)
{
    if (result_)
        result_->summary.inputs.push_back(input);
}
void TaikoResultCollector::RecordControl(PlayControlEvent event)
{
    if (!result_)
        return;
    event.inputIndex = result_->summary.inputs.size();
    result_->summary.controls.push_back(event);
    result_->summary.pauseMenuUsed |= event.pauseMenuShown;
}
void TaikoResultCollector::SetInitialTime(RhythmTime time) noexcept
{
    if (result_)
        result_->summary.initialTime = time;
}
void TaikoResultCollector::SetInputMapping(std::map<PhysicalKey, NoteAction> mapping)
{
    result_->summary.inputMapping = std::move(mapping);
}
void TaikoResultCollector::ConsumeEvent(const NoteEvent &event, bool keyPressed)
{
    const auto found = noteIndices_.find(event.noteId);
    if (found == noteIndices_.end() || result_->notes[found->second].processed)
        return;
    const auto grade = static_cast<std::size_t>(event.judgement.grade);
    if (event.type == NoteEventType::Completed || event.type == NoteEventType::Missed)
        result_->notes[found->second].finalState = event.stateAfter;
    if (event.type == NoteEventType::HitAccepted && grade < 5)
    {
        ++result_->judgements[grade];
        result_->timingErrors.push_back({event.noteId, event.hitIndex, event.eventTime, event.judgement.signedError,
                                         event.judgement.grade,
                                         AccuracyScore::FromRate(event.judgement.scoreRate)});
    }
    else if (keyPressed && event.type == NoteEventType::InputRejected &&
             IsAtLeastAsAccurateAs(event.judgement.grade, JudgementGrade::Good))
        ++result_->poorCount;
    else if (event.type == NoteEventType::TickAccepted &&
             (result_->notes[found->second].type == TaikoNoteType::TickRoll ||
              result_->notes[found->second].type == TaikoNoteType::BigTickRoll ||
              result_->notes[found->second].type == TaikoNoteType::Buzz))
        ++result_->acceptedTicks;
    else if (event.type == NoteEventType::TickMissed)
        ++result_->missedTicks;
    // HoldStarted and Completed carry head judgements too; ignoring them
    // here prevents a single Buzz press from producing duplicate samples.
}
void TaikoResultCollector::RecordRetirement(const NoteAccuracy &accuracy)
{
    if (result_)
        FinalizeNote(accuracy, true);
}
void TaikoResultCollector::FinalizeNote(const NoteAccuracy &accuracy, bool retired)
{
    const auto found = noteIndices_.find(accuracy.noteId);
    if (found == noteIndices_.end() || !accuracy.finalized)
        return;
    auto &note = result_->notes[found->second];
    if (note.processed)
        return;
    note.processed = true;
    note.accuracy = retired ? AccuracyScore{} : accuracy.Score();
    if (retired)
        note.finalState = NoteState::Missed;
    note.target = accuracy.target;
    note.acceptedHits = accuracy.acceptedHits;
    note.acceptedTicks = accuracy.acceptedTicks;
    const bool longNote = ResultGroup(note.type) == TaikoResultGroup::Roll;
    if (longNote && note.finalState == NoteState::Completed && accuracy.acceptedHits >= accuracy.target.hits &&
        accuracy.acceptedTicks >= accuracy.target.ticks)
        ++result_->okCount;
    else if (note.finalState == NoteState::Missed || longNote)
        ++result_->judgements[static_cast<std::size_t>(JudgementGrade::Miss)];
}
void TaikoResultCollector::Consume(const NoteProcessResult &result, bool keyPressed)
{
    if (!result_)
        return;
    for (const auto &event : result.events)
        ConsumeEvent(event, keyPressed);
    for (const auto &accuracy : result.finalizedAccuracies)
        FinalizeNote(accuracy);
}
void TaikoResultCollector::Reset() noexcept
{
    ResetJudgements();
    if (!result_)
        return;
    result_->summary.inputs.clear();
    result_->summary.controls.clear();
    result_->summary.pauseMenuUsed = false;
}
void TaikoResultCollector::ResetJudgements() noexcept
{
    if (!result_)
        return;
    for (auto &note : result_->notes)
    {
        note.processed = false;
        note.finalState = NoteState::Pending;
        note.accuracy = {};
        note.target = {};
        note.acceptedHits = note.acceptedTicks = 0;
    }
    result_->timingErrors.clear();
    result_->judgements = {};
    result_->okCount = result_->poorCount = result_->acceptedTicks = result_->missedTicks = 0;
}
void TaikoResultCollector::Summarize()
{
    auto &summary = result_->summary;
    summary.totalNotes = result_->notes.size();
    std::uint64_t totalSum{};
    std::array<std::uint64_t, 3> groupSums{};
    for (const auto &note : result_->notes)
    {
        const auto index = static_cast<std::size_t>(ResultGroup(note.type));
        auto &group = result_->groups[index];
        ++group.total;
        if (note.processed)
        {
            ++summary.processedNotes;
            ++group.processed;
            groupSums[index] += note.accuracy.Units();
            totalSum += note.accuracy.Units();
        }
    }
    summary.unprocessedNotes = summary.totalNotes - summary.processedNotes;
    summary.accuracy = AccuracyScore::Average(totalSum, summary.totalNotes);
    for (std::size_t i = 0; i < groupSums.size(); ++i)
        result_->groups[i].accuracy = AccuracyScore::Average(groupSums[i], result_->groups[i].total);

    // Population variance of real graded hits, using Welford accumulation.
    // Automatic misses and unfinished notes have no invented timing error.
    double mean{}, m2{};
    std::size_t count{};
    for (const auto &sample : result_->timingErrors)
    {
        const double value = static_cast<double>(sample.error.count()) / 1000.0;
        const double delta = value - mean;
        mean += delta / static_cast<double>(++count);
        m2 += delta * (value - mean);
    }
    if (count)
    {
        result_->meanErrorMilliseconds = mean;
        result_->varianceMillisecondsSquared = std::max(0.0, m2 / static_cast<double>(count));
        result_->standardDeviationMilliseconds = std::sqrt(*result_->varianceMillisecondsSquared);
    }
}
std::unique_ptr<IPlayResult> TaikoResultCollector::TakeResult(PlayEndReason reason, RhythmTime time)
{
    if (!result_)
        throw std::logic_error("The play result was already transferred.");
    result_->summary.endReason = reason;
    result_->summary.endTime = time;
    Summarize();
    return std::move(result_);
}
