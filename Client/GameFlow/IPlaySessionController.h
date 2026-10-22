#pragma once
#include "GameFlow/GameplayFeedback.h"
#include "PhantomCore.h"
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
    virtual bool PrepareSession() = 0;
    virtual bool SessionReady() const noexcept = 0;
    virtual AudioPreparationState PrepareAudioResources(AudioSystem &) = 0;
    virtual void Start(const EngineServices &, RhythmTime initialTime) = 0;
    virtual void Update(const UpdateContext &) = 0;
    virtual PlayState CurrentState() const noexcept = 0;
    virtual void OpenPauseMenu(const UpdateContext &) = 0;
    // true means the command was accepted once. CurrentState remains Pause
    // until background-music preparation and the shared start deadline finish.
    virtual bool ResumeFromPause(const UpdateContext &, RhythmDuration leadIn) = 0;
    virtual bool RewindFromPause(const UpdateContext &, std::size_t measures) = 0;
    virtual void RetryFromPause(const UpdateContext &) = 0;
    virtual void RetireFromPause() = 0;
    virtual bool PauseMenuOpen() const noexcept = 0;
    virtual bool CanContinue() const noexcept = 0;
    virtual const PlaySession &Session() const = 0;
    virtual RhythmTime Time() const noexcept = 0;
    virtual std::span<const GameplayFeedback> Feedback() const noexcept = 0;
    virtual std::string_view ErrorMessage() const noexcept = 0;
    virtual bool ShouldShowResult() const noexcept = 0;
    // Transfer an already finalized Ending result once; never retain play objects.
    virtual std::unique_ptr<IPlayResult> TakeResult() = 0;
    virtual void Shutdown() noexcept = 0;
};
