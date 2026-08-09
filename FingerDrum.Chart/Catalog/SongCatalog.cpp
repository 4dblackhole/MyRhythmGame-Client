#include "Catalog/SongCatalog.h"

#include "Parsing/ChartParser.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <string>
#include <utility>

namespace finger_drum::chart
{
    namespace
    {
        [[nodiscard]] std::string PathKey(
            const std::filesystem::path& path)
        {
            const std::u8string utf8 = path.lexically_normal().generic_u8string();
            std::string key(
                reinterpret_cast<const char*>(utf8.data()),
                utf8.size());
            std::ranges::transform(
                key,
                key.begin(),
                [](const unsigned char value)
                {
                    return static_cast<char>(std::tolower(value));
                });
            return key;
        }

        [[nodiscard]] std::string DisplaySortKey(
            const SongCatalogEntry& song)
        {
            std::string value = song.music.names.empty()
                ? song.metadataPath.stem().string()
                : song.music.names.front();
            std::ranges::transform(
                value,
                value.begin(),
                [](const unsigned char character)
                {
                    return static_cast<char>(std::tolower(character));
                });
            return value;
        }

        void AppendDiagnostics(
            std::vector<Diagnostic>& destination,
            std::vector<Diagnostic> source)
        {
            destination.insert(
                destination.end(),
                std::make_move_iterator(source.begin()),
                std::make_move_iterator(source.end()));
        }
    }

    std::size_t SongCatalogLoadResult::PatternCount() const noexcept
    {
        std::size_t count{};
        for (const SongCatalogEntry& song : songs)
        {
            count += song.patterns.size();
        }
        return count;
    }

    bool SongCatalogLoadResult::Succeeded() const noexcept
    {
        return std::ranges::none_of(
            diagnostics,
            [](const Diagnostic& diagnostic)
            {
                return diagnostic.severity == DiagnosticSeverity::Error;
            });
    }

    SongCatalogLoadResult SongCatalog::Load(
        const std::filesystem::path& songsRoot) const
    {
        SongCatalogLoadResult result;
        if (!std::filesystem::is_directory(songsRoot))
        {
            result.diagnostics.push_back({
                DiagnosticSeverity::Error,
                {songsRoot, 1, 1},
                "The Songs catalog directory does not exist."});
            return result;
        }

        ChartParser parser;
        std::map<std::string, std::size_t, std::less<>> songByMetadataPath;

        // Load music first so pattern references can be joined in one pass.
        for (const std::filesystem::directory_entry& entry :
            std::filesystem::recursive_directory_iterator(songsRoot))
        {
            if (!entry.is_regular_file() || entry.path().extension() != L".ymm")
            {
                continue;
            }
            ++result.discoveredMusicFiles;
            ParseResult<MusicDocument> parsed =
                parser.ParseMusicFile(entry.path());
            AppendDiagnostics(result.diagnostics, std::move(parsed.diagnostics));

            SongCatalogEntry song;
            song.metadataPath = entry.path();
            song.audioPath = entry.path().parent_path() /
                parsed.document.audioFile;
            song.music = std::move(parsed.document);
            const std::size_t index = result.songs.size();
            songByMetadataPath.emplace(PathKey(entry.path()), index);
            result.songs.push_back(std::move(song));
        }

        for (const std::filesystem::directory_entry& entry :
            std::filesystem::recursive_directory_iterator(songsRoot))
        {
            if (!entry.is_regular_file() || entry.path().extension() != L".ymp")
            {
                continue;
            }
            ++result.discoveredPatternFiles;
            ParseResult<PatternDocument> parsed =
                parser.ParsePatternFile(entry.path());
            AppendDiagnostics(result.diagnostics, std::move(parsed.diagnostics));

            const std::filesystem::path referencedMetadata =
                songsRoot / parsed.document.musicMetadataFile;
            const auto target = songByMetadataPath.find(
                PathKey(referencedMetadata));
            if (target == songByMetadataPath.end())
            {
                result.diagnostics.push_back({
                    DiagnosticSeverity::Error,
                    {entry.path(), 1, 1},
                    "The pattern references a Music metadata file that was not found."});
                continue;
            }

            SongCatalogPattern pattern;
            pattern.patternPath = entry.path();
            const std::filesystem::path effectPath =
                entry.path().parent_path() /
                (entry.path().stem().wstring() + L".yme");
            if (std::filesystem::is_regular_file(effectPath))
            {
                pattern.effectPath = effectPath;
            }
            pattern.pattern = std::move(parsed.document);
            result.songs[target->second].patterns.push_back(
                std::move(pattern));
        }

        for (SongCatalogEntry& song : result.songs)
        {
            std::ranges::stable_sort(
                song.patterns,
                [](const SongCatalogPattern& left,
                   const SongCatalogPattern& right)
                {
                    return left.pattern.name < right.pattern.name;
                });
        }
        std::ranges::stable_sort(
            result.songs,
            [](const SongCatalogEntry& left, const SongCatalogEntry& right)
            {
                return DisplaySortKey(left) < DisplaySortKey(right);
            });
        return result;
    }
}
