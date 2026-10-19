#pragma once
#include "EditorAnalysisController.h"
#include "EditorForms.h"
#include "EditorMetronome.h"
#include "EditorPlayback.h"
#include "Modes/IEditorMode.h"
#include <algorithm>
#include <optional>

class EditorWorkspace final : public IEditorContext
{
  public:
    struct AudioSelection
    {
        finger_drum::rhythm::RhythmTime begin{}, end{};
    };
    explicit EditorWorkspace(finger_drum::GameplayLaunchRequest selected, std::unique_ptr<IEditorMode> mode = {})
        : request_(std::move(selected)), mode_(std::move(mode))
    {
    }
    // Injected modes/documents use the same owner and editing boundary.
    EditorWorkspace(std::unique_ptr<IEditorMode> mode, std::unique_ptr<finger_drum::chart::IEditorDocument> document)
        : editor_(std::move(document)), mode_(std::move(mode))
    {
    }
    void Initialize();
    void UpdateAnalysis();
    void InitializePlayback(mrg::audio::AudioSystem &audio) noexcept { playback_.Initialize(audio); }
    void ShutdownPlayback() noexcept { playback_.Shutdown(); }
    void UpdatePlayback();
    void TogglePlayback();
    void SetPlaybackRate(double rate);
    double PlaybackRate() const noexcept { return playback_.Rate(); }
    void SelectAudioRange(finger_drum::rhythm::RhythmTime anchor, finger_drum::rhythm::RhythmTime end)
    {
        audioSelection_ = AudioSelection{std::min(anchor, end), std::max(anchor, end)};
        RequestRebuild();
    }
    const std::optional<AudioSelection> &SelectedAudioRange() const noexcept { return audioSelection_; }
    void ToggleMetronome()
    {
        metronome_.SetEnabled(!metronome_.Enabled());
        RequestRebuild();
    }
    void PrepareAudioPreview();
    const EditorMetronome &Metronome() const noexcept
    {
        return metronome_;
    }
    void SetPatternOffset(double milliseconds);
    void SeekFirstNote();
    void SeekLastNote();
    void SetRealtimeZoom(double factor);
    void SelectTool(int value) override;
    void Save();
    void PlaceNote(finger_drum::chart::MusicalPosition position);
    [[nodiscard]] std::pair<double, double> TimelineRangeMilliseconds() const;
    void Seek(double milliseconds) override;
    double TimeMilliseconds() const noexcept override
    {
        return timeMs_;
    }
    const finger_drum::chart::IEditorDocument &Document() const noexcept override
    {
        return *editor_;
    }
    IEditorMode &Mode() noexcept
    {
        return *mode_;
    }
    EditorAnalysisController &Analysis() noexcept
    {
        return analysis_;
    }
    const EditorScoreState &Score() const noexcept override
    {
        return score_;
    }
    void SetFirstMeasure(std::int64_t) override;
    void SetDivision(std::int64_t);
    void SetRealtime(bool);
    void RequestRebuild() noexcept override
    {
        rebuild_ = true;
    }
    bool NeedsRebuild() const noexcept
    {
        return rebuild_;
    }
    void FinishBuild() noexcept
    {
        rebuild_ = false;
    }
    EditorTab Tab() const noexcept
    {
        return tab_;
    }
    const std::string &Status() const noexcept
    {
        return status_;
    }
    const std::filesystem::path &SavedPath() const noexcept
    {
        return savedPath_;
    }
    void SetStatus(std::string value)
    {
        savedPath_.clear();
        status_ = std::move(value);
        RequestRebuild();
    }
    void SelectTab(EditorTab value);
    void SelectTimingPosition(finger_drum::chart::MusicalPosition) override;
    void Replace(finger_drum::chart::PatternDocument, finger_drum::chart::EffectDocument) override;
    void AddNote(finger_drum::chart::MusicalPosition, int keyType,
                 std::optional<finger_drum::chart::MusicalPosition> end = {},
                 std::vector<std::string> extra = {}) override;
    void UpdateNote(std::size_t, finger_drum::chart::PatternNote,
                    std::optional<finger_drum::chart::MusicalPosition>) override;
    void DeleteNote(std::size_t sourceOrder) override;
    const std::set<std::size_t> &SelectedNoteIds() const noexcept override
    {
        return selectedNotes_;
    }
    void SelectNotes(std::span<const std::size_t>) override;
    void SelectNoteRange(finger_drum::chart::MusicalPosition, finger_drum::chart::MusicalPosition) override;
    const std::optional<std::pair<finger_drum::chart::MusicalPosition, finger_drum::chart::MusicalPosition>> &
    NoteSelectionRange() const noexcept override
    {
        return noteSelectionRange_;
    }
    void SelectListRow(std::size_t row, bool toggle, bool range);
    bool IsListRowSelected(std::size_t) const noexcept;
    void CopySelection();
    void PasteSelection(bool includeBlanks);
    void DeleteSelection();
    void Undo();
    void Redo();
    void SetMeasureLength(std::int64_t, finger_drum::chart::Rational);
    void RemoveTiming(std::size_t);
    void ApplyBpm();
    void ApplyMeasureLength();
    void RemoveEffect(std::size_t);
    void ApplyEffect();
    double audioWindow{8.0};
    EditorTimingForm timingForm;
    EditorEffectForm effectForm;
    std::size_t listOffset{};

  private:
    template <class Event> struct ClipboardEvent
    {
        Event value;
        finger_drum::chart::Rational offset;
        std::optional<finger_drum::chart::Rational> endOffset;
    };
    struct ClipboardNote
    {
        finger_drum::chart::PatternNote head;
        std::optional<finger_drum::chart::PatternNote> tail;
        finger_drum::chart::Rational headOffset, tailOffset;
    };
    struct Clipboard
    {
        EditorTab tab{EditorTab::Pattern};
        finger_drum::chart::Rational length;
        std::vector<ClipboardNote> notes;
        std::vector<ClipboardEvent<finger_drum::chart::TimingDirective>> timing;
        std::vector<ClipboardEvent<finger_drum::chart::EffectCommand>> effects;
        std::vector<ClipboardEvent<finger_drum::chart::HitSoundChange>> sounds;
    };
    void PasteNotes(const Clipboard &, finger_drum::chart::MusicalPosition, bool);
    void PasteTiming(const Clipboard &, finger_drum::chart::MusicalPosition, bool);
    void PasteEffects(const Clipboard &, finger_drum::chart::MusicalPosition, bool);
    void ClearEditSelection();
    std::set<std::size_t> selectedNotes_, selectedTiming_, selectedEffects_;
    std::optional<std::pair<finger_drum::chart::MusicalPosition, finger_drum::chart::MusicalPosition>>
        noteSelectionRange_;
    std::optional<std::size_t> timingSelectionAnchor_, effectSelectionAnchor_;
    std::optional<Clipboard> clipboard_;
    void DocumentChanged();
    EditorTab tab_{EditorTab::Pattern};
    bool rebuild_{true};
    std::string status_;
    std::filesystem::path savedPath_;
    finger_drum::GameplayLaunchRequest request_;
    std::unique_ptr<finger_drum::chart::IEditorDocument> editor_;
    std::unique_ptr<IEditorMode> mode_;
    EditorAnalysisController analysis_;
    EditorPlayback playback_;
    std::optional<AudioSelection> audioSelection_;
    EditorMetronome metronome_;
    EditorScoreState score_;
    double timeMs_{};
    finger_drum::rhythm::RhythmTime lastPlaybackPresentation_{};
    bool nextZToZero_{};
    bool nextXToMusicEnd_{};
    std::uint64_t audioSourceRevision_{};
};
