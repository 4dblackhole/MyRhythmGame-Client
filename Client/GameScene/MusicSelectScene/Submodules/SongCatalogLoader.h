#pragma once
#include "Catalog/SongCatalog.h"
#include <chrono>
#include <future>
#include <memory>

// Jobs own copied paths and their parser cache, never Scene/engine observers.
// KeepAlive retains this loader across transitions; polling never waits for I/O.
class SongCatalogLoader final
{
  public:
    void Refresh(const std::filesystem::path &root)
    {
        if (job_.valid())
            return;
        const auto cache = cache_;
        job_ = std::async(std::launch::async, [cache, root] { return cache->Load(root); });
    }
    std::optional<finger_drum::chart::SongCatalogLoadResult> Poll()
    {
        if (!job_.valid() || job_.wait_for(std::chrono::seconds{0}) != std::future_status::ready)
            return {};
        try
        {
            return job_.get();
        }
        catch (const std::exception &error)
        {
            finger_drum::chart::SongCatalogLoadResult result;
            result.diagnostics.push_back({finger_drum::chart::DiagnosticSeverity::Error, {}, error.what()});
            return result;
        }
    }

  private:
    std::shared_ptr<finger_drum::chart::SongCatalog> cache_{std::make_shared<finger_drum::chart::SongCatalog>()};
    std::future<finger_drum::chart::SongCatalogLoadResult> job_;
};
