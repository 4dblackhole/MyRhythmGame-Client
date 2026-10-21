#pragma once
#include "GameFlow/GameplayLaunchRequest.h"
#include "Mode/Submodules/PlaySession.h"
#include <memory>
#include <utility>

// Used on the Scene thread. Consumers take a value snapshot on entry so a
// retained selector cannot mutate an already running play/editor session.
class GameplayLaunchStore final
{
  public:
    void Set(GameplayLaunchRequest request)
    {
        preparedSession_.reset();
        request_ = std::move(request);
    }
    void SetValidated(GameplayLaunchRequest request, std::unique_ptr<PlaySession> session)
    {
        Set(std::move(request));
        preparedSession_ = std::move(session);
    }
    [[nodiscard]] std::unique_ptr<PlaySession> TakeValidatedSession() noexcept
    {
        return std::move(preparedSession_);
    }
    [[nodiscard]] GameplayLaunchRequest Snapshot() const
    {
        return request_;
    }

  private:
    GameplayLaunchRequest request_;
    // Single-use transfer; the gameplay controller owns it after Scene entry.
    std::unique_ptr<PlaySession> preparedSession_;
};
