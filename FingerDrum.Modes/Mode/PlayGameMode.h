#pragma once

#include "Model/ChartDocument.h"
#include "Parsing/ChartParser.h"
#include "Scroll/ScrollGear.h"
#include "Timing/MusicalTimeline.h"

#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace finger_drum::mode
{
    struct AutomationValue
    {
        chart::EffectCommandType type{chart::EffectCommandType::Custom};
        std::string target;
        double value{};
    };

    class PlaySession final
    {
    public:
        rhythm::ScrollGear& Gear() noexcept;
        [[nodiscard]] const rhythm::ScrollGear& Gear() const noexcept;
        void SetInputMapping(
            std::map<rhythm::PhysicalKey, rhythm::NoteAction> mapping);
        void SetFreeInputCue(
            rhythm::NoteAction action,
            rhythm::AudioCueRequest cue);
        void SetEffects(
            std::vector<chart::CompiledEffectCommand> effects);

        [[nodiscard]] rhythm::NoteProcessResult ProcessInput(
            rhythm::PhysicalKey physicalKey,
            rhythm::InputEdge edge,
            rhythm::RhythmTime time);
        [[nodiscard]] rhythm::NoteProcessResult Update(
            rhythm::RhythmTime time,
            std::span<const rhythm::PhysicalKey> heldPhysicalKeys = {});
        [[nodiscard]] std::vector<AutomationValue> EvaluateAutomation(
            rhythm::RhythmTime time) const;
        void Reset() noexcept;

    private:
        [[nodiscard]] static double Interpolate(
            const chart::CompiledEffectCommand& command,
            rhythm::RhythmTime time) noexcept;

        rhythm::ScrollGear gear_;
        std::map<rhythm::PhysicalKey, rhythm::NoteAction> inputMapping_;
        std::map<rhythm::NoteAction, rhythm::AudioCueRequest> freeInputCues_;
        std::vector<chart::CompiledEffectCommand> effects_;
    };

    struct ModeLoadResult
    {
        std::unique_ptr<PlaySession> session;
        std::vector<chart::Diagnostic> diagnostics;

        [[nodiscard]] bool Succeeded() const noexcept;
    };

    class IPlayGameMode
    {
    public:
        virtual ~IPlayGameMode() = default;
        [[nodiscard]] virtual std::string_view Id() const noexcept = 0;
        [[nodiscard]] virtual ModeLoadResult LoadSession(
            const std::filesystem::path& patternPath,
            const std::optional<std::filesystem::path>& effectPath =
                std::nullopt) const = 0;
        [[nodiscard]] virtual ModeLoadResult CreateSession(
            const chart::PatternDocument& pattern,
            const chart::EffectDocument& effects = {}) const = 0;
    };
}
