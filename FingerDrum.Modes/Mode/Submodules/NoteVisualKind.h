#pragma once
#include <cstdint>

namespace finger_drum::mode
{
    // Semantic appearance; Client alone maps these values to skin images.
    enum class NoteVisualKind : std::uint8_t
    {
        Unknown,
        Don,
        Kat,
        BigDon,
        BigKat,
        Purple,
        Roll,
        BigRoll,
        Balloon,
        DengDeng,
        DonBuzz,
        KatBuzz
    };
} // namespace finger_drum::mode
