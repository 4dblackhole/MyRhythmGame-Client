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
        mrg::audio::AudioPlaybackManager& playback,
        bool debugMode = false);

    void Initialize(const mrg::EngineServices& services) override;
    void Update(
        const mrg::UpdateContext& context,
        mrg::scene::SceneManager& scenes) override;
    void Render(const mrg::graphics::RenderContext& context) override;
    void OnResize(std::uint32_t width, std::uint32_t height) override;
    void Shutdown() noexcept override;

private:
    enum class FocusNoteType : std::uint8_t
    {
        None,
        Balloon,
        DengDeng,
    };

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
        mrg::visual2d::Visual2DNode* bodyOverlay{};
        mrg::visual2d::Visual2DNode* tail{};
        mrg::visual2d::Visual2DNode* tailOverlay{};
        mrg::visual2d::Visual2DNode* processing{};
        mrg::visual2d::Visual2DNode* success{};
        mrg::visual2d::Visual2DNode* counter{};
        mrg::visual2d::Visual2DNode* counterText{};
        std::vector<TimedVisual> ticks;
        FocusNoteType focusType{FocusNoteType::None};
        mrg::visual2d::Size processingSize{};
        mrg::visual2d::Size successSize{};
        float diameter{};
        float bodyWidth{};
        float tailWidth{};
        float tailHeight{};
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
    void UpdateLaneSurfaceLayout(float laneLength);
    void CreateMeasureLineVisuals(
        const mrg::EngineServices& services);
    void CreateKeyIndicators(
        const mrg::EngineServices& services,
        mrg::visual2d::Visual2DNode& inputPanel);
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
    void UpdateInputPresentation(const mrg::platform::InputState& input);
    void UpdateSession(
        const mrg::platform::InputState& input,
        finger_drum::rhythm::RhythmTime time);
    void ConsumeResult(finger_drum::rhythm::NoteProcessResult result);
    void UpdatePresentation(finger_drum::rhythm::RhythmTime time);
    void HideTransientNoteVisuals();
    void PresentFocusNoteProcessing(
        NoteVisualLayers& layers,
        const finger_drum::rhythm::NoteProgress& progress,
        finger_drum::rhythm::RhythmTime time);
    void PresentFocusCounter(
        NoteVisualLayers& layers,
        const finger_drum::rhythm::NoteProgress& progress,
        float visualHeight);
    void PresentCompletionEffect(finger_drum::rhythm::RhythmTime time);
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
    std::unordered_map<finger_drum::rhythm::NoteId,
        finger_drum::rhythm::RhythmTime> completionEffects_;
    std::vector<TimedVisual> measureLineVisuals_;
    std::array<mrg::visual2d::Visual2DNode*, 4> keyIndicators_{};
    std::array<mrg::visual2d::Visual2DNode*, 4> keyGlows_{};
    mrg::visual2d::Visual2DNode* background_{};
    mrg::visual2d::Visual2DNode* scrollGearBorder_{};
    mrg::visual2d::Visual2DNode* scrollGearSurface_{};
    mrg::visual2d::Visual2DNode* inputPresentationRoot_{};
    mrg::visual2d::Visual2DNode* inputPanel_{};
    mrg::visual2d::Visual2DNode* laneRoot_{};
    std::vector<mrg::visual2d::Visual2DNode*> laneTiles_;
    mrg::visual2d::ImageHandle laneImage_{};
    mrg::visual2d::ImageHandle strongKeyLightImage_{};
    mrg::visual2d::ImageHandle weakKeyLightImage_{};
    mrg::visual2d::Visual2DNode* gameProgressBar_{};
    mrg::visual2d::Visual2DNode* accuracyIndicator_{};
    mrg::visual2d::Visual2DNode* judgementIndicator_{};
    float laneWidth_{152.0F};
    float laneTileLength_{42.0F};
    float judgementLocalY_{76.0F};
    mrg::visual2d::Size inputPanelSize_{152.0F, 152.0F};
    double completedElapsedSeconds_{};
    double debugSpeedMillisecondsPerSecond_{1000.0};
    bool musicRegistered_{};
    bool debugMode_{};
};
