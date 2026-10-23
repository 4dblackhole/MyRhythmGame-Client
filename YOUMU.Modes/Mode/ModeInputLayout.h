#pragma once
#include "Common/RhythmTypes.h"
#include "Mode/PlayOptions.h"
#include <map>
#include <string>
#include <vector>

// Stable identifiers belong to the mode. The first key is primary; the
// remaining keys are auxiliary. PhysicalKey contains a Win32 Virtual-Key.
struct ModeInputBinding
{
    std::string id, displayName;
    NoteAction action{};
    std::vector<PhysicalKey> keys;
};
using InputKeyOverrides = std::map<std::string, std::vector<PhysicalKey>>;
struct ModeInputLayout
{
    std::string modeId;
    std::vector<ModeInputBinding> bindings;
};
struct PlaySessionOptions
{
    RhythmDuration globalOffset{};
    InputKeyOverrides inputKeys;
    PlayOptions play;
};
// Resolve and validate before publishing settings or constructing a session.
[[nodiscard]] ModeInputLayout ResolveInputLayout(ModeInputLayout defaults, const InputKeyOverrides &overrides);
