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
        finger_drum::mode::TaikoAction buzzAction{finger_drum::mode::TaikoAction::Don};
    };
    constexpr Tool Define(finger_drum::mode::TaikoNoteType type, Group group)
    {
        return {static_cast<int>(type), type, group};
    }
    using Type = finger_drum::mode::TaikoNoteType;
    // 18 identifies an editor tool only. Both Buzz tools serialize persisted ID 17.
    inline constexpr std::array Tools{
        Define(Type::Don, Group::Small),
        Define(Type::Kat, Group::Small),
        Define(Type::BigDon, Group::Big),
        Define(Type::BigKat, Group::Big),
        Define(Type::Purple, Group::Big),
        Define(Type::Roll, Group::Roll),
        Define(Type::TickRoll, Group::Roll),
        Define(Type::BigRoll, Group::Roll),
        Define(Type::BigTickRoll, Group::Roll),
        Define(Type::Balloon, Group::Focus),
        Define(Type::DengDeng, Group::Focus),
        Define(Type::Buzz, Group::Roll),
        Tool{18, Type::Buzz, Group::Roll, finger_drum::mode::TaikoAction::Kat},
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
