#include "Presentation/LaneKeyBeam.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

namespace
{
    constexpr double FadeSeconds = 0.2;
    constexpr Color White{1, 1, 1, 1};
    constexpr Color WrongInput{1, 56.0F / 255, 40.0F / 255, 1};
    // RPG/MusicalObject/AccuracyRange.h DefaultAccInfo, including its
    // continuous score/color interpolation and non-interpolated Bad.
    constexpr std::array<Color, 5> Colors{{
        White, {0.175F, 0.875F, 1, 1}, {0.15625F, 1, 0.15625F, 1},
        {1, 0.75F, 0.125F, 1}, {0.25F, 0.09375F, 1, 1}}};
    constexpr std::array<double, 4> Scores{1, 0.99, 0.9, 0};

    Color JudgementColor(const JudgementResult& result)
    {
        const auto index = static_cast<std::size_t>(result.grade);
        if (index >= Colors.size()) return White;
        if (index == 0 || index == 4) return Colors[index];
        const float amount = static_cast<float>(std::clamp(
            (Scores[index - 1] - result.scoreRate) /
                (Scores[index - 1] - Scores[index]), 0.0, 1.0));
        const auto& from = Colors[index - 1];
        const auto& to = Colors[index];
        return {std::lerp(from.red, to.red, amount),
            std::lerp(from.green, to.green, amount),
            std::lerp(from.blue, to.blue, amount), 1};
    }
}

void LaneKeyBeam::Initialize(Visual2DNode& lane,
    const ImageHandle image, const Size imageSize)
{
    if (node_ != nullptr || imageSize.width <= 0 || imageSize.height <= 0)
        throw std::logic_error("Invalid Lane key beam initialization.");
    aspectRatio_ = imageSize.height / imageSize.width;
    node_ = &CreateSprite(lane, {}, image, "Lane.KeyBeam");
    node_->SetZIndex(2); // Above the surface, below notes and judgement ring.
    SetLayout(lane.NodeSize().width, lane.NodeSize().height);
    Reset();
}

void LaneKeyBeam::SetLayout(const float laneWidth, const float laneLength)
{
    if (node_ == nullptr) return;
    const float length = laneWidth * aspectRatio_;
    node_->SetBounds({0, 0, laneWidth, length});
    node_->SetClipRect({0, 0, laneWidth, std::min(length, laneLength)});
}

Color LaneKeyBeam::GradeColor(const JudgementGrade grade) noexcept
{
    const auto index = static_cast<std::size_t>(grade);
    return index < Colors.size() ? Colors[index] : White;
}

bool LaneKeyBeam::IsWrongInput(const NoteEvent &event) noexcept
{
    return event.type == NoteEventType::InputRejected &&
        IsAtLeastAsAccurateAs(event.judgement.grade, JudgementGrade::Good);
}

const NoteEvent *LaneKeyBeam::InputResponse(
    const NoteProcessResult& result) noexcept
{
    // A transaction may first miss an old note and then accept the next.
    // Use the actual input response, never a passive miss/completion event.
    for (const auto& event : result.events)
    {
        if (event.type == NoteEventType::HitAccepted ||
            event.type == NoteEventType::TickAccepted ||
            event.type == NoteEventType::HoldStarted)
            return &event;
    }
    for (const auto& event : result.events)
    {
        if (event.type != NoteEventType::InputRejected) continue;
        if (event.judgement.grade == JudgementGrade::Bad || IsWrongInput(event))
            return &event;
    }
    return nullptr;
}

Color LaneKeyBeam::InputColor(
    const NoteProcessResult& result) noexcept
{
    if (const auto *event = InputResponse(result))
        return IsWrongInput(*event) ? WrongInput : JudgementColor(event->judgement);
    return White;
}

void LaneKeyBeam::OnKeyPressed(const NoteProcessResult& result)
{
    if (node_ == nullptr) return;
    color_ = InputColor(result);
    elapsedSeconds_ = 0;
    node_->GetComponent<SpriteVisualComponent>()->SetTint(color_);
    node_->SetVisible(true);
}

void LaneKeyBeam::Update(const double deltaSeconds)
{
    if (node_ == nullptr || elapsedSeconds_ >= FadeSeconds) return;
    elapsedSeconds_ = std::min(FadeSeconds,
        elapsedSeconds_ + std::max(deltaSeconds, 0.0));
    color_.alpha = static_cast<float>(1.0 - elapsedSeconds_ / FadeSeconds);
    node_->GetComponent<SpriteVisualComponent>()->SetTint(color_);
    node_->SetVisible(elapsedSeconds_ < FadeSeconds);
}

void LaneKeyBeam::Reset() noexcept
{
    elapsedSeconds_ = FadeSeconds;
    if (node_ != nullptr) node_->SetVisible(false);
}

void LaneKeyBeam::Shutdown() noexcept
{
    Reset();
    node_ = nullptr;
}
