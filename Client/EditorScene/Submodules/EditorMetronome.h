#pragma once
#include "Editing/IEditorDocument.h"
#include "Modes/IEditorAudioSource.h"
#include <map>

// Playback-only cues. Clicks never enter the hit-sound FFT analysis source.
class EditorMetronome final
{
  public:
    struct Beat
    {
        double seconds{};
        bool accent{};
    };
    void SetEnabled(bool enabled) noexcept
    {
        enabled_ = enabled;
    }
    bool Enabled() const noexcept
    {
        return enabled_;
    }
    void Prepare(const finger_drum::chart::IEditorDocument &, double endSeconds,
                 const std::map<std::string, std::filesystem::path> &, std::uint64_t filesRevision,
                 const std::vector<AudioMarker> &);
    const auto &Files() const noexcept
    {
        return files_;
    }
    const auto &Cues() const noexcept
    {
        return cues_;
    }
    const auto &Beats() const noexcept
    {
        return beats_;
    }
    std::uint64_t CueRevision() const noexcept
    {
        return cueRevision_;
    }

  private:
    bool enabled_{}, preparedEnabled_{};
    const finger_drum::chart::IEditorDocument *document_{};
    std::uint64_t documentRevision_{}, cueRevision_{};
    std::optional<std::uint64_t> filesRevision_;
    double endSeconds_{};
    std::map<std::string, std::filesystem::path> files_;
    std::vector<AudioMarker> cues_;
    std::vector<Beat> beats_;
};
