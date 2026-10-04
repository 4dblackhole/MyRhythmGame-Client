#include "EditorModeFactory.h"
#include "Taiko/TaikoEditorMode.h"
#include <stdexcept>

std::unique_ptr<IEditorMode> CreateEditorMode(const std::string_view id)
{
    if (id.empty() || id == "Taiko")
        return std::make_unique<TaikoEditorMode>();
    throw std::invalid_argument("Unsupported editor mode: " + std::string(id));
}
