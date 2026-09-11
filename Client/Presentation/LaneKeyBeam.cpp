#include "Presentation/LaneKeyBeam.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

namespace finger_drum::presentation
{
    namespace
    {
        constexpr double FadeSeconds = 0.2;
        constexpr mrg::visual2d::Color White{1, 1, 1, 1};
        constexpr mrg::visual2d::Color WrongInput{1, 56.0F / 255, 40.0F / 255, 1};
        // RPG/MusicalObject/AccuracyRange.h DefaultAccInfo, including its
        // continuous score/color interpolation and non-interpolated Bad.
        constexpr std::array<mrg::visual2d::Color, 5> Colors{{
            White, {0.175F, 0.875F, 1, 1}, {0.15625F, 1, 0.15625F, 1},
            {1, 0.75F, 0.125F, 1}, {0.25F, 0.09375F, 1, 1}}};
        constexpr std::array<double, 4> Scores{1, 0.99, 0.9, 0};

        mrg::visual2d::Color JudgementColor(const rhythm::JudgementResult& result)
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

    void LaneKeyBeam::Initialize(mrg::visual2d::Visual2DNode& lane,
        const mrg::visual2d::ImageHandle image, const mrg::visual2d::Size imageSize)
    {
        if (node_ != nullptr || imageSize.width <= 0 || imageSize.height <= 0)
            throw std::logic_error("Invalid Lane key beam initialization.");
        aspectRatio_ = imageSize.height / imageSize.width;
        node_ = &mrg::visual2d::CreateSprite(lane, {}, image, "Lane.KeyBeam");
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

    mrg::visual2d::Color LaneKeyBeam::InputColor(
        const rhythm::NoteProcessResult& result) noexcept
    {
        // A transaction may first miss an old note and then accept the next.
        // Use the actual input response, never a passive miss/completion event.
        for (const auto& event : result.events)
        {
            if (event.type == rhythm::NoteEventType::HitAccepted ||
                event.type == rhythm::NoteEventType::TickAccepted ||
                event.type == rhythm::NoteEventType::HoldStarted)
                return JudgementColor(event.judgement);
        }
        for (const auto& event : result.events)
        {
            if (event.type != rhythm::NoteEventType::InputRejected) continue;
            if (event.judgement.grade == rhythm::JudgementGrade::Bad)
                return Colors[4];
            if (rhythm::IsAtLeastAsAccurateAs(
                event.judgement.grade, rhythm::JudgementGrade::Good))
                return WrongInput;
        }
        return White;
    }

    void LaneKeyBeam::OnKeyPressed(const rhythm::NoteProcessResult& result)
    {
        if (node_ == nullptr) return;
        color_ = InputColor(result);
        elapsedSeconds_ = 0;
        node_->GetComponent<mrg::visual2d::SpriteVisualComponent>()->SetTint(color_);
        node_->SetVisible(true);
    }

    void LaneKeyBeam::Update(const double deltaSeconds)
    {
        if (node_ == nullptr || elapsedSeconds_ >= FadeSeconds) return;
        elapsedSeconds_ = std::min(FadeSeconds,
            elapsedSeconds_ + std::max(deltaSeconds, 0.0));
        color_.alpha = static_cast<float>(1.0 - elapsedSeconds_ / FadeSeconds);
        node_->GetComponent<mrg::visual2d::SpriteVisualComponent>()->SetTint(color_);
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
}
