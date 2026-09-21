#pragma once
#include "Taiko/Submodules/TaikoInputBinding.h"
#include "Mode/Submodules/NoteVisualKind.h"
#include <optional>
#include <array>
#include <string_view>

namespace finger_drum::mode
{
    enum class TaikoNoteType : int
    {
        Don = 1,
        Kat = 2,
        BigDon = 3,
        BigKat = 4,
        Purple = 5,
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

    struct TaikoNoteDefinition
    {
        TaikoNoteType type;
        NoteVisualKind visual;
        bool longNote;
    };

    inline constexpr std::array TaikoNoteDefinitions{
        TaikoNoteDefinition{TaikoNoteType::Don, NoteVisualKind::Don, false},
        TaikoNoteDefinition{TaikoNoteType::Kat, NoteVisualKind::Kat, false},
        TaikoNoteDefinition{TaikoNoteType::BigDon, NoteVisualKind::BigDon, false},
        TaikoNoteDefinition{TaikoNoteType::BigKat, NoteVisualKind::BigKat, false},
        TaikoNoteDefinition{TaikoNoteType::Purple, NoteVisualKind::Purple, false},
        TaikoNoteDefinition{TaikoNoteType::Roll, NoteVisualKind::Roll, true},
        TaikoNoteDefinition{TaikoNoteType::TickRoll, NoteVisualKind::Roll, true},
        TaikoNoteDefinition{TaikoNoteType::BigRoll, NoteVisualKind::BigRoll, true},
        TaikoNoteDefinition{TaikoNoteType::BigTickRoll, NoteVisualKind::BigRoll, true},
        TaikoNoteDefinition{TaikoNoteType::Balloon, NoteVisualKind::Balloon, true},
        TaikoNoteDefinition{TaikoNoteType::DengDeng, NoteVisualKind::DengDeng, true},
        TaikoNoteDefinition{TaikoNoteType::Buzz, NoteVisualKind::DonBuzz, true},
    };

    [[nodiscard]] constexpr const TaikoNoteDefinition *FindTaikoNote(int persistedId) noexcept
    {
        for (const auto &definition : TaikoNoteDefinitions)
            if (static_cast<int>(definition.type) == persistedId)
                return &definition;
        return nullptr;
    }

    [[nodiscard]] constexpr NoteVisualKind TaikoVisual(
        TaikoNoteType type, TaikoAction buzzAction = TaikoAction::Don) noexcept
    {
        if (type == TaikoNoteType::Buzz && buzzAction == TaikoAction::Kat)
            return NoteVisualKind::KatBuzz;
        const auto *definition = FindTaikoNote(static_cast<int>(type));
        return definition ? definition->visual : NoteVisualKind::Unknown;
    }
} // namespace finger_drum::mode
