#pragma once

#include "Model/Submodules/MusicDocument.h"
#include "Model/Submodules/PatternDocument.h"
#include "Model/Submodules/EffectDocument.h"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <vector>
#include <map>

struct SongCatalogPattern
{
    std::filesystem::path patternPath;
    std::optional<std::filesystem::path> effectPath;
    PatternDocument pattern;
};

struct SongCatalogEntry
{
    std::filesystem::path metadataPath;
    std::filesystem::path audioPath;
    MusicDocument music;
    std::vector<SongCatalogPattern> patterns;
};

struct SongCatalogLoadResult
{
    std::vector<SongCatalogEntry> songs;
    std::vector<Diagnostic> diagnostics;
    std::size_t discoveredMusicFiles{};
    std::size_t discoveredPatternFiles{};

    [[nodiscard]] std::size_t PatternCount() const noexcept;
    [[nodiscard]] bool Succeeded() const noexcept;
};

// Scans the runtime Songs root and joins YMP patterns to the YMM path
// referenced by their `Music metadata` field. The catalog owns the parsed
// documents, so Scenes can safely retain selections after scanning ends.
class SongCatalog final
{
public:
    [[nodiscard]] SongCatalogLoadResult Load(
        const std::filesystem::path& songsRoot) const;

  private:
    template <class Document> struct CachedDocument
    {
        std::filesystem::file_time_type modified;
        std::uintmax_t size{};
        ParseResult<Document> parsed;
    };
    mutable std::map<std::filesystem::path, CachedDocument<MusicDocument>> musicCache_;
    mutable std::map<std::filesystem::path, CachedDocument<PatternDocument>> patternCache_;
    mutable std::map<std::filesystem::path, CachedDocument<EffectDocument>> effectCache_;
};
