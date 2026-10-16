#pragma once
#include "GameFlow/GameplayFeedback.h"
#include "MRG_Core.h"
#include "Mode/Submodules/PlaySession.h"
#include <memory>
#include <span>
#include <string_view>

// Every gameplay Scene uses the same lifecycle, independent of its note rules.
enum class PlayState
{
    Loading,
    Gameplay,
    Pause,
    Ending
};

class IPlaySessionController
{
  public:
    virtual ~IPlaySessionController() = default;
    virtual void InitializeSession() = 0;
    virtual void Start(const mrg::EngineServices &, finger_drum::rhythm::RhythmTime initialTime) = 0;
    virtual void Update(const mrg::UpdateContext &) = 0;
    virtual PlayState CurrentState() const noexcept = 0;
    virtual void OpenPauseMenu(const mrg::UpdateContext &) = 0;
    // true means the command was accepted once. CurrentState remains Pause
    // until background-music preparation and the shared start deadline finish.
    virtual bool ResumeFromPause(const mrg::UpdateContext &, finger_drum::rhythm::RhythmDuration leadIn) = 0;
    virtual bool RewindFromPause(const mrg::UpdateContext &, std::size_t measures) = 0;
    virtual void RetryFromPause(const mrg::UpdateContext &) = 0;
    virtual void RetireFromPause() = 0;
    virtual bool PauseMenuOpen() const noexcept = 0;
    virtual bool CanContinue() const noexcept = 0;
    virtual const finger_drum::mode::PlaySession &Session() const = 0;
    virtual finger_drum::rhythm::RhythmTime Time() const noexcept = 0;
    virtual std::span<const GameplayFeedback> Feedback() const noexcept = 0;
    virtual std::string_view ErrorMessage() const noexcept = 0;
    virtual bool ShouldShowResult() const noexcept = 0;
    // Transfer an already finalized Ending result once; never retain play objects.
    virtual std::unique_ptr<finger_drum::mode::IPlayResult> TakeResult() = 0;
    virtual void Shutdown() noexcept = 0;
};
