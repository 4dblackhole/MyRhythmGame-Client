#pragma once
#include "Audio/GameplayAudioRouter.h"
#include "GameFlow/GameplayLaunchRequest.h"
#include "GameFlow/IPlaySessionController.h"
#include "MRG_Core.h"
#include "Time/RhythmTimer.h"
#include <memory>
#include <span>
#include <vector>

class GameplaySessionController final : public IPlaySessionController
{
  public:
    GameplaySessionController(finger_drum::GameplayLaunchRequest request, mrg::audio::AudioPlaybackManager &playback,
                              bool debug, std::unique_ptr<finger_drum::mode::PlaySession> prepared = {})
        : launchRequest_(std::move(request)), session_(std::move(prepared)), audioRouter_(playback), debugMode_(debug)
    {
    }
    ~GameplaySessionController() override
    {
        Shutdown();
    }
    void InitializeSession() override;
    void Start(const mrg::EngineServices &services, finger_drum::rhythm::RhythmTime initialTime) override;
    void Update(const mrg::UpdateContext &context) override;
    PlayState CurrentState() const noexcept override
    {
        return state_;
    }
    void OpenPauseMenu(const mrg::UpdateContext &context) override;
    bool ResumeFromPause(const mrg::UpdateContext &context, finger_drum::rhythm::RhythmDuration leadIn) override;
    bool RewindFromPause(const mrg::UpdateContext &context, std::size_t measures) override;
    void RetryFromPause(const mrg::UpdateContext &context) override;
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
    const finger_drum::mode::PlaySession &Session() const override
    {
        return *session_;
    }
    finger_drum::rhythm::RhythmTime Time() const noexcept override
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
    std::unique_ptr<finger_drum::mode::IPlayResult> TakeResult() override;

  private:
#if defined(_DEBUG)
    static constexpr bool ReferenceTimeDebug = true;
#else
    static constexpr bool ReferenceTimeDebug = false;
#endif
    std::vector<finger_drum::rhythm::PhysicalKey> heldKeys_;
    std::unique_ptr<finger_drum::mode::PlaySession> CreateSession();
    std::unique_ptr<finger_drum::mode::PlaySession> CreateDemoSession();
    std::unique_ptr<finger_drum::mode::PlaySession> CreateLongNoteDebugSession();
    void PrepareDebugLaunchRequest();
    void InitializeAudio(mrg::audio::AudioSystem &audio);
    void UpdateLoading(const mrg::UpdateContext &context);
    void UpdateGameplay(const mrg::UpdateContext &context);
    void UpdatePause(const mrg::UpdateContext &context);
    bool PrepareResources(const mrg::UpdateContext &context);
    void PrepareGameplay(const mrg::UpdateContext &context);
    void ScheduleGameplay(const mrg::UpdateContext &context);
    void FailPlaybackPreparation();
    void EnterEnding(finger_drum::mode::PlayEndReason reason);
    void PauseAt(std::int64_t ticks, bool menuShown);
    bool ContinueAt(const mrg::UpdateContext &context, finger_drum::rhythm::RhythmTime target,
                    finger_drum::mode::PlayControlType action);
    void ResetTimeline();
    void ProcessControlKeys(const mrg::UpdateContext &context, const mrg::audio::AudioClockSnapshot &clock);
    void ProcessDebugTimeline(const mrg::platform::InputState &input, const mrg::audio::AudioClockSnapshot &clock,
                              const double deltaSeconds);
    void ProcessRhythmInput(const mrg::platform::InputState &input, std::optional<std::int64_t> exitTicks);
      void UpdateBeforeInput(const mrg::platform::InputState &input, std::size_t eventIndex,
                             finger_drum::rhythm::RhythmTime time);
    void UpdateSession(const mrg::platform::InputState &input, const finger_drum::rhythm::RhythmTime time,
                       bool beforeExit = false);
    bool IsPatternComplete() const noexcept;
    void ConsumeResult(finger_drum::rhythm::NoteProcessResult result, bool pressed = false);
    finger_drum::GameplayLaunchRequest launchRequest_;
    std::unique_ptr<finger_drum::mode::PlaySession> session_;
    finger_drum::rhythm::RhythmTimer timer_;
    finger_drum::audio::GameplayAudioRouter audioRouter_;
    std::vector<GameplayFeedback> feedback_;
    finger_drum::rhythm::RhythmTime time_{};
    finger_drum::rhythm::RhythmTime initialTime_{-2'000'000};
    PlayState state_{PlayState::Loading};
    std::unique_ptr<finger_drum::mode::IPlayResult> result_;
    std::optional<mrg::audio::AudioClockSnapshot> playbackStart_;
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
