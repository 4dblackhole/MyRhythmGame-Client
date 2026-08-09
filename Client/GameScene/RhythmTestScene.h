#pragma once

#include "MRG_Core.h"

#include "Audio/GameplayAudioRouter.h"
#include "GameFlow/GameplayLaunchRequest.h"
#include "Mode/PlayGameMode.h"
#include "Time/RhythmTimer.h"

#include <cstdint>
#include <memory>
#include <unordered_map>

// A transient Taiko gameplay driver. One logical Lane owns every Don, Kat,
// large note and roll in chronological order, while presentation metadata maps
// those mode-neutral notes to the legacy layered skin.
class RhythmTestScene final : public mrg::scene::GameScene
{
public:
    explicit RhythmTestScene(
        std::shared_ptr<finger_drum::GameplayLaunchRequest> launchRequest);

    void Initialize(const mrg::EngineServices& services) override;
    void Update(
        const mrg::UpdateContext& context,
        mrg::scene::SceneManager& scenes) override;
    void Render(const mrg::graphics::RenderContext& context) override;
    void OnResize(std::uint32_t width, std::uint32_t height) override;
    void Shutdown() noexcept override;

private:
    struct NoteVisualLayers
    {
        mrg::visual2d::Visual2DNode* root{};
        mrg::visual2d::Visual2DNode* ambient{};
        mrg::visual2d::Visual2DNode* overlay{};
        mrg::visual2d::Visual2DNode* body{};
        mrg::visual2d::Visual2DNode* tail{};
        float diameter{};
    };

    [[nodiscard]] std::unique_ptr<finger_drum::mode::PlaySession>
        CreateSession();
    [[nodiscard]] std::unique_ptr<finger_drum::mode::PlaySession>
        CreateDemoSession();
    void CreatePresentation(const mrg::EngineServices& services);
    void CreateLaneVisuals(
        const mrg::EngineServices& services,
        mrg::visual2d::Visual2DNode& sceneRoot);
    void CreateLaneBackgroundTiles(
        const mrg::EngineServices& services);
    void CreateNoteVisuals(const mrg::EngineServices& services);
    void InitializeAudio(const mrg::EngineServices& services);
    void RegisterTaikoSounds(std::string& errorMessage);
    void ScheduleMusic();
    void StartTimeline(const mrg::audio::AudioClockSnapshot& clock);
    void ResetTimeline(const mrg::audio::AudioClockSnapshot& clock);
    void ProcessControlKeys(
        const mrg::platform::InputState& input,
        const mrg::audio::AudioClockSnapshot& clock);
    void ProcessRhythmInput(const mrg::platform::InputState& input);
    void UpdateSession(
        const mrg::platform::InputState& input,
        finger_drum::rhythm::RhythmTime time);
    void ConsumeResult(finger_drum::rhythm::NoteProcessResult result);
    void UpdatePresentation(finger_drum::rhythm::RhythmTime time);
    [[nodiscard]] bool IsPatternComplete() const noexcept;
    [[nodiscard]] bool ReturnToLobby(
        mrg::scene::SceneManager& scenes) const;

    std::shared_ptr<finger_drum::GameplayLaunchRequest> launchRequest_;
    std::uint32_t width_{1280};
    std::uint32_t height_{720};
    std::unique_ptr<mrg::visual2d::Visual2DCanvas> canvas_;
    std::unique_ptr<finger_drum::mode::PlaySession> session_;
    finger_drum::rhythm::RhythmTimer timer_;
    finger_drum::audio::GameplayAudioRouter audioRouter_;
    std::unordered_map<finger_drum::rhythm::NoteId, NoteVisualLayers>
        noteVisuals_;
    mrg::visual2d::Visual2DNode* laneRoot_{};
    mrg::visual2d::Visual2DNode* timelineLabel_{};
    mrg::visual2d::Visual2DNode* resultLabel_{};
    mrg::visual2d::Visual2DNode* audioStatusLabel_{};
    std::uint64_t acceptedHitCount_{};
    double accumulatedScore_{};
    double completedElapsedSeconds_{};
    bool musicRegistered_{};
};
