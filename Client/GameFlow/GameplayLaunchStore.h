#pragma once
#include "GameFlow/GameplayLaunchRequest.h"
#include <utility>

namespace finger_drum
{
    // Used on the Scene thread. Consumers take a value snapshot on entry so a
    // retained selector cannot mutate an already running play/editor session.
    class GameplayLaunchStore final
    {
      public:
        void Set(GameplayLaunchRequest request)
        {
            request_ = std::move(request);
        }
        [[nodiscard]] GameplayLaunchRequest Snapshot() const
        {
            return request_;
        }

      private:
        GameplayLaunchRequest request_;
    };
} // namespace finger_drum
