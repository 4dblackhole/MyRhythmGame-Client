#pragma once
#include "MRG_Core.h"
#include "SongSelectionState.h"
#include <array>
#include <optional>

class SongPreviewController final
{
    struct PreviewSlot
    {
        std::shared_ptr<mrg::audio::AudioClip> clip;
        mrg::audio::AudioPlaybackId playbackId{mrg::audio::InvalidAudioPlaybackId};
        std::optional<std::size_t> catalogIndex;
        float volume{};
        float targetVolume{};
    };

  public:
    explicit SongPreviewController(mrg::audio::AudioPlaybackManager &playback)
        : audioPlayback_(playback)
    {
    }
    ~SongPreviewController()
    {
        StopPreviewAudio();
    }
    void Initialize(mrg::audio::AudioSystem &audio)
    {
        audioSystem_ = &audio;
    }
    void SyncPreviewToFocusedSong(const SongSelectionState &selection, bool active);
    void StartSongPreview(const SongSelectionState &selection, const std::size_t catalogIndex);
    void UpdatePreviewAudio(const double deltaSeconds);
    void StopPreviewAudio() noexcept;
    void StopPreviewSlot(PreviewSlot &slot) noexcept;

  private:
    mrg::audio::AudioPlaybackManager &audioPlayback_;
    mrg::audio::AudioSystem *audioSystem_{};
    std::array<PreviewSlot, 2> previewSlots_{};
    std::optional<std::size_t> currentPreviewSlot_;
};
