#pragma once
#include "Mode/Results/IPlayResult.h"
#include <filesystem>
#include <string>
#include <vector>

struct PlayRecord
{
    std::int64_t id{}, playedAtMilliseconds{};
    std::filesystem::path patternPath;
    std::string mode;
    AccuracyScore accuracy;
    std::size_t totalNotes{}, processedNotes{};
    bool completed{}, cleared{}, pauseMenuUsed{};
    JudgeLevel judgeLevel;
    PlayOptions options;
    RhythmDuration globalOffset{};
    static PlayRecord From(const IPlayResult &result);
};
class PlayRecordStore final
{
  public:
    static std::filesystem::path DefaultPath();
    explicit PlayRecordStore(std::filesystem::path database) : path_(std::move(database))
    {
    }
    void Save(const PlayRecord &record) const;
    // Supported conditions: named option comparisons, AND/OR/NOT, parentheses.
    // Unknown fields, arbitrary SQL and ill-typed values are rejected.
    static void ValidateQuery(std::string_view condition);
    std::vector<PlayRecord> Query(const std::filesystem::path &pattern, std::string_view mode,
                                  std::string_view condition, std::size_t offset, std::size_t limit) const;

  private:
    std::filesystem::path path_;
};
