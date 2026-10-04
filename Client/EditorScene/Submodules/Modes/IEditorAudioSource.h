#pragma once
#include "Editing/IEditorDocument.h"
#include "GameFlow/GameplayLaunchRequest.h"
#include <map>

struct AudioMarker
{
    double seconds{};
    std::string sound;
};

// Analysis depends only on immutable audio queries, not mode UI or Workspace.
class IEditorAudioSource
{
  public:
    virtual ~IEditorAudioSource() = default;
    virtual std::map<std::string, std::filesystem::path> AudioFiles(
        const finger_drum::chart::IEditorDocument &, const finger_drum::GameplayLaunchRequest &) const = 0;
    virtual std::vector<AudioMarker> AudioMarkers(const finger_drum::chart::IEditorDocument &) const = 0;
};
