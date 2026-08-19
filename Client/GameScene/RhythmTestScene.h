#pragma once

#include "MRG_Core.h"

#include "Audio/GameplayAudioRouter.h"
#include "GameFlow/GameplayLaunchRequest.h"
#include "Mode/PlayGameMode.h"
#include "Time/RhythmTimer.h"

#include <cstdint>
#include <array>
#include <memory>
#include <unordered_map>
#include <vector>

// A transient Taiko gameplay driver. One logical Lane owns every Don, Kat,
// large note and roll in chronological order, while presentation metadata maps
// those mode-neutral notes to the legacy layered skin.
class RhythmTestScene final : public mrg::scene::GameScene
{
public:
    explicit RhythmTestScene(
        std::shared_ptr<finger_drum::GameplayLaunchRequest> launchRequest,
        bool debugMode = false);

    void Initialize(const mrg::EngineServices& services) override;
    void Update(
        const mrg::UpdateContext& context,
        mrg::scene::SceneManager& scenes) override;
    void Render(const mrg::graphics::RenderContext& context) override;
    void OnResize(std::uint32_t width, std::uint32_t height) override;
    void Shutdown() noexcept override;

private:
    struct TimedVisual
    {
        finger_drum::rhythm::RhythmTime timing{};
        mrg::visual2d::Visual2DNode* node{};
    };

    struct NoteVisualLayers
    {
        mrg::visual2d::Visual2DNode* root{};
        mrg::visual2d::Visual2DNode* ambient{};
        mrg::visual2d::Visual2DNode* overlay{};
        mrg::visual2d::Visual2DNode* body{};
        mrg::visual2d::Visual2DNode* tail{};
        std::vector<TimedVisual> ticks;
        float diameter{};
    };

    [[nodiscard]] std::unique_ptr<finger_drum::mode::PlaySession>
        CreateSession();
    [[nodiscard]] std::unique_ptr<finger_drum::mode::PlaySession>
        CreateDemoSession();
    [[nodiscard]] std::unique_ptr<finger_drum::mode::PlaySession>
        CreateLongNoteDebugSession();
    void PrepareDebugLaunchRequest();
    void CreatePresentation(const mrg::EngineServices& services);
    void CreateLaneVisuals(
        const mrg::EngineServices& services,
        mrg::visual2d::Visual2DNode& sceneRoot);
    void CreateLaneSurface(
        const mrg::EngineServices& services);
    void CreateMeasureLineVisuals(
        const mrg::EngineServices& services);
    void CreateKeyIndicators(
        mrg::visual2d::Visual2DNode& sceneRoot);
    void CreateRemainingCountVisuals(
        const mrg::EngineServices& services,
        mrg::visual2d::Visual2DNode& sceneRoot);
    void CreateNoteVisuals(const mrg::EngineServices& services);
    void UpdatePresentationLayout();
    void InitializeAudio(const mrg::EngineServices& services);
    void RegisterTaikoSounds(std::string& errorMessage);
    void ScheduleMusic();
    void StartTimeline(const mrg::audio::AudioClockSnapshot& clock);
    void ResetTimeline(const mrg::audio::AudioClockSnapshot& clock);
    void ProcessControlKeys(
        const mrg::platform::InputState& input,
        const mrg::audio::AudioClockSnapshot& clock);
    void ProcessDebugTimeline(
        const mrg::platform::InputState& input,
        const mrg::audio::AudioClockSnapshot& clock,
        double deltaSeconds);
    void ProcessRhythmInput(const mrg::platform::InputState& input);
    void UpdateInputPresentation(
        const mrg::platform::InputState& input,
        double deltaSeconds);
    void UpdateSession(
        const mrg::platform::InputState& input,
        finger_drum::rhythm::RhythmTime time);
    void ConsumeResult(finger_drum::rhythm::NoteProcessResult result);
    void UpdatePresentation(finger_drum::rhythm::RhythmTime time);
    void UpdateRemainingCount();
    void UpdateDebugText(finger_drum::rhythm::RhythmTime time);
    void FlashKeyBeam(mrg::visual2d::Color color);
    [[nodiscard]] bool IsWrongActionForCurrentNote(
        finger_drum::rhythm::PhysicalKey physicalKey) const noexcept;
    [[nodiscard]] const finger_drum::rhythm::INote* FindNote(
        finger_drum::rhythm::NoteId noteId) const noexcept;
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
    std::vector<TimedVisual> measureLineVisuals_;
    std::array<mrg::visual2d::Visual2DNode*, 4> keyIndicators_{};
    std::array<mrg::visual2d::ImageHandle, 10> numberImages_{};
    std::array<mrg::visual2d::Visual2DNode*, 4> countDigits_{};
    mrg::visual2d::Visual2DNode* background_{};
    mrg::visual2d::Visual2DNode* headerSurface_{};
    mrg::visual2d::Visual2DNode* scrollGearBorder_{};
    mrg::visual2d::Visual2DNode* scrollGearSurface_{};
    mrg::visual2d::Visual2DNode* scrollGearTopAccent_{};
    mrg::visual2d::Visual2DNode* inputPresentationRoot_{};
    mrg::visual2d::Visual2DNode* laneRoot_{};
    mrg::visual2d::Visual2DNode* laneLight_{};
    mrg::visual2d::Visual2DNode* countBadge_{};
    mrg::visual2d::Visual2DNode* timelineLabel_{};
    mrg::visual2d::Visual2DNode* resultLabel_{};
    mrg::visual2d::Visual2DNode* audioStatusLabel_{};
    mrg::visual2d::Visual2DNode* debugLabel_{};
    mrg::visual2d::Visual2DNode* instructionsLabel_{};
    std::uint64_t acceptedHitCount_{};
    double accumulatedScore_{};
    double completedElapsedSeconds_{};
    double debugSpeedMillisecondsPerSecond_{1000.0};
    mrg::visual2d::Color keyBeamColor_{1.0F, 1.0F, 1.0F, 1.0F};
    float keyBeamAlpha_{};
    bool musicRegistered_{};
    bool debugMode_{};
};
