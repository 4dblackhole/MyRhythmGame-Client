#pragma once
#include "Mode/Results/IPlayResult.h"
#include "Taiko/Submodules/TaikoNoteDefinition.h"
#include <array>
#include <optional>

enum class TaikoResultGroup : std::size_t
{
    Normal,
    Big,
    Roll,
    Count
};
struct TaikoNoteResult
{
    NoteId id{};
    TaikoNoteType type{};
    RhythmTime time{};
    bool processed{};
    NoteState finalState{NoteState::Pending};
    AccuracyScore accuracy;
    NoteAccuracyTarget target;
    std::size_t acceptedHits{};
    std::size_t acceptedTicks{};
};
struct TaikoGroupResult
{
    std::size_t total{};
    std::size_t processed{};
    AccuracyScore accuracy;
};
struct TimingErrorSample
{
    NoteId noteId{};
    std::size_t hitIndex{};
    RhythmTime inputTime{};
    RhythmDuration error{};
    JudgementGrade grade{};
    AccuracyScore accuracy;
};
class TaikoPlayResult final : public IPlayResult
{
  public:
    std::string_view ModeId() const noexcept override
    {
        return "Taiko";
    }
    const PlayResultSummary &Summary() const noexcept override
    {
        return summary;
    }
    PlayResultSummary summary;
    std::vector<TaikoNoteResult> notes;
    std::array<TaikoGroupResult, static_cast<std::size_t>(TaikoResultGroup::Count)> groups{};
    // MAX..BAD count graded hits, MISS counts failed logical completions.
    // Long-note completion is counted once as OK/MISS, never as another head hit.
    std::array<std::size_t, 6> judgements{};
    std::size_t okCount{};
    std::size_t poorCount{};
    std::size_t acceptedTicks{};
    std::size_t missedTicks{};
    std::vector<TimingErrorSample> timingErrors;
    std::optional<double> meanErrorMilliseconds;
    std::optional<double> varianceMillisecondsSquared;
    std::optional<double> standardDeviationMilliseconds;
};
TaikoResultGroup ResultGroup(TaikoNoteType type) noexcept;
