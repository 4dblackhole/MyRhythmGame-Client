#pragma once

#include "Judgement/AccuracyScore.h"
#include "Note/Submodules/NoteTypes.h"
#include <filesystem>
#include <map>
#include <string>
#include <string_view>

namespace finger_drum::mode
{
    enum class PlayEndReason
    {
        Completed,
        Interrupted
    };

    struct PlayResultSummary
    {
        std::filesystem::path patternPath;
        std::string patternName;
        rhythm::AccuracyScore accuracy;
        std::size_t totalNotes{};
        std::size_t processedNotes{};
        std::size_t unprocessedNotes{};
        PlayEndReason endReason{PlayEndReason::Interrupted};
        rhythm::RhythmTime initialTime{};
        rhythm::RhythmTime endTime{};
        std::size_t judgementLevel{};
        std::map<rhythm::PhysicalKey, rhythm::NoteAction> inputMapping;
        // Append order resolves ties at the same microsecond. These are the
        // exact mapped press/release events passed to judgement, including blanks.
        std::vector<rhythm::RhythmInputEvent> inputs;
    };

    class IPlayResult
    {
      public:
        virtual ~IPlayResult() = default;
        virtual std::string_view ModeId() const noexcept = 0;
        virtual const PlayResultSummary &Summary() const noexcept = 0;
    };
} // namespace finger_drum::mode
