#pragma once
#include "Catalog/SongCatalog.h"
#include <chrono>
#include <future>
#include <memory>
#include <cstdlib>
#include <cwctype>

// Jobs own copied paths and their parser cache, never Scene/engine observers.
// KeepAlive retains this loader across transitions; polling never waits for I/O.
class SongCatalogLoader final
{
  public:
    void Refresh(const std::filesystem::path &root)
    {
        if (job_.valid())
        {
            pendingRoot_ = root != loadingRoot_ ? std::optional{root} : std::nullopt;
            return;
        }
        if (!cache_ || loadingRoot_ != root)
            cache_ = std::make_shared<SongCatalog>(SongCatalog::MetadataIndex(CachePath(root)));
        loadingRoot_ = root;
        pendingRoot_.reset();
        const auto cache = cache_;
        job_ = std::async(std::launch::async, [cache, root] { return cache->Load(root); });
    }
    std::optional<SongCatalogLoadResult> Poll()
    {
        if (!job_.valid() || job_.wait_for(std::chrono::seconds{0}) != std::future_status::ready)
            return {};
        try
        {
            auto result = job_.get();
            if (pendingRoot_)
            {
                const auto next = *pendingRoot_;
                Refresh(next);
                return {};
            }
            return result;
        }
        catch (const std::exception &error)
        {
            if (pendingRoot_)
            {
                const auto next = *pendingRoot_;
                Refresh(next);
                return {};
            }
            SongCatalogLoadResult result;
            result.diagnostics.push_back({DiagnosticSeverity::Error, {}, error.what()});
            return result;
        }
    }

  private:
    static std::filesystem::path CachePath(const std::filesystem::path &root)
    {
        wchar_t *directory{};
        std::size_t count{};
        if (_wdupenv_s(&directory, &count, L"LOCALAPPDATA") != 0 || !directory)
            return {}; // Read-only in-memory index if no user cache directory is available.
        const std::unique_ptr<wchar_t, decltype(&std::free)> owned(directory, &std::free);
        std::uint64_t hash = 14695981039346656037ULL;
        for (const auto character : root.lexically_normal().generic_wstring())
            hash = (hash ^ static_cast<std::uint64_t>(std::towlower(character))) * 1099511628211ULL;
        return std::filesystem::path{directory} / L"YOUMU" / L"SongCatalog" / (std::to_string(hash) + ".bin");
    }
    std::shared_ptr<SongCatalog> cache_;
    std::future<SongCatalogLoadResult> job_;
    std::filesystem::path loadingRoot_;
    std::optional<std::filesystem::path> pendingRoot_;
};
