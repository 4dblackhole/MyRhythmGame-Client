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
            ? PathKey(song.metadataPath.stem())
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
        const std::vector<Diagnostic>& source)
    {
        destination.insert(
            destination.end(),
            source.begin(), source.end());
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
        std::vector<std::filesystem::directory_entry> music;
        std::vector<std::filesystem::directory_entry> patterns;
        std::map<std::filesystem::path, std::filesystem::directory_entry> metadata;
    };

    template <class Cache, class Parser>
    auto ParseChangedFile(Cache &cache, const std::filesystem::directory_entry &file, Parser parse,
                         bool &dirty, bool metadataOnly = false)
    {
        const auto &path = file.path();
        using Parsed = typename Cache::mapped_type::Parsed;
        const auto read = [&](auto modified, auto size)
        {
            auto parsed = parse(path);
            std::size_t noteCount{};
            if constexpr (requires { parsed.document.notes; })
            {
                noteCount = parsed.document.notes.size();
                if (metadataOnly)
                {
                    decltype(parsed.document.notes){}.swap(parsed.document.notes);
                    decltype(parsed.document.timing){}.swap(parsed.document.timing);
                    decltype(parsed.document.systemBreakMeasures){}.swap(parsed.document.systemBreakMeasures);
                }
            }
            if constexpr (requires { parsed.document.commands; })
                if (metadataOnly) parsed.document = {};
            return typename Cache::mapped_type{modified, size,
                std::make_shared<const Parsed>(std::move(parsed)), noteCount};
        };
        std::error_code error;
        const auto modified = file.last_write_time(error);
        if (error)
            return read(std::filesystem::file_time_type{}, std::uintmax_t{});
        const auto size = file.file_size(error);
        if (error)
            return read(std::filesystem::file_time_type{}, std::uintmax_t{});
        if (const auto found = cache.find(path);
            found != cache.end() && found->second.modified == modified && found->second.size == size)
            return found->second;
        auto entry = read(modified, size);
        cache.insert_or_assign(path, entry);
        dirty = true;
        return entry;
    }

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
                    files.music.push_back(*iterator);
                }
                else if (extension == L".ymp")
                {
                    files.patterns.push_back(*iterator);
                }
                if (extension == L".ymm" || extension == L".ymp" || extension == L".yme")
                    files.metadata.emplace(path, *iterator);
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

        const auto byPath = [](const auto &entry) -> const std::filesystem::path & { return entry.path(); };
        std::ranges::sort(files.music, {}, byPath);
        std::ranges::sort(files.patterns, {}, byPath);
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
    if (metadataOnly_ && cachedRoot_ != songsRoot)
    {
        musicCache_.clear();
        patternCache_.clear();
        effectCache_.clear();
        cachedRoot_ = songsRoot;
        ReadIndex(songsRoot);
    }
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
    const auto deleted = [&files](const auto &entry)
    {
        if (files.metadata.contains(entry.first)) return false;
        std::error_code error;
        const bool exists = std::filesystem::exists(entry.first, error);
        return !exists && !error;
    };
    cacheDirty_ |= std::erase_if(musicCache_, deleted) != 0;
    cacheDirty_ |= std::erase_if(patternCache_, deleted) != 0;
    cacheDirty_ |= std::erase_if(effectCache_, deleted) != 0;
    result.discoveredMusicFiles = files.music.size();
    result.discoveredPatternFiles = files.patterns.size();

    // Load music first so pattern references can be joined in one pass.
    for (const auto &file : files.music)
    {
        const auto &path = file.path();
        std::shared_ptr<const ParseResult<MusicDocument>> parsed;
        try
        {
            parsed = ParseChangedFile(musicCache_, file,
                                      [&parser](const auto &file) { return parser.ParseMusicFile(file); }, cacheDirty_).parsed;
        }
        catch (const std::exception& exception)
        {
            AddCatalogError(result, path, exception.what());
            continue;
        }
        const bool valid = parsed->Succeeded();
        AppendDiagnostics(result.diagnostics, parsed->diagnostics);
        if (!valid)
        {
            continue;
        }

        SongCatalogEntry song;
        song.metadataPath = path;
        song.audioPath = path.parent_path() /
            parsed->document.audioFile;
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
        song.music = parsed->document;
        const std::size_t index = result.songs.size();
        songByMetadataPath.emplace(PathKey(path), index);
        result.songs.push_back(std::move(song));
    }

    for (const auto &file : files.patterns)
    {
        const auto &path = file.path();
        std::shared_ptr<const ParseResult<PatternDocument>> parsed;
        std::size_t noteCount{};
        try
        {
            const auto cached = ParseChangedFile(patternCache_, file,
                [&parser](const auto &file) { return parser.ParsePatternFile(file); }, cacheDirty_, metadataOnly_);
            parsed = cached.parsed;
            noteCount = cached.noteCount;
        }
        catch (const std::exception& exception)
        {
            AddCatalogError(result, path, exception.what());
            continue;
        }
        const bool valid = parsed->Succeeded();
        AppendDiagnostics(result.diagnostics, parsed->diagnostics);
        if (!valid)
        {
            continue;
        }

        const std::filesystem::path referencedMetadata =
            songsRoot / parsed->document.musicMetadataFile;
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
            parsed->document.effectFile.empty() ? std::filesystem::path{} :
            (path.parent_path() / parsed->document.effectFile).lexically_normal();
        std::error_code effectError;
        const auto discoveredEffect = files.metadata.find(effectPath);
        const bool effectExists =
            discoveredEffect != files.metadata.end() ||
            (!effectPath.empty() && std::filesystem::exists(effectPath, effectError));
        if (effectError)
        {
            AddCatalogError(
                result,
                effectPath,
                "Unable to inspect the matching effect file: " +
                    effectError.message());
            continue;
        }
        if (!effectPath.empty() && !effectExists)
        {
            AddCatalogError(result, effectPath, "Referenced Effect file was not found.");
            continue;
        }
        if (effectExists)
        {
            std::error_code effectTypeError;
            if (discoveredEffect == files.metadata.end() && !std::filesystem::is_regular_file(
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
            std::shared_ptr<const ParseResult<EffectDocument>> effects;
            try
            {
                const auto effectFile = discoveredEffect == files.metadata.end()
                    ? std::filesystem::directory_entry{effectPath} : discoveredEffect->second;
                effects = ParseChangedFile(effectCache_, effectFile,
                    [&parser](const auto &file) { return parser.ParseEffectFile(file); }, cacheDirty_, metadataOnly_).parsed;
            }
            catch (const std::exception& exception)
            {
                AddCatalogError(result, effectPath, exception.what());
                continue;
            }
            const bool effectsValid = effects->Succeeded();
            AppendDiagnostics(
                result.diagnostics,
                effects->diagnostics);
            if (!effectsValid)
            {
                continue;
            }
            pattern.effectPath = effectPath;
        }
        pattern.pattern = parsed->document;
        pattern.noteCount = noteCount;
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
    // Normalize once per song, rather than allocating strings in every comparison.
    std::vector<std::pair<std::string, std::size_t>> order;
    order.reserve(result.songs.size());
    for (std::size_t i = 0; i < result.songs.size(); ++i)
        order.emplace_back(DisplaySortKey(result.songs[i]), i);
    std::ranges::stable_sort(order, {}, &decltype(order)::value_type::first);
    std::vector<SongCatalogEntry> sorted;
    sorted.reserve(result.songs.size());
    for (const auto &[key, index] : order)
        sorted.push_back(std::move(result.songs[index]));
    result.songs = std::move(sorted);
    if (metadataOnly_ && cacheDirty_)
        WriteIndex(songsRoot);
    return result;
}
