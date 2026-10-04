#include "EditorView.h"
#include <algorithm>
#include <sstream>
using namespace editor_ui;

void EditorView::UpdateSliders(const mrg::UpdateContext &context)
{
    if (state_.Tab() != EditorTab::Pattern && state_.Tab() != EditorTab::Audio)
        return;

    const auto &input = context.input;
    v::PointerInput pointer{};
    const auto mapped =
        v::MapScreenPointer({static_cast<float>(input.MousePositionX()), static_cast<float>(input.MousePositionY())},
                            {static_cast<float>(width), static_cast<float>(height)}, *canvas.Get());
    pointer.available = input.IsMouseInsideWindow() && mapped.has_value();
    pointer.position =
        mapped.value_or(v::Point{static_cast<float>(input.MousePositionX()) / canvas.Get()->PixelScale() -
                                     canvas.Get()->LogicalSize().width * .5F,
                                 canvas.Get()->LogicalSize().height * .5F -
                                     static_cast<float>(input.MousePositionY()) / canvas.Get()->PixelScale()});
    pointer.leftButtonDown = input.IsMouseButtonDown(mrg::platform::MouseButton::Left);
    pointer.leftButtonPressed = input.WasMouseButtonPressed(mrg::platform::MouseButton::Left);
    pointer.leftButtonReleased = input.WasMouseButtonReleased(mrg::platform::MouseButton::Left);
    sliderInput_.Process(*canvas.Get(), pointer);

    bool changed = false;
    bool timeChanged = false;
    for (const auto &action : canvas.Get()->TakeActions())
    {
        changed |= action.source == divisionSliderNode_->Id() && action.type == v::ActionType::ValueChanged;
        timeChanged |= action.source == timelineSliderNode_->Id() && action.type == v::ActionType::ValueChanged;
    }
    // A direct-input value above 16 can share the slider's end position.
    if (pointer.leftButtonPressed && sliderInput_.CapturedNode() == divisionSliderNode_->Id())
        changed = true;
    if (pointer.leftButtonPressed && sliderInput_.CapturedNode() == timelineSliderNode_->Id())
        timeChanged = true;
    if (changed)
    {
        const float value = divisionSliderNode_->GetComponent<v::SliderBehaviorComponent>()->Value();
        const int division = std::clamp(1 + static_cast<int>(std::lround(value * 15)), 1, 16);
        if (state_.Score().division != division)
        {
            state_.SetDivision(division);
        }
        SyncDivisionSlider();
    }
    if (timeChanged)
    {
        const auto [begin, end] = state_.TimelineRangeMilliseconds();
        const float value = timelineSliderNode_->GetComponent<v::SliderBehaviorComponent>()->Value();
        state_.Seek(std::round(std::lerp(begin, end, static_cast<double>(value))));
    }
}

bool EditorView::Update(const mrg::UpdateContext &context)
{
    bool leave = false;
    const auto &input = context.input;
    if (textRevision_ != texts_.Revision())
    {
        textRevision_ = texts_.Revision();
        state_.RequestRebuild();
    }
    try
    {
        leave = UpdateKeyboard(input);
        UpdatePointer(context);
    }
    catch (const std::exception &e)
    {
        state_.SetStatus(e.what());
    }
    if (state_.NeedsRebuild())
        Build();
    return leave;
}

bool EditorView::UpdateKeyboard(const mrg::platform::InputState &input)
{
    bool leave = false;
    if (input.WasKeyPressed(VK_ESCAPE))
    {
        if (state_.Mode().CancelInteraction())
        {
            state_.RequestRebuild();
        }
        else if (!state_.Document().Dirty() ||
                 MessageBoxW(GetActiveWindow(), Texts().discardChanges.data(), Texts().editorTitle.data(),
                             MB_YESNO | MB_ICONQUESTION) == IDYES)
            leave = true;
    }
    const bool controlDown = input.IsKeyDown(VK_LCONTROL) || input.IsKeyDown(VK_RCONTROL);
    if (controlDown && input.WasKeyPressed('S'))
        state_.Save();
    if (input.WasKeyPressed(VK_LEFT))
    {
        state_.Seek(state_.TimeMilliseconds() - 1);
    }
    if (input.WasKeyPressed(VK_RIGHT))
    {
        state_.Seek(state_.TimeMilliseconds() + 1);
    }

    return leave;
}

void EditorView::UpdatePointer(const mrg::UpdateContext &context)
{
    const auto &input = context.input;
    if (input.MouseWheelDelta() != 0)
    {
        const int delta = input.MouseWheelDelta() > 0 ? -1 : 1;
        if (state_.Tab() == EditorTab::Pattern)
            state_.Mode().ScrollScore(state_, delta);
        else if (state_.Tab() != EditorTab::Audio)
            state_.listOffset = static_cast<std::size_t>(
                std::max<std::int64_t>(0, static_cast<std::int64_t>(state_.listOffset) + delta));
        state_.RequestRebuild();
    }
    const float scale = std::max(.001F, std::min(width / 1920.0F, height / 1080.0F));
    const v::Point point{(input.MousePositionX() - width * .5F) / scale + 960,
                         (input.MousePositionY() - height * .5F) / scale + 540};
    UpdateSliders(context);
    if (input.WasMouseButtonPressed(mrg::platform::MouseButton::Right) && state_.Tab() == EditorTab::Pattern &&
        !buildFailed_)
    {
        if (state_.Mode().OpenToolMenu(point))
        {
            state_.RequestRebuild();
        }
        else
            state_.Mode().EditScore(state_, point, true);
    }
    if (input.WasMouseButtonPressed(mrg::platform::MouseButton::Left) &&
        sliderInput_.CapturedNode() != divisionSliderNode_->Id() &&
        sliderInput_.CapturedNode() != timelineSliderNode_->Id())
    {
        auto found = std::find_if(controls.rbegin(), controls.rend(),
                                  [point](const Control &c) { return c.rect.Contains(point); });
        if (found != controls.rend())
        {
            auto action = found->click;
            action();
        }
        else if (state_.Tab() == EditorTab::Pattern && !buildFailed_)
            state_.Mode().EditScore(state_, point, false);
    }
}
