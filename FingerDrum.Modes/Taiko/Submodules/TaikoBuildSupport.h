#pragma once
#include "Taiko/TaikoMode.h"
#include "Note/Note.h"
#include <algorithm>
#include <charconv>
#include <cctype>
#include <map>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

namespace finger_drum::mode::taiko_build
{
    [[nodiscard]] inline std::string ChartSoundId(const std::string &index)
    {
        return "Chart.HitSound." + index;
    }

    inline void ConfigureHitSounds(PlaySession &session, const chart::PatternDocument &pattern,
                                   const chart::EffectDocument &effects,
                                   const chart::MusicalTimeline &timeline,
                                   std::vector<chart::Diagnostic> &diagnostics)
    {
        std::map<rhythm::SoundId, std::filesystem::path> files;
        for (const auto &[index, path] : pattern.hitSounds)
        {
            files.emplace(ChartSoundId(index),
                          (pattern.sourcePath.parent_path() / path).lexically_normal());
        }
        session.SetHitSoundFiles(std::move(files));

        std::vector<TimedSoundOverride> donChanges;
        std::vector<TimedSoundOverride> katChanges;
        for (const auto &change : effects.hitSoundChanges)
        {
            if ((change.keyType != 1 && change.keyType != 2) ||
                !pattern.hitSounds.contains(change.soundIndex) || change.position.measure < 0 ||
                change.position.fraction < chart::Rational{} ||
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
        for (const char *id : {finger_drum::mode::taiko_sound::DonHit,
                               finger_drum::mode::taiko_sound::BigDonFirstHit,
                               finger_drum::mode::taiko_sound::DonFreeInput,
                               finger_drum::mode::taiko_sound::LongNoteTick})
        {
            session.SetSoundOverrides(id, donChanges);
        }
        for (const char *id : {finger_drum::mode::taiko_sound::KatHit,
                               finger_drum::mode::taiko_sound::BigKatFirstHit,
                               finger_drum::mode::taiko_sound::KatFreeInput})
        {
            session.SetSoundOverrides(id, katChanges);
        }
    }

    [[nodiscard]] inline rhythm::NoteAction ActionValue(const TaikoAction action) noexcept
    {
        return static_cast<rhythm::NoteAction>(action);
    }

    [[nodiscard]] inline rhythm::AudioCueRequest Cue(std::string sound,
                                                     std::string bus = "HitSound")
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

    [[nodiscard]] inline std::string_view Trim(std::string_view value) noexcept
    {
        while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())) != 0)
        {
            value.remove_prefix(1);
        }
        while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0)
        {
            value.remove_suffix(1);
        }
        return value;
    }

    [[nodiscard]] inline bool EqualsInsensitive(const std::string_view left,
                                                const std::string_view right) noexcept
    {
        return left.size() == right.size() &&
               std::ranges::equal(left, right, [](const char a, const char b) {
                   return std::tolower(static_cast<unsigned char>(a)) ==
                          std::tolower(static_cast<unsigned char>(b));
               });
    }

    [[nodiscard]] inline std::optional<std::string_view> FindOption(
        const chart::PatternNote &note, const std::string_view name) noexcept
    {
        for (const std::string &field : note.extraData)
        {
            const std::string_view view = field;
            const std::size_t equals = view.find('=');
            if (equals != std::string_view::npos &&
                EqualsInsensitive(Trim(view.substr(0, equals)), name))
            {
                return Trim(view.substr(equals + 1));
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] inline bool ParsePositiveSize(const std::string_view value,
                                                std::size_t &output) noexcept
    {
        const char *const begin = value.data();
        const char *const end = begin + value.size();
        const auto parsed = std::from_chars(begin, end, output);
        return parsed.ec == std::errc{} && parsed.ptr == end && output > 0;
    }

    [[nodiscard]] inline std::optional<std::size_t> ReadPositiveOption(
        const chart::PatternNote &note, const std::string_view name,
        const std::optional<std::size_t> fallback, std::vector<chart::Diagnostic> &diagnostics)
    {
        const std::optional<std::string_view> value = FindOption(note, name);
        if (!value.has_value())
        {
            if (fallback.has_value())
            {
                return fallback;
            }
            diagnostics.push_back({chart::DiagnosticSeverity::Error, note.source,
                                   std::string(name) + " is required for this long note."});
            return std::nullopt;
        }

        std::size_t parsed{};
        if (!ParsePositiveSize(*value, parsed) || parsed > 1024)
        {
            diagnostics.push_back({chart::DiagnosticSeverity::Error, note.source,
                                   std::string(name) + " must be an integer from 1 through 1024."});
            return std::nullopt;
        }
        return parsed;
    }

    [[nodiscard]] inline std::optional<std::size_t> ReadHitCount(
        const chart::PatternNote &note, const std::size_t fallback,
        std::vector<chart::Diagnostic> &diagnostics)
    {
        std::optional<std::string_view> value;
        for (const std::string &field : note.extraData)
        {
            const std::string_view trimmed = Trim(field);
            if (!trimmed.empty() && trimmed.find('=') == std::string_view::npos)
            {
                value = trimmed;
                break;
            }
        }
        if (!value.has_value())
        {
            value = FindOption(note, finger_drum::mode::taiko_option::HitCount);
        }

        std::size_t parsed = fallback;
        if ((value.has_value() && !ParsePositiveSize(*value, parsed)) || parsed == 0 ||
            parsed > 1024)
        {
            diagnostics.push_back({chart::DiagnosticSeverity::Error, note.source,
                                   "Hit count must be an integer from 1 through 1024."});
            return std::nullopt;
        }
        return parsed;
    }

    [[nodiscard]] inline std::optional<TaikoAction> ReadBuzzAction(
        const chart::PatternNote &note, std::vector<chart::Diagnostic> &diagnostics)
    {
        const std::optional<std::string_view> value =
            FindOption(note, finger_drum::mode::taiko_option::Action);
        if (value.has_value() && EqualsInsensitive(*value, "Don"))
        {
            return TaikoAction::Don;
        }
        if (value.has_value() && EqualsInsensitive(*value, "Kat"))
        {
            return TaikoAction::Kat;
        }
        diagnostics.push_back({chart::DiagnosticSeverity::Error, note.source,
                               "Buzz requires Action=Don or Action=Kat."});
        return std::nullopt;
    }

    [[nodiscard]] inline NoteVisualKind LongNoteVisualId(
        TaikoNoteType type, std::optional<TaikoAction> buzzAction = std::nullopt) noexcept
    {
        return TaikoVisual(type, buzzAction.value_or(TaikoAction::Don));
    }
    [[nodiscard]] inline NoteVisualKind TapVisualId(TaikoNoteType type) noexcept
    {
        return TaikoVisual(type);
    }
} // namespace finger_drum::mode::taiko_build
