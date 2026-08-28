#pragma once

#include "Mode/PlayGameMode.h"

#include <array>
#include <cstddef>

namespace finger_drum::mode
{
    enum class TaikoAction : rhythm::NoteAction
    {
        Don = 1,
        Kat = 2,
    };

    // The first physical key is the primary (strong-light) binding for the
    // matching playfield input. The remaining keys produce the same action
    // while using the secondary (weak-light) presentation.
    struct TaikoInputBinding
    {
        TaikoAction action{};
        rhythm::PhysicalKey primaryKey{};
        std::array<rhythm::PhysicalKey, 2> secondaryKeys{};

        [[nodiscard]] bool Contains(
            const rhythm::PhysicalKey physicalKey) const noexcept
        {
            if (physicalKey == primaryKey)
            {
                return true;
            }
            for (const rhythm::PhysicalKey secondaryKey : secondaryKeys)
            {
                if (physicalKey == secondaryKey)
                {
                    return true;
                }
            }
            return false;
        }

        [[nodiscard]] bool IsPrimary(
            const rhythm::PhysicalKey physicalKey) const noexcept
        {
            return physicalKey == primaryKey;
        }
    };

    inline constexpr std::array<TaikoInputBinding, 4> TaikoInputBindings{{
        {TaikoAction::Kat, 'E', {'D', 'C'}},
        {TaikoAction::Don, 'R', {'F', 'V'}},
        {TaikoAction::Don, 'U', {'J', 'M'}},
        {TaikoAction::Kat, 'I', {'K', 0xBC}},
    }};

    enum class TaikoNoteType : int
    {
        Don = 1,
        Kat = 2,
        BigDon = 3,
        BigKat = 4,
        Roll = 11,
        TickRoll = 12,
        BigRoll = 13,
        BigTickRoll = 14,
        Balloon = 15,
        DengDeng = 16,
        Buzz = 17,
    };

    enum class TaikoPatternAction : int
    {
        Down = 0,
        LongNoteStart = 1,
        LongNoteEnd = 2,
    };

    class TaikoMode final : public IPlayGameMode
    {
    public:
        [[nodiscard]] std::string_view Id() const noexcept override;
        [[nodiscard]] ModeLoadResult LoadSession(
            const std::filesystem::path& patternPath,
            const std::optional<std::filesystem::path>& effectPath =
                std::nullopt) const override;
        [[nodiscard]] ModeLoadResult CreateSession(
            const chart::PatternDocument& pattern,
            const chart::EffectDocument& effects = {}) const override;

    private:
        static void AddLongNote(
            const chart::CompiledPatternNote& head,
            const chart::CompiledPatternNote& tail,
            const chart::MusicalTimeline& timeline,
            const std::shared_ptr<const rhythm::JudgementProfile>& profile,
            rhythm::Lane& lane,
            PlaySession& session,
            rhythm::NoteId& nextId,
            std::vector<chart::Diagnostic>& diagnostics);
        [[nodiscard]] static std::shared_ptr<const rhythm::INoteSoundPolicy>
            MakeTapSoundPolicy(std::string soundId);
        [[nodiscard]] static std::shared_ptr<const rhythm::INoteSoundPolicy>
            MakeBigSoundPolicy(std::string soundId);
        [[nodiscard]] static std::shared_ptr<const rhythm::INoteSoundPolicy>
            MakeTickSoundPolicy(std::string soundId);
        [[nodiscard]] static std::shared_ptr<const rhythm::INoteSoundPolicy>
            MakeBalloonSoundPolicy();
        [[nodiscard]] static std::shared_ptr<const rhythm::INoteSoundPolicy>
            MakeAlternatingSoundPolicy(std::size_t hitCount);
    };
}
