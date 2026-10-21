#pragma once

#include "Judgement/AccuracyScore.h"
#include "Judgement/JudgeLevel.h"
#include "Mode/Results/PlayControlEvent.h"
#include "Note/Submodules/NoteTypes.h"
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

enum class PlayEndReason
{
    Completed,
    Interrupted
};

struct PlayResultSummary
{
    std::filesystem::path patternPath;
    std::string patternName;
    AccuracyScore accuracy;
    std::size_t totalNotes{};
    std::size_t processedNotes{};
    std::size_t unprocessedNotes{};
    PlayEndReason endReason{PlayEndReason::Interrupted};
    RhythmTime initialTime{};
    RhythmTime endTime{};
    JudgeLevel judgementLevel;
    std::map<PhysicalKey, NoteAction> inputMapping;
    // Append order resolves ties at the same microsecond. These are the
    // exact mapped press/release events passed to judgement, including blanks.
    std::vector<RhythmInputEvent> inputs;
    std::vector<PlayControlEvent> controls;
    RhythmDuration globalOffset{}; // Reproduce the compiled timing in a future replay.
    bool pauseMenuUsed{};
    // Future HP/record systems must use these gates, not completion alone.
    bool CanUseHp() const noexcept
    {
        return !pauseMenuUsed;
    }
    bool IsCleared() const noexcept
    {
        return endReason == PlayEndReason::Completed && !pauseMenuUsed;
    }
};

class IPlayResult
{
  public:
    virtual ~IPlayResult() = default;
    virtual std::string_view ModeId() const noexcept = 0;
    virtual const PlayResultSummary &Summary() const noexcept = 0;
};
