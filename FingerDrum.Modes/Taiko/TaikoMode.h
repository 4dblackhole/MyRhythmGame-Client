#pragma once

#include "Mode/PlayGameMode.h"

namespace finger_drum::mode
{
    enum class TaikoAction : rhythm::NoteAction
    {
        Don = 1,
        Kat = 2,
    };

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
        [[nodiscard]] static std::shared_ptr<const rhythm::INoteSoundPolicy>
            MakeTapSoundPolicy(std::string soundId);
        [[nodiscard]] static std::shared_ptr<const rhythm::INoteSoundPolicy>
            MakeBigSoundPolicy(std::string soundId);
        [[nodiscard]] static std::shared_ptr<const rhythm::INoteSoundPolicy>
            MakeTickSoundPolicy(std::string soundId);
    };
}
