#include "Catalog/SongCatalog.h"

#include "Parsing/ChartParser.h"

#include <algorithm>
#include <cctype>
#include <cwctype>
#include <map>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

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

        void AddCatalogError(
            SongCatalogLoadResult& result,
            const std::filesystem::path& path,
            std::string message)
        {
            result.diagnostics.push_back({
                DiagnosticSeverity::Error,
                {path, 1, 1},
                std::move(message)});
        }

        [[nodiscard]] std::wstring LowercaseExtension(
            const std::filesystem::path& path)
        {
            std::wstring extension = path.extension().wstring();
            std::ranges::transform(
                extension,
                extension.begin(),
                [](const wchar_t character)
                {
                    return static_cast<wchar_t>(std::towlower(character));
                });
            return extension;
        }

        struct CatalogFiles final
        {
            std::vector<std::filesystem::path> music;
            std::vector<std::filesystem::path> patterns;
        };

        [[nodiscard]] CatalogFiles DiscoverCatalogFiles(
            const std::filesystem::path& songsRoot,
            SongCatalogLoadResult& result)
        {
            CatalogFiles files;
            std::error_code error;
            std::filesystem::recursive_directory_iterator iterator(
                songsRoot,
                std::filesystem::directory_options::skip_permission_denied,
                error);
            const std::filesystem::recursive_directory_iterator end;
            if (error)
            {
                AddCatalogError(
                    result,
                    songsRoot,
                    "Unable to scan the Songs catalog: " + error.message());
                return files;
            }

            while (iterator != end)
            {
                const std::filesystem::path path = iterator->path();
                std::error_code typeError;
                if (iterator->is_regular_file(typeError))
                {
                    const std::wstring extension = LowercaseExtension(path);
                    if (extension == L".ymm")
                    {
                        files.music.push_back(path);
                    }
                    else if (extension == L".ymp")
                    {
                        files.patterns.push_back(path);
                    }
                }
                else if (typeError)
                {
                    AddCatalogError(
                        result,
                        path,
                        "Unable to inspect a catalog entry: " +
                            typeError.message());
                }

                iterator.increment(error);
                if (error)
                {
                    AddCatalogError(
                        result,
                        path,
                        "Unable to continue scanning a catalog directory: " +
                            error.message());
                    error.clear();
                }
            }

            std::ranges::sort(files.music);
            std::ranges::sort(files.patterns);
            return files;
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
        std::error_code rootError;
        if (!std::filesystem::is_directory(songsRoot, rootError))
        {
            AddCatalogError(
                result,
                songsRoot,
                rootError
                    ? "Unable to inspect the Songs catalog directory: " +
                        rootError.message()
                    : "The Songs catalog directory does not exist.");
            return result;
        }

        ChartParser parser;
        std::map<std::string, std::size_t, std::less<>> songByMetadataPath;
        CatalogFiles files = DiscoverCatalogFiles(songsRoot, result);
        result.discoveredMusicFiles = files.music.size();
        result.discoveredPatternFiles = files.patterns.size();

        // Load music first so pattern references can be joined in one pass.
        for (const std::filesystem::path& path : files.music)
        {
            ParseResult<MusicDocument> parsed;
            try
            {
                parsed = parser.ParseMusicFile(path);
            }
            catch (const std::exception& exception)
            {
                AddCatalogError(result, path, exception.what());
                continue;
            }
            const bool valid = parsed.Succeeded();
            AppendDiagnostics(result.diagnostics, std::move(parsed.diagnostics));
            if (!valid)
            {
                continue;
            }

            SongCatalogEntry song;
            song.metadataPath = path;
            song.audioPath = path.parent_path() /
                parsed.document.audioFile;
            std::error_code audioError;
            const bool audioExists =
                std::filesystem::exists(song.audioPath, audioError);
            if (audioError || !audioExists)
            {
                AddCatalogError(
                    result,
                    song.audioPath,
                    audioError
                        ? "Unable to inspect the referenced audio file: " +
                            audioError.message()
                        : "The referenced audio file does not exist.");
                continue;
            }
            std::error_code audioTypeError;
            if (!std::filesystem::is_regular_file(
                    song.audioPath,
                    audioTypeError))
            {
                AddCatalogError(
                    result,
                    song.audioPath,
                    audioTypeError
                        ? "Unable to inspect the referenced audio file: " +
                            audioTypeError.message()
                        : "The referenced audio path is not a regular file.");
                continue;
            }
            song.music = std::move(parsed.document);
            const std::size_t index = result.songs.size();
            songByMetadataPath.emplace(PathKey(path), index);
            result.songs.push_back(std::move(song));
        }

        for (const std::filesystem::path& path : files.patterns)
        {
            ParseResult<PatternDocument> parsed;
            try
            {
                parsed = parser.ParsePatternFile(path);
            }
            catch (const std::exception& exception)
            {
                AddCatalogError(result, path, exception.what());
                continue;
            }
            const bool valid = parsed.Succeeded();
            AppendDiagnostics(result.diagnostics, std::move(parsed.diagnostics));
            if (!valid)
            {
                continue;
            }

            const std::filesystem::path referencedMetadata =
                songsRoot / parsed.document.musicMetadataFile;
            const auto target = songByMetadataPath.find(
                PathKey(referencedMetadata));
            if (target == songByMetadataPath.end())
            {
                result.diagnostics.push_back({
                    DiagnosticSeverity::Error,
                    {path, 1, 1},
                    "The pattern references a Music metadata file that was not found."});
                continue;
            }

            SongCatalogPattern pattern;
            pattern.patternPath = path;
            const std::filesystem::path effectPath =
                path.parent_path() /
                (path.stem().wstring() + L".yme");
            std::error_code effectError;
            const bool effectExists =
                std::filesystem::exists(effectPath, effectError);
            if (effectError)
            {
                AddCatalogError(
                    result,
                    effectPath,
                    "Unable to inspect the matching effect file: " +
                        effectError.message());
                continue;
            }
            if (effectExists)
            {
                std::error_code effectTypeError;
                if (!std::filesystem::is_regular_file(
                        effectPath,
                        effectTypeError))
                {
                    AddCatalogError(
                        result,
                        effectPath,
                        effectTypeError
                            ? "Unable to inspect the matching effect file: " +
                                effectTypeError.message()
                            : "The matching effect path is not a regular file.");
                    continue;
                }
                ParseResult<EffectDocument> effects;
                try
                {
                    effects = parser.ParseEffectFile(effectPath);
                }
                catch (const std::exception& exception)
                {
                    AddCatalogError(result, effectPath, exception.what());
                    continue;
                }
                const bool effectsValid = effects.Succeeded();
                AppendDiagnostics(
                    result.diagnostics,
                    std::move(effects.diagnostics));
                if (!effectsValid)
                {
                    continue;
                }
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
