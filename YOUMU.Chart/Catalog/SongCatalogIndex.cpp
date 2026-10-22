#include "Catalog/SongCatalog.h"
#include "Catalog/SongCatalogCacheVersion.h"
#include <atomic>
#include <chrono>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace
{
    constexpr std::uint64_t Magic = 0x315844494d554f59; // YOUMIDX1
    constexpr std::size_t MaximumBytes = 256 * 1024 * 1024;
    constexpr std::uint32_t MaximumItems = 1'000'000;

    std::uint64_t Checksum(std::string_view bytes)
    {
        std::uint64_t value = 14695981039346656037ULL;
        for (const unsigned char byte : bytes)
            value = (value ^ byte) * 1099511628211ULL;
        return value;
    }
    struct Writer
    {
        std::string bytes;
        template <class T> void Number(T value)
        {
            static_assert(std::is_arithmetic_v<T>);
            bytes.append(reinterpret_cast<const char *>(&value), sizeof(value));
        }
        void Count(std::size_t count)
        {
            if (count > MaximumItems)
                throw std::runtime_error("Catalog cache collection is too large.");
            Number(static_cast<std::uint32_t>(count));
        }
        void Text(std::string_view text)
        {
            if (text.size() > MaximumBytes)
                throw std::runtime_error("Catalog cache string is too large.");
            Number(static_cast<std::uint32_t>(text.size()));
            bytes.append(text);
        }
        void Path(const std::filesystem::path &path)
        {
            const auto utf8 = path.generic_u8string();
            Text({reinterpret_cast<const char *>(utf8.data()), utf8.size()});
        }
        void Strings(const std::vector<std::string> &values)
        {
            Count(values.size());
            for (const auto &value : values) Text(value);
        }
    };
    struct Reader
    {
        std::string_view bytes;
        template <class T> T Number()
        {
            static_assert(std::is_arithmetic_v<T>);
            if (bytes.size() < sizeof(T)) throw std::runtime_error("Truncated catalog cache.");
            T value{};
            std::memcpy(&value, bytes.data(), sizeof(T));
            bytes.remove_prefix(sizeof(T));
            return value;
        }
        std::uint32_t Count()
        {
            const auto count = Number<std::uint32_t>();
            if (count > MaximumItems || count > bytes.size())
                throw std::runtime_error("Invalid catalog cache collection size.");
            return count;
        }
        std::string Text()
        {
            const auto size = Number<std::uint32_t>();
            if (size > bytes.size()) throw std::runtime_error("Invalid catalog cache string size.");
            std::string result(bytes.substr(0, size));
            bytes.remove_prefix(size);
            return result;
        }
        std::filesystem::path Path()
        {
            const auto text = Text();
            return std::filesystem::path{std::u8string{
                reinterpret_cast<const char8_t *>(text.data()), text.size()}};
        }
        std::vector<std::string> Strings()
        {
            const auto count = Count();
            std::vector<std::string> values;
            for (std::uint32_t i = 0; i < count; ++i) values.push_back(Text());
            return values;
        }
    };
    void Write(Writer &out, const MusicDocument &doc)
    {
        out.Number(doc.version); out.Path(doc.audioFile);
        out.Strings(doc.names); out.Strings(doc.artists); out.Strings(doc.tags);
    }
    void Read(Reader &in, MusicDocument &doc)
    {
        doc.version = in.Number<int>(); doc.audioFile = in.Path();
        doc.names = in.Strings(); doc.artists = in.Strings(); doc.tags = in.Strings();
    }
    void Write(Writer &out, const PatternDocument &doc)
    {
        out.Number(doc.version); out.Path(doc.musicMetadataFile); out.Path(doc.effectFile);
        out.Path(doc.backgroundFile); out.Number(doc.backgroundOffsetX); out.Number(doc.backgroundOffsetY);
        out.Strings(doc.makers); out.Strings(doc.tags); out.Text(doc.name); out.Text(doc.mode);
        out.Number(doc.patternOffsetMilliseconds); out.Number(doc.baseBpm); out.Number(doc.judgementLevel.Value());
    }
    void Read(Reader &in, PatternDocument &doc)
    {
        doc.version = in.Number<int>(); doc.musicMetadataFile = in.Path(); doc.effectFile = in.Path();
        doc.backgroundFile = in.Path(); doc.backgroundOffsetX = in.Number<double>(); doc.backgroundOffsetY = in.Number<double>();
        doc.makers = in.Strings(); doc.tags = in.Strings(); doc.name = in.Text(); doc.mode = in.Text();
        doc.patternOffsetMilliseconds = in.Number<double>(); doc.baseBpm = in.Number<double>();
        doc.judgementLevel = JudgeLevel{in.Number<double>()};
    }
    // Effect bodies aren't needed by song selection; their parse diagnostics are.
    void Write(Writer &, const EffectDocument &) {}
    void Read(Reader &, EffectDocument &) {}

    template <class Cache> void WriteCache(Writer &out, const Cache &cache)
    {
        out.Count(cache.size());
        for (const auto &[path, entry] : cache)
        {
            out.Path(path);
            out.Number(static_cast<std::int64_t>(entry.modified.time_since_epoch().count()));
            out.Number(static_cast<std::uint64_t>(entry.size));
            out.Number(static_cast<std::uint64_t>(entry.noteCount));
            Write(out, entry.parsed->document);
            out.Count(entry.parsed->diagnostics.size());
            for (const auto &d : entry.parsed->diagnostics)
            {
                out.Number(static_cast<std::uint8_t>(d.severity)); out.Path(d.location.file);
                out.Number(static_cast<std::uint64_t>(d.location.line));
                out.Number(static_cast<std::uint64_t>(d.location.column)); out.Text(d.message);
            }
            if (out.bytes.size() > MaximumBytes) throw std::runtime_error("Catalog cache exceeds its size limit.");
        }
    }
    template <class Cache> void ReadCache(Reader &in, Cache &cache)
    {
        const auto count = in.Count();
        for (std::uint32_t i = 0; i < count; ++i)
        {
            const auto path = in.Path();
            const auto modified = std::filesystem::file_time_type{
                std::filesystem::file_time_type::duration{in.Number<std::int64_t>()}};
            const auto size = in.Number<std::uint64_t>();
            const auto noteCount = in.Number<std::uint64_t>();
            auto parsed = std::make_shared<typename Cache::mapped_type::Parsed>();
            parsed->document.sourcePath = path;
            Read(in, parsed->document);
            const auto diagnostics = in.Count();
            for (std::uint32_t j = 0; j < diagnostics; ++j)
            {
                Diagnostic d;
                const auto severity = in.Number<std::uint8_t>();
                if (severity > static_cast<std::uint8_t>(DiagnosticSeverity::Error))
                    throw std::runtime_error("Invalid catalog cache diagnostic.");
                d.severity = static_cast<DiagnosticSeverity>(severity); d.location.file = in.Path();
                d.location.line = static_cast<std::size_t>(in.Number<std::uint64_t>());
                d.location.column = static_cast<std::size_t>(in.Number<std::uint64_t>()); d.message = in.Text();
                parsed->diagnostics.push_back(std::move(d));
            }
            if (!cache.emplace(path, typename Cache::mapped_type{modified, size, std::move(parsed),
                static_cast<std::size_t>(noteCount)}).second)
                throw std::runtime_error("Duplicate catalog cache record.");
        }
    }
}

