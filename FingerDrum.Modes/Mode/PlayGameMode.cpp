#include "Mode/PlayGameMode.h"

namespace finger_drum::mode
{
    bool ModeLoadResult::Succeeded() const noexcept
    {
        if (session == nullptr)
        {
            return false;
        }
        return std::ranges::none_of(diagnostics, [](const chart::Diagnostic &diagnostic) {
            return diagnostic.severity == chart::DiagnosticSeverity::Error;
        });
    }
} // namespace finger_drum::mode
