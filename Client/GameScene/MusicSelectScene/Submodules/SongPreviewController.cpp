#include "SongPreviewController.h"
#include <algorithm>
namespace
{
    constexpr double PreviewFadeSeconds = 0.2;
}

void SongPreviewController::SyncPreviewToFocusedSong(const SongSelectionState &selection,
                                                     bool active)
{
    if (!active || audioSystem_ == nullptr)
    {
        return;
    }

    if (selection.visibleSongIndices_.empty() ||
        selection.focusedSongPosition_ >= selection.visibleSongIndices_.size())
    {
        if (currentPreviewSlot_.has_value())
        {
            previewSlots_[*currentPreviewSlot_].targetVolume = 0.0F;
            currentPreviewSlot_.reset();
        }
        return;
    }

    const std::size_t catalogIndex = selection.visibleSongIndices_[selection.focusedSongPosition_];
    if (pendingPreviewSlot_)
    {
        auto &pending = previewSlots_[*pendingPreviewSlot_];
        std::string error;
        const auto state = pending.clip->LoadState(error);
        if (state == mrg::audio::AudioClipLoadState::Loading)
            return;
        const auto slot = *pendingPreviewSlot_;
        pendingPreviewSlot_.reset();
        if (pending.catalogIndex == catalogIndex && state == mrg::audio::AudioClipLoadState::Ready)
        {
            PlayPendingPreview(slot);
            return;
        }
        if (pending.catalogIndex == catalogIndex)
            failedPreviewIndex_ = catalogIndex;
        StopPreviewSlot(pending);
    }
    if (failedPreviewIndex_ == catalogIndex)
        return;
    failedPreviewIndex_.reset();
    if (currentPreviewSlot_.has_value())
    {
        const PreviewSlot &current = previewSlots_[*currentPreviewSlot_];
        if (current.catalogIndex == catalogIndex &&
            current.playbackId != mrg::audio::InvalidAudioPlaybackId)
        {
            return;
        }
    }

    StartSongPreview(selection, catalogIndex);
}

void SongPreviewController::StartSongPreview(const SongSelectionState &selection,
                                             const std::size_t catalogIndex)
{
    if (audioSystem_ == nullptr || catalogIndex >= selection.catalog_.songs.size())
    {
        return;
    }

    if (currentPreviewSlot_.has_value())
    {
        previewSlots_[*currentPreviewSlot_].targetVolume = 0.0F;
    }

    const std::size_t nextSlotIndex =
        currentPreviewSlot_.has_value()
            ? 1U - *currentPreviewSlot_
            : (previewSlots_[0].playbackId == mrg::audio::InvalidAudioPlaybackId ? 0U : 1U);
    PreviewSlot &nextSlot = previewSlots_[nextSlotIndex];
    StopPreviewSlot(nextSlot);

    std::string errorMessage;
    std::unique_ptr<mrg::audio::AudioClip> loadedClip =
        audioSystem_->LoadSound(selection.catalog_.songs[catalogIndex].audioPath,
                                mrg::audio::AudioLoadMode::StreamAsync, errorMessage);
    if (loadedClip == nullptr)
    {
        failedPreviewIndex_ = catalogIndex;
        return;
    }

    nextSlot.clip = std::shared_ptr<mrg::audio::AudioClip>(std::move(loadedClip));
    nextSlot.catalogIndex = catalogIndex;
    pendingPreviewSlot_ = nextSlotIndex;
}

void SongPreviewController::PlayPendingPreview(std::size_t slotIndex)
{
    auto &nextSlot = previewSlots_[slotIndex];
    std::string errorMessage;
    mrg::audio::AudioPlaybackSettings settings;
    settings.volume = 0.0F;
    const mrg::audio::AudioPlaybackId playbackId =
        audioPlayback_.Play(nextSlot.clip, settings, nullptr, errorMessage);
    if (playbackId == mrg::audio::InvalidAudioPlaybackId)
    {
        failedPreviewIndex_ = nextSlot.catalogIndex;
        StopPreviewSlot(nextSlot);
        return;
    }

    nextSlot.playbackId = playbackId;
    nextSlot.volume = 0.0F;
    nextSlot.targetVolume = 1.0F;
    currentPreviewSlot_ = slotIndex;
}

void SongPreviewController::UpdatePreviewAudio(const double deltaSeconds)
{
    PollRetiredClips();
    const float fadeStep = static_cast<float>(std::max(deltaSeconds, 0.0) / PreviewFadeSeconds);

    for (std::size_t index = 0; index < previewSlots_.size(); ++index)
    {
        PreviewSlot &slot = previewSlots_[index];
        if (slot.playbackId == mrg::audio::InvalidAudioPlaybackId)
        {
            continue;
        }

        mrg::audio::AudioVoice *const voice = audioPlayback_.FindVoice(slot.playbackId);
        if (voice == nullptr)
        {
            StopPreviewSlot(slot);
            if (currentPreviewSlot_ == index)
            {
                currentPreviewSlot_.reset();
            }
            continue;
        }

        if (slot.volume < slot.targetVolume)
        {
            slot.volume = std::min(slot.volume + fadeStep, slot.targetVolume);
        }
        else if (slot.volume > slot.targetVolume)
        {
            slot.volume = std::max(slot.volume - fadeStep, slot.targetVolume);
        }

        std::string errorMessage;
        if (!voice->SetVolume(slot.volume, errorMessage))
        {
            StopPreviewSlot(slot);
            if (currentPreviewSlot_ == index)
            {
                currentPreviewSlot_.reset();
            }
            continue;
        }

        if (slot.targetVolume <= 0.0F && slot.volume <= 0.0F)
        {
            StopPreviewSlot(slot);
            if (currentPreviewSlot_ == index)
            {
                currentPreviewSlot_.reset();
            }
        }
    }
}

void SongPreviewController::StopPreviewAudio() noexcept
{
    for (PreviewSlot &slot : previewSlots_)
    {
        StopPreviewSlot(slot);
    }
    currentPreviewSlot_.reset();
    pendingPreviewSlot_.reset();
    failedPreviewIndex_.reset();
}

void SongPreviewController::StopPreviewSlot(PreviewSlot &slot) noexcept
{
    if (slot.playbackId != mrg::audio::InvalidAudioPlaybackId)
    {
        try
        {
            std::string ignoredError;
            static_cast<void>(audioPlayback_.Stop(slot.playbackId, ignoredError));
        }
        catch (...)
        {
        }
    }
    // Native async loading and recently stopped streams may still be busy.
    // Keep their handles until polling reports a safe release state.
    if (slot.clip)
    {
        try
        {
            std::string error;
            const auto state = slot.clip->LoadState(error);
            if (state == mrg::audio::AudioClipLoadState::Loading || state == mrg::audio::AudioClipLoadState::Playing)
                retiredClips_.push_back(std::move(slot.clip));
        }
        catch (...) {}
    }
    slot = {};
}

void SongPreviewController::PollRetiredClips()
{
    std::erase_if(retiredClips_, [](const auto &clip) {
        std::string error;
        const auto state = clip->LoadState(error);
        return state == mrg::audio::AudioClipLoadState::Ready || state == mrg::audio::AudioClipLoadState::Failed;
    });
}
