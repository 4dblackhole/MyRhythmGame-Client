#pragma once
#include "MRG_Core.h"
#include "Mode/PlayGameMode.h"
#include "Presentation/LaneKeyBeam.h"
#include "GameplayFeedback.h"
#include <array>
#include <unordered_map>

class GameplayPresenter final
{
  public:
    explicit GameplayPresenter(mrg::visual2d::ScreenVisual2DManager &visuals)
        : screenVisuals_(visuals)
    {
    }
    void Initialize(const mrg::EngineServices &services,
                    const finger_drum::mode::PlaySession &session);
    void SetVisible(bool visible)
    {
        static_cast<void>(canvasHandle_.SetVisible(visible));
    }
    void AdvanceEffects(double delta)
    {
        keyBeam_.Update(delta);
    }
    void OnResize(const std::uint32_t width, const std::uint32_t height);
    void Shutdown() noexcept;
    void ApplyFeedback(const GameplayFeedback &feedback);
    void PresentAudioError(const std::string_view message);
    void UpdatePresentation(const finger_drum::rhythm::RhythmTime time);
    void UpdateInputPresentation(const mrg::platform::InputState &input, const double deltaSeconds);

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
        mrg::visual2d::Visual2DNode *node{};
    };

    struct NoteVisualLayers
    {
        mrg::visual2d::Visual2DNode *root{};
        mrg::visual2d::Visual2DNode *ambient{};
        mrg::visual2d::Visual2DNode *overlay{};
        mrg::visual2d::Visual2DNode *body{};
        mrg::visual2d::Visual2DNode *bodyOverlay{};
        mrg::visual2d::Visual2DNode *tail{};
        mrg::visual2d::Visual2DNode *tailOverlay{};
        mrg::visual2d::Visual2DNode *processing{};
        mrg::visual2d::Visual2DNode *success{};
        mrg::visual2d::Visual2DNode *counter{};
        mrg::visual2d::Visual2DNode *counterText{};
        std::vector<TimedVisual> ticks;
        FocusNoteType focusType{FocusNoteType::None};
        mrg::visual2d::Size processingSize{};
        mrg::visual2d::Size successSize{};
        float diameter{};
        float bodyWidth{};
        float tailWidth{};
        float tailHeight{};
    };

    void CreatePresentation();
    void CreateLaneVisuals(mrg::visual2d::Visual2DNode &sceneRoot);
    void CreateLaneSurface();
    void UpdateLaneSurfaceLayout(const float laneLength);
    void CreateMeasureLineVisuals();
    void CreateKeyIndicators(mrg::visual2d::Visual2DNode &inputPanel);
    void CreateNoteVisuals();
    void UpdatePresentationLayout();
    void HideTransientNoteVisuals();
    void PresentFocusCounter(NoteVisualLayers &layers,
                             const finger_drum::rhythm::NoteProgress &progress,
                             const float visualHeight);
    void PresentFocusNoteProcessing(NoteVisualLayers &layers,
                                    const finger_drum::rhythm::NoteProgress &progress,
                                    const finger_drum::rhythm::RhythmTime time);
    void PresentCompletionEffect(const finger_drum::rhythm::RhythmTime time);
    void UpdateAccuracyPresentation();
    std::uint32_t width_{1280};
    std::uint32_t height_{720};
    mrg::visual2d::ScreenVisual2DManager &screenVisuals_;
    mrg::visual2d::ScreenCanvasHandle canvasHandle_;
    mrg::visual2d::Visual2DCanvas *canvas_{};
    const finger_drum::mode::PlaySession *session_{};
    finger_drum::presentation::LaneKeyBeam keyBeam_;
    std::unordered_map<finger_drum::rhythm::NoteId, NoteVisualLayers> noteVisuals_;
    std::vector<finger_drum::rhythm::NoteId> presentedNoteIds_;
    std::unordered_map<finger_drum::rhythm::NoteId, finger_drum::rhythm::RhythmTime>
        completionEffects_;
    std::vector<TimedVisual> measureLineVisuals_;
    std::array<mrg::visual2d::Visual2DNode *, 4> keyIndicators_{};
    std::array<mrg::visual2d::Visual2DNode *, 4> keyGlows_{};
    std::array<mrg::visual2d::Visual2DNode *, 4> keyPressFlashes_{};
    std::array<double, 4> keyPressFlashRemainingSeconds_{};
    mrg::visual2d::Visual2DNode *background_{};
    mrg::visual2d::Visual2DNode *scrollGearBorder_{};
    mrg::visual2d::Visual2DNode *scrollGearSurface_{};
    mrg::visual2d::Visual2DNode *inputPresentationRoot_{};
    mrg::visual2d::Visual2DNode *inputPanel_{};
    mrg::visual2d::Visual2DNode *laneRoot_{};
    std::vector<mrg::visual2d::Visual2DNode *> laneTiles_;
    mrg::visual2d::ImageHandle laneImage_{};
    mrg::visual2d::ImageHandle strongKeyLightImage_{};
    mrg::visual2d::ImageHandle weakKeyLightImage_{};
    mrg::visual2d::ImageHandle keyPressFlashImage_{};
    mrg::visual2d::Visual2DNode *gameProgressBar_{};
    mrg::visual2d::Visual2DNode *accuracyIndicator_{};
#if defined(_DEBUG)
    mrg::visual2d::Visual2DNode *noteDebugLabel_{};
#endif
    mrg::visual2d::Visual2DNode *judgementIndicator_{};
    mrg::visual2d::Visual2DNode *audioErrorLabel_{};
    float laneWidth_{152.0F};
    float laneTileLength_{42.0F};
    float judgementLocalY_{76.0F};
    mrg::visual2d::Size inputPanelSize_{152.0F, 152.0F};
};
