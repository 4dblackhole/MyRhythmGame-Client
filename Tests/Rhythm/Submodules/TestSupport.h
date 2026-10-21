#pragma once
#include "Catalog/SongCatalog.h"
#include "Judgement/AccuracyRange.h"
#include "Lane/Lane.h"
#include "Mode/PlayGameMode.h"
#include "Note/Submodules/RuleBasedNote.h"
#include "Note/Submodules/TapInputRule.h"
#include "Parsing/ChartParser.h"
#include "Taiko/TaikoMode.h"
#include "Time/RhythmTimer.h"
#include "Timing/MusicalTimeline.h"
#include "Editing/ChartEditor.h"
#include "Audio/EditorAudioAnalysis.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace tests
{

    [[nodiscard]] inline std::unique_ptr<INote> MakeTap(
        NoteId id, RhythmTime time,
        const std::shared_ptr<const AccuracyRange> &profile)
    {
        return std::make_unique<RuleBasedNote>(id, time, profile,
                                                       std::make_unique<TapInputRule>(1));
    }

    inline void Require(const bool condition, const std::string &message)
    {
        if (!condition)
        {
            throw std::runtime_error(message);
        }
    }

    class TemporaryDirectory final
    {
      public:
        explicit TemporaryDirectory(std::string_view name)
        {
            const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
            path_ = std::filesystem::temp_directory_path() /
                    (std::string(name) + "-" + std::to_string(nonce));
            std::filesystem::create_directories(path_);
        }

        ~TemporaryDirectory()
        {
            std::error_code ignored;
            std::filesystem::remove_all(path_, ignored);
        }

        [[nodiscard]] const std::filesystem::path &Path() const noexcept
        {
            return path_;
        }

      private:
        std::filesystem::path path_;
    };

    inline void WriteTextFile(const std::filesystem::path &path, const std::string_view contents)
    {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream stream(path, std::ios::binary);
        if (!stream)
        {
            throw std::runtime_error("Failed to create a temporary chart file.");
        }
        stream.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    }

    [[nodiscard]] inline bool HasEvent(const NoteProcessResult &result,
                                       const NoteEventType type)
    {
        return std::ranges::any_of(
            result.events, [type](const NoteEvent &event) { return event.type == type; });
    }

    [[nodiscard]] inline PatternDocument MakeLongPattern(
        const TaikoNoteType type, std::vector<std::string> extraData = {})
    {
        PatternDocument pattern;
        pattern.mode = "Taiko";
        pattern.baseBpm = 120.0;
        pattern.judgementLevel = 50;
        pattern.notes.push_back(PatternNote{
            .position = {0, Rational{0, 1}},
            .keyType = static_cast<int>(type),
            .actionType = static_cast<int>(TaikoPatternAction::LongNoteStart),
            .extraData = std::move(extraData),
            .sourceOrder = 0});
        pattern.notes.push_back(PatternNote{
            .position = {0, Rational{1, 2}},
            .keyType = static_cast<int>(type),
            .actionType = static_cast<int>(TaikoPatternAction::LongNoteEnd),
            .sourceOrder = 1});
        return pattern;
    }
} // namespace youmu::tests
