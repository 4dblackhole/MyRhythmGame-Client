#pragma once
#include "MRG_Core.h"
#include "Audio/GameplayAudioRouter.h"
#include "GameFlow/GameplayLaunchRequest.h"
#include "Mode/PlayGameMode.h"
#include "Time/RhythmTimer.h"
#include "GameplayFeedback.h"
#include <memory>
#include <span>
#include <vector>

class GameplaySessionController final
{
  public:
    GameplaySessionController(finger_drum::GameplayLaunchRequest request,
                              mrg::audio::AudioPlaybackManager &playback, bool debug)
        : launchRequest_(std::move(request)), audioRouter_(playback), debugMode_(debug)
    {
    }
    ~GameplaySessionController()
    {
        Shutdown();
    }
    void InitializeSession();
    void Start(const mrg::EngineServices &services,
               finger_drum::rhythm::RhythmTime initialTime);
    void Update(const mrg::UpdateContext &context);
    void Shutdown() noexcept;
    const finger_drum::mode::PlaySession &Session() const
    {
        return *session_;
    }
    finger_drum::rhythm::RhythmTime Time() const noexcept
    {
        return time_;
    }
    std::span<const GameplayFeedback> Feedback() const noexcept
    {
        return feedback_;
    }
    std::string_view AudioError() const noexcept;
    bool ShouldReturnToLobby() const noexcept
    {
        return !debugMode_ && completedElapsedSeconds_ >= 3.0;
    }

  private:
    std::unique_ptr<finger_drum::mode::PlaySession> CreateSession();
    std::unique_ptr<finger_drum::mode::PlaySession> CreateDemoSession();
    std::unique_ptr<finger_drum::mode::PlaySession> CreateLongNoteDebugSession();
    void PrepareDebugLaunchRequest();
    void InitializeAudio(const mrg::EngineServices &services);
    void RegisterTaikoSounds(std::string &errorMessage);
    void ScheduleMusic();
    void StartTimeline(const mrg::audio::AudioClockSnapshot &clock);
    void ResetTimeline(const mrg::audio::AudioClockSnapshot &clock);
    void ProcessControlKeys(const mrg::platform::InputState &input,
                            const mrg::audio::AudioClockSnapshot &clock);
    void ProcessDebugTimeline(const mrg::platform::InputState &input,
                              const mrg::audio::AudioClockSnapshot &clock,
                              const double deltaSeconds);
    void ProcessRhythmInput(const mrg::platform::InputState &input);
    void UpdateSession(const mrg::platform::InputState &input,
                       const finger_drum::rhythm::RhythmTime time);
    bool IsPatternComplete() const noexcept;
    void ConsumeResult(finger_drum::rhythm::NoteProcessResult result, bool pressed = false);
    finger_drum::GameplayLaunchRequest launchRequest_;
    std::unique_ptr<finger_drum::mode::PlaySession> session_;
    finger_drum::rhythm::RhythmTimer timer_;
    finger_drum::audio::GameplayAudioRouter audioRouter_;
    std::vector<GameplayFeedback> feedback_;
    finger_drum::rhythm::RhythmTime time_{};
    finger_drum::rhythm::RhythmTime initialTime_{-2'000'000};
    std::string initializationError_;
    double completedElapsedSeconds_{};
    double debugSpeedMillisecondsPerSecond_{1000.0};
    bool musicRegistered_{};
    bool debugMode_{};
};
