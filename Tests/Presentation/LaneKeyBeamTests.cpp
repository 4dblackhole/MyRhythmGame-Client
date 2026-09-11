#include "Presentation/LaneKeyBeam.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace finger_drum;

void Check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

bool Near(float left, float right)
{
    return std::abs(left - right) < 0.0001F;
}

int main()
{
    try
    {
        using rhythm::JudgementGrade;
        using rhythm::NoteEventType;
        presentation::LaneKeyBeam beam;
        mrg::visual2d::Visual2DCanvas canvas;
        auto& lane = canvas.CreateNode(mrg::visual2d::Anchor::Center, "Lane");
        lane.SetSize({152, 1000});
        beam.Initialize(lane, {}, {50, 250});
        auto& sprite = *lane.Children().front();
        auto tint = [&] { return sprite.GetComponent<
            mrg::visual2d::SpriteVisualComponent>()->Style().normal; };
        Check(sprite.Transform().Parent() == &lane.Transform(), "Lane transform parent");
        Check(!sprite.IsVisible(), "Initially hidden");
        beam.OnKeyPressed({});
        Check(sprite.IsVisible() && Near(tint().alpha, 1) && Near(tint().red, 1), "Empty input white");
        beam.Update(0.1);
        Check(Near(tint().alpha, 0.5F), "100ms half alpha");
        beam.OnKeyPressed({});
        Check(Near(tint().alpha, 1), "Repeated hit restarts fade");
        beam.Update(0.2);
        Check(!sprite.IsVisible() && Near(tint().alpha, 0), "200ms hidden");

        rhythm::JudgementProfile profile;
        const auto makeResult = [&](double milliseconds, bool accepted) {
            rhythm::NoteProcessResult result;
            rhythm::NoteEvent event;
            event.type = accepted ? NoteEventType::HitAccepted : NoteEventType::InputRejected;
            event.judgement = profile.Evaluate({}, rhythm::RhythmTime{
                static_cast<rhythm::RhythmTime::rep>(milliseconds * 1000)});
            result.events.push_back(event);
            return result;
        };
        beam.OnKeyPressed(makeResult(9.5, true));
        Check(Near(tint().red, 0.175F) && Near(tint().blue, 1), "Perfect cyan");
        beam.OnKeyPressed(makeResult(23.5, true));
        Check(Near(tint().green, 1) && Near(tint().red, 0.15625F), "Great green");
        beam.OnKeyPressed(makeResult(54.5, true));
        Check(Near(tint().green, 0.75F) && Near(tint().blue, 0.125F), "Good yellow");
        beam.OnKeyPressed(makeResult(7, true));
        Check(Near(tint().red, (1 + 0.175F) / 2), "Interpolated accuracy color");
        beam.OnKeyPressed(makeResult(70, false));
        Check(Near(tint().red, 0.25F) && Near(tint().blue, 1), "Early Bad purple");
        beam.OnKeyPressed(makeResult(0, false));
        Check(Near(tint().red, 1) && Near(tint().green, 56.0F / 255), "Wrong input red");
        beam.OnKeyPressed(makeResult(500, false));
        Check(Near(tint().red, 1) && Near(tint().green, 1), "Out of range white");
        auto rollover = makeResult(0, true);
        rhythm::NoteEvent missed;
        missed.type = NoteEventType::Missed;
        rollover.events.insert(rollover.events.begin(), missed);
        beam.OnKeyPressed(rollover);
        Check(Near(tint().green, 1), "Passive miss does not override accepted hit");
        rollover.events.back().type = NoteEventType::TickAccepted;
        rollover.events.back().judgement.grade = JudgementGrade::Unjudged;
        beam.OnKeyPressed(rollover);
        Check(Near(tint().green, 1), "Roll tick white");

        lane.Transform().SetRotationRollPitchYaw(0, 0, -DirectX::XM_PIDIV2);
        lane.Transform().SetScale(2, 2, 1);
        beam.SetLayout(152, 300);
        Check(Near(sprite.Bounds().y, 0) && Near(sprite.Bounds().height, 760), "Image aspect and lane origin");
        const auto world = DirectX::XMLoadFloat4x4(&sprite.Transform().WorldMatrix());
        const auto origin = DirectX::XMVector3TransformCoord(DirectX::XMVectorZero(), world);
        const auto forward = DirectX::XMVector3TransformCoord(
            DirectX::XMVectorSet(0, 1, 0, 1), world);
        Check(Near(DirectX::XMVectorGetX(DirectX::XMVectorSubtract(forward, origin)), 2), "Lane rotation and scale inherited");
        const auto draws = canvas.BuildDrawList();
        Check(draws.size() == 1 && draws.front().clipBounds.has_value(), "Beam draw packet has clip");
        Check(Near(draws.front().clipBounds->width, 600), "Rotated clip ends at resized lane boundary");
        Check(lane.Children().size() == 1, "Repeated inputs do not allocate sprites");
        beam.Reset();
        Check(!sprite.IsVisible(), "Reset clears beam");
        Check(canvas.BuildDrawList().empty(), "Hidden beam is not submitted");
        beam.Shutdown();
        beam.Update(1);
        std::cout << "Lane key beam tests passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
