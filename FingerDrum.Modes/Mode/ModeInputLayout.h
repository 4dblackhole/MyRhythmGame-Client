#pragma once
#include "Common/RhythmTypes.h"
#include <map>
#include <string>
#include <vector>

namespace finger_drum::mode
{
    // Stable identifiers belong to the mode. The first key is primary; the
    // remaining keys are auxiliary. PhysicalKey contains a Win32 Virtual-Key.
    struct ModeInputBinding
    {
        std::string id, displayName;
        rhythm::NoteAction action{};
        std::vector<rhythm::PhysicalKey> keys;
    };
    using InputKeyOverrides = std::map<std::string, std::vector<rhythm::PhysicalKey>>;
    struct ModeInputLayout
    {
        std::string modeId;
        std::vector<ModeInputBinding> bindings;
    };
    struct PlaySessionOptions
    {
        rhythm::RhythmDuration globalOffset{};
        InputKeyOverrides inputKeys;
    };
    // Resolve and validate before publishing settings or constructing a session.
    [[nodiscard]] ModeInputLayout ResolveInputLayout(ModeInputLayout defaults, const InputKeyOverrides &overrides);
} // namespace finger_drum::mode
