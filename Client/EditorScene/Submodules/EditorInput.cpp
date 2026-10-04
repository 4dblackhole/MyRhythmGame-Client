#include "EditorView.h"
#include <algorithm>
#include <sstream>
using namespace editor_ui;

void EditorView::EditScore(v::Point point, bool erase)
{
    if (erase)
    {
        const auto nearest = std::ranges::min_element(noteHits, {}, [point](const NoteHit &n) {
            return std::hypot(n.point.x - point.x, n.point.y - point.y);
        });
        if (nearest != noteHits.end() &&
            std::hypot(nearest->point.x - point.x, nearest->point.y - point.y) <
                (state_.realtime ? 55 : 24))
            state_.editor->DeleteNote(nearest->order);
        state_.rebuild = true;
        return;
    }
    chart::MusicalPosition p;
    if (state_.realtime)
    {
        if (point.y < 351 || point.y > 579 || realtimeGrid.empty())
            return;
        p = std::ranges::min_element(realtimeGrid, {}, [point](const auto &g) {
                return std::abs(g.first - point.x);
            })->second;
    }
    else
    {
        const int row = static_cast<int>((point.y - 150) / 100),
                  col = static_cast<int>((point.x - 205) / 401);
        if (point.x < 205 || point.x >= 1809 || point.y < 150 || row >= 6 ||
            point.y - (150 + row * 100) > 48)
            return;
        p.measure = state_.firstMeasure + row * 4 + col;
        const auto length = state_.editor->Timeline().MeasureLength(p.measure);
        const int subdivisionsPerWholeNote = state_.division * 4;
        const auto tick = static_cast<std::int64_t>(
            std::llround((point.x - 205 - col * 401) / 401.0 * length.Value() *
                         subdivisionsPerWholeNote));
        p.fraction = chart::Rational{tick, subdivisionsPerWholeNote};
        if (p.fraction >= length)
            p.fraction = chart::Rational{std::max<std::int64_t>(0, tick - 1),
                                         subdivisionsPerWholeNote};
    }
    state_.PlaceNote(p);
}

void EditorView::UpdateDivisionSlider(const mrg::UpdateContext &context)
{
    if (state_.tab != 0)
        return;

    const auto &input = context.input;
    v::PointerInput pointer{};
    const auto mapped = v::MapScreenPointer(
        {static_cast<float>(input.MousePositionX()), static_cast<float>(input.MousePositionY())},
        {static_cast<float>(width), static_cast<float>(height)}, *canvas.Get());
    pointer.available = input.IsMouseInsideWindow() && mapped.has_value();
    pointer.position = mapped.value_or(v::Point{
        static_cast<float>(input.MousePositionX()) / canvas.Get()->PixelScale() -
            canvas.Get()->LogicalSize().width * .5F,
        canvas.Get()->LogicalSize().height * .5F -
            static_cast<float>(input.MousePositionY()) / canvas.Get()->PixelScale()});
    pointer.leftButtonDown = input.IsMouseButtonDown(mrg::platform::MouseButton::Left);
    pointer.leftButtonPressed = input.WasMouseButtonPressed(mrg::platform::MouseButton::Left);
    pointer.leftButtonReleased = input.WasMouseButtonReleased(mrg::platform::MouseButton::Left);
    divisionSliderInput_.Process(*canvas.Get(), pointer);

    bool changed = false;
    for (const auto &action : canvas.Get()->TakeActions())
        changed |= action.source == divisionSliderNode_->Id() &&
                   action.type == v::ActionType::ValueChanged;
    // A direct-input value above 16 can share the slider's end position.
    if (pointer.leftButtonPressed && divisionSliderInput_.CapturedNode() == divisionSliderNode_->Id())
        changed = true;
    if (changed)
    {
        const float value = divisionSliderNode_->GetComponent<v::SliderBehaviorComponent>()->Value();
        const int division = std::clamp(1 + static_cast<int>(std::lround(value * 15)), 1, 16);
        if (state_.division != division)
        {
            state_.division = division;
            state_.rebuild = true;
        }
        SyncDivisionSlider();
    }
}

bool EditorView::Update(const mrg::UpdateContext &context)
{
    bool leave = false;
    const auto &input = context.input;
    if (textRevision_ != texts_.Revision())
    {
        textRevision_ = texts_.Revision();
        state_.rebuild = true;
    }
    try
    {
        if (input.WasKeyPressed(VK_ESCAPE))
        {
            if (state_.pending || state_.popup >= 0)
            {
                state_.pending.reset();
                state_.popup = -1;
                state_.rebuild = true;
            }
            else if (!state_.editor->Dirty() ||
                     MessageBoxW(GetActiveWindow(), Texts().discardChanges.data(),
                                 Texts().editorTitle.data(), MB_YESNO | MB_ICONQUESTION) == IDYES)
                leave = true;
        }
        const bool controlDown =
            input.IsKeyDown(VK_LCONTROL) || input.IsKeyDown(VK_RCONTROL);
        if (controlDown && input.WasKeyPressed('S'))
            state_.Save();
        if (input.WasKeyPressed(VK_LEFT))
        {
            state_.timeMs -= 1;
            state_.rebuild = true;
        }
        if (input.WasKeyPressed(VK_RIGHT))
        {
            state_.timeMs += 1;
            state_.rebuild = true;
        }
        if (input.MouseWheelDelta() != 0)
        {
            const int delta = input.MouseWheelDelta() > 0 ? -1 : 1;
            if (state_.tab == 0)
                state_.firstMeasure = std::max<std::int64_t>(0, state_.firstMeasure + delta * 4);
            else
                state_.listOffset = static_cast<std::size_t>(std::max<std::int64_t>(
                    0, static_cast<std::int64_t>(state_.listOffset) + delta));
            state_.rebuild = true;
        }
        const float scale = std::max(.001F, std::min(width / 1920.0F, height / 1080.0F));
        const v::Point point{(input.MousePositionX() - width * .5F) / scale + 960,
                             (input.MousePositionY() - height * .5F) / scale + 540};
        UpdateDivisionSlider(context);
        if (input.WasMouseButtonPressed(mrg::platform::MouseButton::Right) && state_.tab == 0)
        {
            if (point.x >= 24 && point.x < 94 && point.y >= 202 && point.y < 522)
            {
                state_.popup = point.y < 282 ? 1 : point.y < 362 ? 2 : 3;
                state_.pending.reset();
                state_.rebuild = true;
            }
            else
                EditScore(point, true);
        }
        if (input.WasMouseButtonPressed(mrg::platform::MouseButton::Left) &&
            divisionSliderInput_.CapturedNode() != divisionSliderNode_->Id())
        {
            auto found = std::find_if(controls.rbegin(), controls.rend(),
                                      [point](const Control &c) { return c.rect.Contains(point); });
            if (found != controls.rend())
            {
                auto action = found->click;
                action();
            }
            else if (state_.tab == 0)
                EditScore(point, false);
        }
    }
    catch (const std::exception &e)
    {
        state_.status = e.what();
        state_.rebuild = true;
    }
    if (state_.rebuild)
        Build();
    return leave;
}
