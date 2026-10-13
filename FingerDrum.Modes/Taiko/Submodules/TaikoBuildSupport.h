#pragma once
#include "Note/Submodules/NoteTypes.h"
#include "Taiko/TaikoMode.h"
#include "TaikoChartAudio.h"
#include "TaikoNoteOptions.h"
#include "Automation/InterpolationExpression.h"
#include "Timing/MusicalTimeline.h"
#include <algorithm>
#include <cctype>
#include <charconv>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string_view>
#include <utility>

namespace finger_drum::mode::taiko_build
{
    using namespace taiko_options;
    using taiko_audio::ChartSoundId;
    // Borrow the already compiled commands for this build. Equal-time entries
    // retain CompileEffects' musical-position/authored ordering.
    class EffectLookup final
    {
      public:
        explicit EffectLookup(const std::vector<chart::CompiledEffectCommand> &commands)
        {
            for (const auto &command : commands)
                index_.insert_or_assign({command.command.type, command.timing}, &command);
        }
        const chart::CompiledEffectCommand *Find(chart::EffectCommandType type, rhythm::RhythmTime time) const
        {
            const auto next = index_.upper_bound({type, time});
            if (next == index_.begin())
                return nullptr;
            const auto previous = std::prev(next);
            return previous->first.first == type ? previous->second : nullptr;
        }
        double Value(chart::EffectCommandType type, rhythm::RhythmTime time, double fallback = 1) const
        {
            const auto *selected = Find(type, time);
            if (!selected)
                return fallback;
            if (selected->duration <= rhythm::RhythmDuration::zero())
                return selected->command.endValue;
            return chart::EvaluateInterpolation(
                selected->command,
                static_cast<double>(static_cast<long double>(time.count()) - selected->timing.count()) /
                    static_cast<double>(selected->duration.count()));
        }

      private:
        std::map<std::pair<chart::EffectCommandType, rhythm::RhythmTime>, const chart::CompiledEffectCommand *> index_;
    };
    inline bool UsesSyncopation(const EffectLookup &effects, const chart::MusicalTimeline &timeline,
                                chart::MusicalPosition position)
    {
        const auto *selected = effects.Find(chart::EffectCommandType::SyncopationZone, timeline.Compile(position));
        if (!selected || selected->command.endValue < .5)
            return false;
        for (const auto division : selected->command.excludedDivisions)
            if ((position.fraction * static_cast<std::int64_t>(division * 4)).Denominator() == 1)
                return false;
        return true;
    }
    inline void ConfigureHitSounds(PlaySession &session, const chart::PatternDocument &pattern,
                                   const chart::EffectDocument &effects, const chart::MusicalTimeline &timeline,
                                   std::vector<chart::Diagnostic> &diagnostics)
    {
        std::map<rhythm::SoundId, std::filesystem::path> files;
        std::set<std::string> used;
        for (const auto &note : pattern.notes)
            if (!note.hitSound.empty())
                used.insert(note.hitSound);
        for (const auto &change : effects.hitSoundChanges)
            if (timeline.IsPositionActive(change.position))
                used.insert(change.soundIndex);
        for (const auto &[index, path] : effects.hitSounds)
        {
            if (used.contains(index))
                files.emplace(ChartSoundId(index), (effects.sourcePath.parent_path() / path).lexically_normal());
        }
        session.SetHitSoundFiles(std::move(files));

        std::vector<TimedSoundOverride> donChanges;
        std::vector<TimedSoundOverride> katChanges;
        for (const auto &change : effects.hitSoundChanges)
        {
            if ((change.keyType != 1 && change.keyType != 2) || !effects.hitSounds.contains(change.soundIndex) ||
                change.position.measure < 0 || change.position.measure >= chart::MaximumMeasureCount ||
                change.position.fraction < chart::Rational{} ||
                change.position.fraction >= timeline.MeasureLength(change.position.measure))
            {
                diagnostics.push_back({chart::DiagnosticSeverity::Error, change.source,
                                       "Hit sound change references an undefined table index, "
                                       "invalid key ID, or a position outside its measure."});
                continue;
            }
            if (!timeline.IsPositionActive(change.position))
                continue;
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
