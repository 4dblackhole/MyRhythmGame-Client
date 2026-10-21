#include "Mode/PlayGameMode.h"

bool ModeLoadResult::Succeeded() const noexcept
{
    if (session == nullptr)
    {
        return false;
    }
    return std::ranges::none_of(diagnostics, [](const Diagnostic &diagnostic) {
        return diagnostic.severity == DiagnosticSeverity::Error;
    });
}
