#pragma once
#include "GameFlow/GameplayFeedback.h"
#include "GameplayJudgementView.h"
#include "GameplayProgressView.h"
#include "PhantomCore.h"
#include "Mode/PlayGameMode.h"
#include "Presentation/LaneKeyBeam.h"
#include "Texts/TextCatalog.h"
#include "Utility/IntervalIndex.h"
#include <array>
#include <unordered_map>

class GameplayPresenter final
{
  public:
    GameplayPresenter(ScreenVisual2DManager &visuals,
                      TextCatalog &texts)
        : screenVisuals_(visuals), texts_(texts)
    {
    }
    void Initialize(const EngineServices &services,
                    const PlaySession &session);
    bool PrepareAssets(const EngineServices &services);
    void ConfigureBackground(std::filesystem::path path, double x, double y)
    {
        backgroundPath_ = std::move(path);
        backgroundOffset_ = {static_cast<float>(x), static_cast<float>(y)};
    }
    void SetVisible(bool visible)
    {
        static_cast<void>(canvasHandle_.SetVisible(visible));
    }
    void AdvanceEffects(double delta)
    {
        keyBeam_.Update(delta);
        judgementView_.Update(delta);
    }
    void OnResize(const std::uint32_t width, const std::uint32_t height);
    void Shutdown() noexcept;
    void ApplyFeedback(const GameplayFeedback &feedback);
    void PresentErrorMessage(const std::string_view message);
    void UpdatePresentation(const RhythmTime time);
    [[nodiscard]] RhythmTime InitialTimelineTime() const;
    void UpdateInputPresentation(const InputState &input, const double deltaSeconds);

  private:
    ImageHandle backgroundImage_{};
    Size backgroundSize_{};
    Point backgroundOffset_{};
    std::filesystem::path backgroundPath_;
    std::string backgroundWarning_;
    enum class FocusNoteType : std::uint8_t
    {
        None,
        Balloon,
        DengDeng,
    };

    struct TimedVisual
    {
        RhythmTime timing{};
        Visual2DNode *node{};
    };

    struct NoteVisualLayers
    {
        Visual2DNode *root{};
        Visual2DNode *ambient{};
        Visual2DNode *overlay{};
        Visual2DNode *body{};
        Visual2DNode *bodyOverlay{};
        Visual2DNode *tail{};
        Visual2DNode *tailOverlay{};
        Visual2DNode *processing{};
        Visual2DNode *success{};
        Visual2DNode *counter{};
        Visual2DNode *counterText{};
        std::vector<TimedVisual> ticks;
        ImageHandle tickImage{};
        Size tickSize{};
        RhythmTime timing{};
        RhythmTime endTime{};
        FocusNoteType focusType{FocusNoteType::None};
        Size processingSize{};
        Size successSize{};
        float diameter{};
        float bodyWidth{};
        float tailWidth{};
        float tailHeight{};
        std::uint64_t presentedGeneration{};
    };

    void CreatePresentation();
    void CreateLaneVisuals(Visual2DNode &gearRoot);
    void CreateLaneSurface();
    void UpdateLaneSurfaceLayout(const float laneLength);
    void CreateKeyIndicators(Visual2DNode &inputPanel);
    void CreateNoteVisuals();
    void CreateNoteVisual(const INote &);
    ImageHandle NoteImage(std::wstring_view);
    void UpdateMeasureLines(RhythmTime, bool);
    void UpdateTickVisuals(NoteVisualLayers &, const NotePresentationInfo &,
                           RhythmTime, RhythmTime, std::size_t);
    void UpdatePresentationLayout();
    void UpdateNoteTravelLayout();
    [[nodiscard]] float TravelPixels(RhythmDuration duration,
                                     float multiplier = 1.0F) const noexcept;
    float TravelPixelsAt(RhythmTime target, RhythmTime current,
                         float multiplier = 1) const;
    [[nodiscard]] Point LaneObjectPosition(RhythmTime target,
                                                          RhythmTime current,
                                                          float multiplier) const;
    void HideTransientNoteVisuals();
    void RemoveUnusedNoteVisuals();
    void RebuildVisibilityIndices();
    void PresentHitCounter(NoteVisualLayers &layers,
                             const NoteProgress &progress,
                             const float visualHeight);
    void PresentFocusNoteProcessing(NoteVisualLayers &layers,
                                    const NoteProgress &progress,
                                    const RhythmTime time);
    void PresentCompletionEffect(const RhythmTime time);
    void UpdateAccuracyPresentation();
    void PresentJudgementFeedback(const GameplayFeedback &feedback);
    std::uint32_t width_{1280};
    std::uint32_t height_{720};
    ScreenVisual2DManager &screenVisuals_;
    TextCatalog &texts_;
    ScreenCanvasHandle canvasHandle_;
    Visual2DCanvas *canvas_{};
    const PlaySession *session_{};
    LaneKeyBeam keyBeam_;
    GameplayJudgementView judgementView_;
    GameplayProgressView progressView_;
    std::unordered_map<NoteId, NoteVisualLayers> noteVisuals_;
    std::unordered_map<NoteId, const INote *> noteModels_;
    std::map<std::wstring, ImageHandle, std::less<>> noteImages_;
    std::vector<std::filesystem::path> preparingImages_;
    float largestHeadRadius_{};
    std::vector<NoteId> presentedNoteIds_;
    IntervalIndex<long double, NoteId> noteVisibility_;
    IntervalIndex<long double, std::size_t> measureVisibility_;
    std::vector<const INote *> visibleNoteModels_;
    std::uint64_t visibilityRevision_{};
    std::uint64_t presentationGeneration_{};
    long double firstVisibilityDistance_{};
    std::unordered_map<NoteId, RhythmTime>
        completionEffects_;
    std::vector<TimedVisual> measureLineVisuals_;
    std::array<Visual2DNode *, 4> keyIndicators_{};
    std::array<Visual2DNode *, 4> keyGlows_{};
    std::array<Visual2DNode *, 4> keyPressFlashes_{};
    std::array<double, 4> keyPressFlashRemainingSeconds_{};
    Visual2DNode *background_{};
    Visual2DNode *scrollGearRoot_{};
    Visual2DNode *scrollGearBorder_{};
    Visual2DNode *scrollGearSurface_{};
    Visual2DNode *inputPresentationRoot_{};
    Visual2DNode *inputPanel_{};
    Visual2DNode *laneRoot_{};
    Visual2DNode *noteRoot_{};
    std::vector<Visual2DNode *> laneTiles_;
    ImageHandle laneImage_{};
    ImageHandle strongKeyLightImage_{};
    ImageHandle weakKeyLightImage_{};
    ImageHandle keyPressFlashImage_{};
    Visual2DNode *accuracyIndicator_{};
#if defined(_DEBUG)
    Visual2DNode *noteDebugLabel_{};
#endif
    Visual2DNode *errorLabel_{};
    float laneWidth_{152.0F};
    float laneTileLength_{42.0F};
    float judgementLocalY_{76.0F};
    float noteTravelDistance_{1.0F};
    float pixelsPerWholeNote_{1.0F};
    Size inputPanelSize_{152.0F, 152.0F};
};