SongCatalog SongCatalog::MetadataIndex(std::filesystem::path cacheFile)
{
    SongCatalog catalog;
    catalog.metadataOnly_ = true;
    catalog.cacheFile_ = std::move(cacheFile);
    return catalog;
}

void SongCatalog::ReadIndex(const std::filesystem::path &root) const noexcept
{
    try
    {
        std::ifstream file(cacheFile_, std::ios::binary | std::ios::ate);
        if (!file || file.tellg() < 24 || file.tellg() > static_cast<std::streamoff>(MaximumBytes)) return;
        std::string bytes(static_cast<std::size_t>(file.tellg()), '\0');
        file.seekg(0);
        if (!file.read(bytes.data(), static_cast<std::streamsize>(bytes.size()))) return;
        Reader in{bytes};
        if (in.Number<std::uint64_t>() != Magic || in.Number<std::uint32_t>() != SongCatalogCacheSchema ||
            in.Number<std::uint32_t>() != SongCatalogParserRevision) return;
        const auto checksum = in.Number<std::uint64_t>();
        if (Checksum(in.bytes) != checksum || in.Path() != root) return;
        // Publish only after the entire file has passed validation.
        decltype(musicCache_) music; decltype(patternCache_) patterns; decltype(effectCache_) effects;
        ReadCache(in, music); ReadCache(in, patterns); ReadCache(in, effects);
        if (!in.bytes.empty()) return;
        musicCache_ = std::move(music); patternCache_ = std::move(patterns); effectCache_ = std::move(effects);
    }
    catch (...) { /* An optional, disposable index must never prevent source loading. */ }
}

void SongCatalog::WriteIndex(const std::filesystem::path &root) const noexcept
{
    std::filesystem::path temporary;
    try
    {
        if (cacheFile_.empty()) return;
        std::error_code statusError;
        const auto status = std::filesystem::symlink_status(cacheFile_, statusError);
        if (!statusError && std::filesystem::exists(status) && !std::filesystem::is_regular_file(status)) return;
        Writer payload;
        payload.Path(root);
        WriteCache(payload, musicCache_); WriteCache(payload, patternCache_); WriteCache(payload, effectCache_);
        Writer header;
        header.Number(Magic); header.Number(SongCatalogCacheSchema); header.Number(SongCatalogParserRevision);
        header.Number(Checksum(payload.bytes));
        if (payload.bytes.size() + header.bytes.size() > MaximumBytes) return;
        std::filesystem::create_directories(cacheFile_.parent_path());
        static std::atomic<std::uint64_t> sequence{};
        temporary = cacheFile_;
        temporary += "." + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
                     "." + std::to_string(sequence.fetch_add(1)) + ".tmp";
        {
            std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
            file.write(header.bytes.data(), static_cast<std::streamsize>(header.bytes.size()));
            file.write(payload.bytes.data(), static_cast<std::streamsize>(payload.bytes.size()));
            file.close();
            if (!file) throw std::runtime_error("Could not write catalog cache.");
        }
        std::error_code error;
        std::filesystem::rename(temporary, cacheFile_, error);
        if (error)
        {
            // Windows rename doesn't replace an existing file. A concurrent
            // reader may rebuild during this gap; it never receives partial data.
            std::filesystem::remove(cacheFile_, error);
            std::filesystem::rename(temporary, cacheFile_);
        }
        cacheDirty_ = false;
    }
    catch (...)
    {
        std::error_code ignored;
        if (!temporary.empty()) std::filesystem::remove(temporary, ignored);
    }
}
