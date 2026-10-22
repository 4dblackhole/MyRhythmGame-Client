#pragma once
#include "GameFlow/GameplayLaunchRequest.h"
#include "Mode/PlayGameModeFactory.h"
#include <chrono>
#include <future>
#include <stdexcept>

// The worker owns a request snapshot and domain data only. Poll never waits.
class PlaySessionLoader final
{
  public:
    bool Busy() const noexcept { return job_.valid(); }
    void Start(GameplayLaunchRequest request)
    {
        if (Busy())
            throw std::logic_error("A pattern load is already in progress.");
        job_ = std::async(std::launch::async, [request = std::move(request)]
        {
            try
            {
                return CreatePlayGameMode(request.mode,
                    {RhythmDuration{static_cast<std::int64_t>(request.globalOffsetMs) * 1000}, request.inputKeys})
                    ->LoadSession(request.patternPath, request.effectPath);
            }
            catch (const std::exception &error)
            {
                ModeLoadResult result;
                result.diagnostics.push_back({DiagnosticSeverity::Error, {request.patternPath, 1, 1}, error.what()});
                return result;
            }
        });
    }
    std::optional<ModeLoadResult> Poll()
    {
        if (!Busy() || job_.wait_for(std::chrono::seconds{0}) != std::future_status::ready)
            return {};
        return job_.get();
    }
  private:
    std::future<ModeLoadResult> job_;
};
