#pragma once
#include "Audio/GameplayAudioRouter.h"
#include "GameFlow/GameplayLaunchRequest.h"
#include "GameFlow/IPlaySessionController.h"
#include "GameFlow/PlaySessionLoader.h"
#include "PhantomCore.h"
#include "Time/RhythmTimer.h"
#include <memory>
#include <span>
#include <vector>

class GameplaySessionController final : public IPlaySessionController
{
  public:
    GameplaySessionController(GameplayLaunchRequest request, AudioPlaybackManager &playback,
                              bool debug, std::unique_ptr<PlaySession> prepared = {})
        : launchRequest_(std::move(request)), session_(std::move(prepared)), audioRouter_(playback), debugMode_(debug)
    {
    }
    ~GameplaySessionController() override
    {
        Shutdown();
    }
    void InitializeSession() override;
    bool PrepareSession() override;
    bool SessionReady() const noexcept override { return sessionInitialized_; }
    AudioPreparationState PrepareAudioResources(AudioSystem &audio) override;
    void Start(const EngineServices &services, RhythmTime initialTime) override;
    void Update(const UpdateContext &context) override;
    PlayState CurrentState() const noexcept override
    {
        return state_;
    }
    void OpenPauseMenu(const UpdateContext &context) override;
    bool ResumeFromPause(const UpdateContext &context, RhythmDuration leadIn) override;
    bool RewindFromPause(const UpdateContext &context, std::size_t measures) override;
    void RetryFromPause(const UpdateContext &context) override;
    void RetireFromPause() override;
    bool PauseMenuOpen() const noexcept override
    {
        return pauseMenuOpen_;
    }
    bool CanContinue() const noexcept override
    {
        return state_ == PlayState::Pause && resourcesReady_ && !playbackRequested_;
    }
    void Shutdown() noexcept override;
    const PlaySession &Session() const override
    {
        return *session_;
    }
    RhythmTime Time() const noexcept override
    {
        return time_;
    }
    std::span<const GameplayFeedback> Feedback() const noexcept override
    {
        return feedback_;
    }
    std::string_view ErrorMessage() const noexcept override;
    bool ShouldShowResult() const noexcept override
    {
        return state_ == PlayState::Ending && result_ != nullptr;
    }
    std::unique_ptr<IPlayResult> TakeResult() override;

  private:
#if defined(_DEBUG)
    static constexpr bool ReferenceTimeDebug = true;
#else
    static constexpr bool ReferenceTimeDebug = false;
#endif
    std::vector<PhysicalKey> heldKeys_;
    std::unique_ptr<PlaySession> CreateSession();
    std::unique_ptr<PlaySession> CreateDemoSession();
    std::unique_ptr<PlaySession> CreateLongNoteDebugSession();
    void PrepareDebugLaunchRequest();
    void InitializeAudio(AudioSystem &audio);
    void UpdateLoading(const UpdateContext &context);
    void UpdateGameplay(const UpdateContext &context);
    void UpdatePause(const UpdateContext &context);
    bool PrepareResources(const UpdateContext &context);
    void PrepareGameplay(const UpdateContext &context);
    void ScheduleGameplay(const UpdateContext &context);
    void FailPlaybackPreparation();
    void EnterEnding(PlayEndReason reason);
    void PauseAt(std::int64_t ticks, bool menuShown);
    bool ContinueAt(const UpdateContext &context, RhythmTime target,
                    PlayControlType action);
    void ResetTimeline();
    void ProcessControlKeys(const UpdateContext &context, const AudioClockSnapshot &clock);
    void ProcessDebugTimeline(const InputState &input, const AudioClockSnapshot &clock,
                              const double deltaSeconds);
    void ProcessRhythmInput(const InputState &input, std::optional<std::int64_t> exitTicks);
      void UpdateBeforeInput(const InputState &input, std::size_t eventIndex,
                             RhythmTime time);
    void UpdateSession(const InputState &input, const RhythmTime time,
                       bool beforeExit = false);
    bool IsPatternComplete() const noexcept;
    void ConsumeResult(NoteProcessResult result, bool pressed = false);
    GameplayLaunchRequest launchRequest_;
    std::unique_ptr<PlaySession> session_;
    PlaySessionLoader sessionLoader_;
    bool sessionInitialized_{};
    RhythmTimer timer_;
    GameplayAudioRouter audioRouter_;
    std::vector<GameplayFeedback> feedback_;
    RhythmTime time_{};
    RhythmTime initialTime_{-2'000'000};
    PlayState state_{PlayState::Loading};
    std::unique_ptr<IPlayResult> result_;
    std::optional<AudioClockSnapshot> playbackStart_;
    std::int64_t gameplayStartTicks_{};
    std::string initializationError_;
    double completedElapsedSeconds_{};
    double debugSpeedMillisecondsPerSecond_{1000.0};
    bool musicRegistered_{};
    bool audioInitialized_{}, resourcesReady_{}, playbackRequested_{};
    bool resourceLoadFailed_{};
    double nextAudioPreparationPoll_{};
    bool debugMode_{};
    bool pauseMenuOpen_{};
};
