#pragma once
#include "IEditorMode.h"

// Explicitly supported modes only. Empty IDs retain the legacy Taiko default.
std::unique_ptr<IEditorMode> CreateEditorMode(std::string_view id);
