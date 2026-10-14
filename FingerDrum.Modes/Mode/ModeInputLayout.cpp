#include "Mode/ModeInputLayout.h"
#include <algorithm>
#include <set>
#include <stdexcept>

namespace finger_drum::mode
{
    ModeInputLayout ResolveInputLayout(ModeInputLayout layout, const InputKeyOverrides &overrides)
    {
        for (const auto &[id, keys] : overrides)
        {
            auto found = std::ranges::find(layout.bindings, id, &ModeInputBinding::id);
            if (found == layout.bindings.end() || found->keys.size() != keys.size())
                throw std::invalid_argument("Unknown input binding or incorrect key count: " + id);
            found->keys = keys;
        }
        std::set<rhythm::PhysicalKey> used;
        for (const auto &binding : layout.bindings)
            for (const auto key : binding.keys)
            {
                // Preserve pause/reset controls and the global performance-display shortcut.
                if (key < 8 || key > 254 || key == 27 || key == 32 || key == 'R' || key == 0x76)
                    throw std::invalid_argument("This key is reserved for game controls.");
                if (!used.insert(key).second)
                    throw std::invalid_argument("Each input in a mode must use a different key.");
            }
        return layout;
    }
} // namespace finger_drum::mode
