#pragma once

#include "Model/Submodules/MusicDocument.h"
#include "Model/Submodules/PatternDocument.h"
#include "Model/Submodules/EffectDocument.h"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <vector>
#include <map>
#include <memory>

struct SongCatalogPattern
{
    std::filesystem::path patternPath;
    std::optional<std::filesystem::path> effectPath;
    PatternDocument pattern;
    std::optional<std::size_t> noteCount;
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
    SongCatalog() = default;
    // List-only projection: validates changed source files in full, then drops
    // note/timing/effect bodies. Gameplay must still load the selected pattern.
    [[nodiscard]] static SongCatalog MetadataIndex(std::filesystem::path cacheFile);
    [[nodiscard]] SongCatalogLoadResult Load(
        const std::filesystem::path& songsRoot) const;

  private:
    void ReadIndex(const std::filesystem::path &root) const noexcept;
    void WriteIndex(const std::filesystem::path &root) const noexcept;
    bool metadataOnly_{};
    std::filesystem::path cacheFile_;
    mutable std::filesystem::path cachedRoot_;
    mutable bool cacheDirty_{};
    template <class Document> struct CachedDocument
    {
        using Parsed = ParseResult<Document>;
        std::filesystem::file_time_type modified;
        std::uintmax_t size{};
        std::shared_ptr<const ParseResult<Document>> parsed;
        std::size_t noteCount{};
    };
    mutable std::map<std::filesystem::path, CachedDocument<MusicDocument>> musicCache_;
    mutable std::map<std::filesystem::path, CachedDocument<PatternDocument>> patternCache_;
    mutable std::map<std::filesystem::path, CachedDocument<EffectDocument>> effectCache_;
};
