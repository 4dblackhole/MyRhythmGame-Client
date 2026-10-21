#include "Note/Submodules/INoteRule.h"
#include "Note/Submodules/RuleHelpers.h"
#include <algorithm>
#include <stdexcept>
#include <utility>

bool NoteUpdateContext::IsHeld(const NoteAction action) const noexcept
{
    return std::ranges::find(heldActions, action) != heldActions.end();
}
