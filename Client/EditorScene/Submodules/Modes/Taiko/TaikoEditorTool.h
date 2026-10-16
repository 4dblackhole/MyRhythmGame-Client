#pragma once
#include "Taiko/Submodules/TaikoNoteDefinition.h"
#include "Taiko/Submodules/TaikoSoundIds.h"
#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace editor_tools
{
    enum class Group
    {
        Small,
        Big,
        Roll,
        TickRoll,
        Focus,
        Buzz
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
        Define(Type::TickRoll, Group::TickRoll),
        Define(Type::BigRoll, Group::Roll),
        Define(Type::BigTickRoll, Group::TickRoll),
        Define(Type::Balloon, Group::Focus),
        Define(Type::DengDeng, Group::Focus),
        Define(Type::Buzz, Group::Buzz),
        Tool{18, Type::Buzz, Group::Buzz, finger_drum::mode::TaikoAction::Kat},
    };
    constexpr const Tool *Find(int id) noexcept
    {
        for (const auto &tool : Tools)
            if (tool.id == id)
                return &tool;
        return nullptr;
    }
    constexpr int NextInGroup(int id) noexcept
    {
        const auto *current = Find(id);
        if (!current)
            return id;
        const auto index = static_cast<std::size_t>(current - Tools.data());
        for (std::size_t offset = 1; offset <= Tools.size(); ++offset)
        {
            const auto &next = Tools[(index + offset) % Tools.size()];
            if (next.group == current->group)
                return next.id;
        }
        return id;
    }
    inline std::wstring_view Name(int id, const std::array<std::wstring_view, Tools.size()> &names) noexcept
    {
        for (std::size_t i = 0; i < Tools.size(); ++i)
            if (Tools[i].id == id)
                return names[i];
        return {};
    }
    inline bool UsesHitCount(int id) noexcept
    {
        const auto *tool = Find(id);
        return tool && (tool->note == Type::Roll || tool->note == Type::BigRoll || tool->note == Type::Balloon ||
                        tool->note == Type::DengDeng);
    }
    inline bool UsesTickDivision(int id) noexcept
    {
        const auto *tool = Find(id);
        return tool && (tool->note == Type::TickRoll || tool->note == Type::BigTickRoll || tool->note == Type::Buzz);
    }
    inline std::vector<std::string> ExtraData(const Tool &tool)
    {
        using namespace finger_drum::mode;
        if (tool.note != Type::Buzz)
            return {};
        return {std::string(taiko_option::Action) + (tool.buzzAction == TaikoAction::Kat ? "=Kat" : "=Don"),
                std::string(taiko_option::TickDivision) + "=16"};
    }
} // namespace editor_tools
