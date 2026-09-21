#include "Taiko/TaikoMode.h"
#include "Taiko/Submodules/TaikoSessionBuilder.h"

namespace finger_drum::mode
{
    std::string_view TaikoMode::Id() const noexcept
    {
        return "Taiko";
    }

    ModeLoadResult TaikoMode::LoadSession(
        const std::filesystem::path &patternPath,
        const std::optional<std::filesystem::path> &effectPath) const
    {
        chart::ChartParser parser;
        chart::ParseResult<chart::PatternDocument> pattern = parser.ParsePatternFile(patternPath);
        chart::ParseResult<chart::EffectDocument> effect;
        if (effectPath.has_value())
        {
            effect = parser.ParseEffectFile(*effectPath);
        }

        ModeLoadResult result;
        result.diagnostics = std::move(pattern.diagnostics);
        result.diagnostics.insert(result.diagnostics.end(),
                                  std::make_move_iterator(effect.diagnostics.begin()),
                                  std::make_move_iterator(effect.diagnostics.end()));
        if (std::ranges::any_of(result.diagnostics, [](const chart::Diagnostic &diagnostic) {
                return diagnostic.severity == chart::DiagnosticSeverity::Error;
            }))
        {
            return result;
        }

        ModeLoadResult created = CreateSession(pattern.document, effect.document);
        result.session = std::move(created.session);
        result.diagnostics.insert(result.diagnostics.end(),
                                  std::make_move_iterator(created.diagnostics.begin()),
                                  std::make_move_iterator(created.diagnostics.end()));
        return result;
    }

    ModeLoadResult TaikoMode::CreateSession(const chart::PatternDocument &pattern,
                                            const chart::EffectDocument &effects) const
    {
        return TaikoSessionBuilder::CreateSession(pattern, effects);
    }
} // namespace finger_drum::mode
