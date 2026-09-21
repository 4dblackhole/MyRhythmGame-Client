#pragma once
#include "Taiko/Submodules/TaikoNoteDefinition.h"
#include "Taiko/Submodules/TaikoSoundIds.h"
#include <array>
#include <string>
#include <vector>

namespace editor_tools
{
    enum class Group
    {
        Small,
        Big,
        Roll,
        Focus
    };
    struct Tool
    {
        int id;
        finger_drum::mode::TaikoNoteType note;
        Group group;
        const wchar_t *label;
        finger_drum::mode::TaikoAction buzzAction{finger_drum::mode::TaikoAction::Don};
    };
    constexpr Tool Define(finger_drum::mode::TaikoNoteType type, Group group, const wchar_t *label)
    {
        return {static_cast<int>(type), type, group, label};
    }
    using Type = finger_drum::mode::TaikoNoteType;
    // 18 identifies an editor tool only. Both Buzz tools serialize persisted ID 17.
    inline constexpr std::array Tools{
        Define(Type::Don, Group::Small, L"동"),
        Define(Type::Kat, Group::Small, L"캇"),
        Define(Type::BigDon, Group::Big, L"큰 동"),
        Define(Type::BigKat, Group::Big, L"큰 캇"),
        Define(Type::Purple, Group::Big, L"보라노트"),
        Define(Type::Roll, Group::Roll, L"Roll"),
        Define(Type::TickRoll, Group::Roll, L"TickRoll"),
        Define(Type::BigRoll, Group::Roll, L"BigRoll"),
        Define(Type::BigTickRoll, Group::Roll, L"BigTickRoll"),
        Define(Type::Balloon, Group::Focus, L"Balloon"),
        Define(Type::DengDeng, Group::Focus, L"DengDeng"),
        Define(Type::Buzz, Group::Roll, L"Don Buzz"),
        Tool{18, Type::Buzz, Group::Roll, L"Kat Buzz", finger_drum::mode::TaikoAction::Kat},
    };
    constexpr const Tool *Find(int id) noexcept
    {
        for (const auto &tool : Tools)
            if (tool.id == id)
                return &tool;
        return nullptr;
    }
    inline std::vector<std::string> ExtraData(const Tool &tool)
    {
        using namespace finger_drum::mode;
        if (tool.note != Type::Buzz)
            return {};
        return {std::string(taiko_option::Action) +
                    (tool.buzzAction == TaikoAction::Kat ? "=Kat" : "=Don"),
                std::string(taiko_option::TickDivision) + "=16"};
    }
} // namespace editor_tools
