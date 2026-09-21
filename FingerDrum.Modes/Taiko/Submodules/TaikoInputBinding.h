#pragma once
#include "Common/RhythmTypes.h"
#include <array>

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

        [[nodiscard]] bool Contains(const rhythm::PhysicalKey physicalKey) const noexcept
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

        [[nodiscard]] bool IsPrimary(const rhythm::PhysicalKey physicalKey) const noexcept
        {
            return physicalKey == primaryKey;
        }
    };

    inline constexpr std::array<TaikoInputBinding, 4> TaikoInputBindings{{
        {TaikoAction::Kat, 'D', {'C', 0}},
        {TaikoAction::Don, 'F', {'V', 0}},
        {TaikoAction::Don, 'J', {'N', 0}},
        {TaikoAction::Kat, 'K', {'M', 0}},
    }};
} // namespace finger_drum::mode
