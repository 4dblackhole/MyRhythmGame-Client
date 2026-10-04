#pragma once
#include "Note/Note.h"
#include "Taiko/TaikoMode.h"
#include "TaikoChartAudio.h"
#include "TaikoNoteOptions.h"
#include <algorithm>
#include <cctype>
#include <charconv>
#include <map>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

namespace finger_drum::mode::taiko_build
{
    using namespace taiko_options;
    using taiko_audio::ChartSoundId;
    inline void ConfigureHitSounds(PlaySession &session, const chart::PatternDocument &pattern,
                                   const chart::EffectDocument &effects, const chart::MusicalTimeline &timeline,
                                   std::vector<chart::Diagnostic> &diagnostics)
    {
        std::map<rhythm::SoundId, std::filesystem::path> files;
        for (const auto &[index, path] : pattern.hitSounds)
        {
            files.emplace(ChartSoundId(index), (pattern.sourcePath.parent_path() / path).lexically_normal());
        }
        session.SetHitSoundFiles(std::move(files));

        std::vector<TimedSoundOverride> donChanges;
        std::vector<TimedSoundOverride> katChanges;
        for (const auto &change : effects.hitSoundChanges)
        {
            if ((change.keyType != 1 && change.keyType != 2) || !pattern.hitSounds.contains(change.soundIndex) ||
                change.position.measure < 0 || change.position.fraction < chart::Rational{} ||
                change.position.fraction >= timeline.MeasureLength(change.position.measure))
            {
                diagnostics.push_back({chart::DiagnosticSeverity::Error, change.source,
                                       "Hit sound change references an undefined table index, "
                                       "invalid key ID, or a position outside its measure."});
                continue;
            }
            auto &changes = change.keyType == 1 ? donChanges : katChanges;
            changes.push_back({timeline.Compile(change.position), ChartSoundId(change.soundIndex)});
        }
        for (const auto &binding : taiko_audio::TimedSoundBindings)
            session.SetSoundOverrides(binding.sound, binding.keyType == 1 ? donChanges : katChanges);
    }

    [[nodiscard]] inline rhythm::NoteAction ActionValue(const TaikoAction action) noexcept
    {
        return static_cast<rhythm::NoteAction>(action);
    }

    [[nodiscard]] inline rhythm::AudioCueRequest Cue(std::string sound, std::string bus = "HitSound")
    {
        rhythm::AudioCueRequest result;
        result.sound = std::move(sound);
        result.bus = std::move(bus);
        return result;
    }

    [[nodiscard]] inline bool IsDonType(const TaikoNoteType type) noexcept
    {
        return type == TaikoNoteType::Don || type == TaikoNoteType::BigDon;
    }

    [[nodiscard]] inline NoteVisualKind LongNoteVisualId(TaikoNoteType type,
                                                         std::optional<TaikoAction> buzzAction = std::nullopt) noexcept
    {
        return TaikoVisual(type, buzzAction.value_or(TaikoAction::Don));
    }
    [[nodiscard]] inline NoteVisualKind TapVisualId(TaikoNoteType type) noexcept
    {
        return TaikoVisual(type);
    }
} // namespace finger_drum::mode::taiko_build
