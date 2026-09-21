#include "Note/Submodules/INoteRule.h"
#include "Note/Submodules/RuleHelpers.h"
#include <algorithm>
#include <format>
#include <iterator>
#include <numeric>
#include <stdexcept>
#include <utility>

namespace finger_drum::rhythm
{
    using namespace detail;

    bool NoteUpdateContext::IsHeld(const NoteAction action) const noexcept
    {
        return std::ranges::find(heldActions, action) != heldActions.end();
    }
} // namespace finger_drum::rhythm
