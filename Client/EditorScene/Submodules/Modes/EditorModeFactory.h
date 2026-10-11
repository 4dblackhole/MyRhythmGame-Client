#pragma once
#include "IEditorMode.h"

// Select only an explicitly named supported mode.
std::unique_ptr<IEditorMode> CreateEditorMode(std::string_view id);
