#pragma once

#include "App/OptionSettings.h"
#include "App/SkinSetSelection.h"
#include "PhantomCore.h"
#include "Mode/ModeInputLayout.h"
#include "Texts/TextCatalog.h"

#include <optional>
#include <vector>

class OptionsPanel final
{
  public:
    OptionsPanel(TextCatalog &texts, OptionSettings &options) noexcept
        : texts_(texts), options_(options) {}

    void Initialize(Visual2DCanvas &canvas, AudioSystem &audio,
                    IWindowControl *window = nullptr);
    void Shutdown() noexcept;
    void Toggle();
    void Update(double deltaSeconds);
    // True consumes a panel keyboard command, including the closing frame.
    bool ProcessInput(const InputState &input, std::optional<Point> canvasPointer,
                      NodeId hoveredNode, Visual2DInputRouter &inputRouter);
    bool HandleKey(std::uint16_t virtualKey, Visual2DInputRouter &inputRouter);
    [[nodiscard]] bool ProcessAction(const Action &action);
    void RefreshTexts();
    [[nodiscard]] bool OwnsKeyboard() const noexcept
    {
        return captureRow_.has_value() || editingOffset_;
    }
    [[nodiscard]] bool IsVisible() const noexcept;

  private:
    void CreateDisplayControls();
    void RefreshDisplayControls();
    void SelectResolution(std::size_t index);
    void CreateKeyControls();
    void RefreshKeyControls();
    void CaptureKey(std::uint16_t key);
    void CreateOffsetControls();
    void CreateSongsControls();
    void RefreshSongsControls();
    void ChooseSongsFolder();
    void RefreshOffsetControls();
    void EditOffset(std::uint16_t key);
    void ApplyOffset();
    void CancelKeyboardEdit();
    bool ProcessFieldAction(const Action &action);
    void ApplyAnimationPosition();
    void ApplyScrollOffset(Visual2DInputRouter &inputRouter);
    void CreateLanguageControls();
    void CreateSkinControls();
    void CreateAudioControls();
    void RefreshAudioControls();
    void RefreshStatus();
    void RefreshSkinSets();
    bool SaveOptions(const OptionValues &candidate);
    void SelectOutput(AudioOutputBackend output);
    void SelectDriver(int index);
    void RestoreAudio(AudioOutputBackend output, int driver, std::string &error);
    [[nodiscard]] std::string DriverName(int index) const;

    static constexpr float PanelWidth = 320.0F;
    static constexpr float PanelHeight = 720.0F;
    static constexpr float AnimationSeconds = 0.2F;
    static constexpr float ContentViewportHeight = 608.0F;
    float contentHeight_{};
    struct KeyRow
    {
        std::string mode, binding, displayName;
        std::size_t slot{};
        Visual2DNode *label{}, *button{};
    };
    std::vector<ModeInputLayout> inputLayouts_;
    std::vector<KeyRow> keyRows_;
    std::vector<Visual2DNode *> modeLabels_;
    std::vector<WindowResolution> resolutions_;
    IWindowControl *window_{};
    Visual2DNode *resolutionLabel_{}, *resolutionCombo_{};
    Visual2DNode *offsetLabel_{}, *offsetField_{}, *offsetHint_{};
    Visual2DNode *songsLabel_{}, *songsField_{}, *songsDefault_{};
    std::optional<std::size_t> captureRow_;
    std::wstring offsetDraft_;
    bool editingOffset_{}, replaceOffsetDraft_{};

    TextCatalog &texts_;
    OptionSettings &options_;
    AudioSystem *audio_{};
    Visual2DCanvas *canvas_{};
    enum class Status
    {
        None,
        SaveFailed,
        AudioFailed,
        WindowFailed,
        InvalidInput
    };
    Status status_{};
    std::string statusDetail_;
    Visual2DNode *panel_{};
    Visual2DNode *title_{};
    Visual2DNode *languageLabel_{};
    Visual2DNode *skinLabel_{};
    Visual2DNode *statusLabel_{};
    Visual2DNode *middlewareLabel_{};
    Visual2DNode *outputLabel_{};
    Visual2DNode *driverLabel_{};
    Visual2DNode *content_{};
    Visual2DNode *languageCombo_{};
    Visual2DNode *skinCombo_{};
    Visual2DNode *middlewareCombo_{};
    Visual2DNode *outputCombo_{};
    Visual2DNode *driverCombo_{};
    NodeId panelId_{};
    NodeId languageComboId_{};
    NodeId skinComboId_{};
    NodeId outputComboId_{};
    NodeId driverComboId_{};
    std::vector<int> driverIndices_;
    std::vector<std::wstring> skinNames_;
    std::optional<Point> previousDragPoint_;
    float animationProgress_{};
    float scrollOffset_{};
    bool targetOpen_{};
    bool dragging_{};
    std::uint64_t optionRevision_{};
};
