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
        const IEditorDocument &, const GameplayLaunchRequest &) const = 0;
    virtual std::vector<AudioMarker> AudioMarkers(const IEditorDocument &) const = 0;
};
