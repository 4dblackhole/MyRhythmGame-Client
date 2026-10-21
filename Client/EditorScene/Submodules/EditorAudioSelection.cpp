#include "EditorTime.h"
#include "EditorView.h"
#include <algorithm>
#include <string>

using namespace editor_ui;

void EditorView::InitializeAudioSelection()
{
    const std::array tracks{Waveform, MusicSpectrum, HitSpectrum};
    for (std::size_t index = 0; index < tracks.size(); ++index)
    {
        const auto rect = tracks[index];
        auto &track = node->CreateChild("Audio.Selection." + std::to_string(index));
        track.SetBounds({rect.x - 960, 540 - rect.y - rect.height, rect.width, rect.height});
        track.SetZIndex(2);
        track.AddComponent<RectangleCollider2DComponent>();
        track.AddComponent<PointerReceiverComponent>(
            [this](Visual2DNode &target, const PointerEvent &event, std::vector<Action> &)
            { UpdateAudioSelection(target, event); });
        audioSelectionNodes_[index] = &track;
    }
}

void EditorView::UpdateAudioSelection(Visual2DNode &target, const PointerEvent &event)
{
    if (state_.Tab() != EditorTab::Audio || buildFailed_)
        return;
    const bool press = event.type == PointerEventType::Press && event.button == PointerButton::Left;
    const bool release = event.type == PointerEventType::Release && event.button == PointerButton::Left;
    if (!press && !release && (event.type != PointerEventType::Move || !target.IsPressed()))
        return;
    if (press)
    {
        audioSelectionAnchor_.reset();
        // Keep the coordinate mapping stable until release, even during playback.
        audioSelectionViewBegin_ = state_.TimeMilliseconds() / 1000.0 - state_.audioWindow * .25;
        audioSelectionViewWindow_ = state_.audioWindow;
    }
    const double fraction = std::clamp(static_cast<double>(event.localPosition.x) / Waveform.width, 0.0, 1.0);
    const auto time =
        Microseconds((audioSelectionViewBegin_ + audioSelectionViewWindow_ * fraction) * 1000.0);
    if (press)
        audioSelectionAnchor_ = time;
    if (audioSelectionAnchor_)
        state_.SelectAudioRange(*audioSelectionAnchor_, time);
    if (release)
        audioSelectionAnchor_.reset();
}

void EditorView::DrawAudioSelection(const double begin)
{
    const auto &selection = state_.SelectedAudioRange();
    if (!selection)
    {
        Text({205, 321, 1565, 19}, std::wstring(texts_.Get("Editor.audioSelectionHelp")), 15);
        return;
    }
    const double firstMs = selection->begin.count() / 1000.0;
    const double lastMs = selection->end.count() / 1000.0;
    Text({205, 321, 1565, 19}, texts_.Format("Editor.audioSelectionFormat", firstMs, lastMs, lastMs - firstMs), 15);
    const double first = (firstMs / 1000.0 - begin) / state_.audioWindow;
    const double last = (lastMs / 1000.0 - begin) / state_.audioWindow;
    if (last < 0 || first > 1)
        return;
    const float left = static_cast<float>(std::clamp(first, 0.0, 1.0)) * Waveform.width;
    const float right = static_cast<float>(std::clamp(last, 0.0, 1.0)) * Waveform.width;
    const Color fill{Blue.red, Blue.green, Blue.blue, .24F};
    Box({Waveform.x + left, Waveform.y, right - left, Waveform.height}, fill);
    Box({Waveform.x + std::min(left, Waveform.width - 1), Waveform.y, 1, Waveform.height}, Blue);
    Box({std::min(Waveform.x + right, Waveform.x + Waveform.width - 1), Waveform.y, 1, Waveform.height}, Blue);
    for (auto *overlay : audioOverlays_)
    {
        overlay->AddRectangle({left, 0, right - left, MusicSpectrum.height}, fill, 0);
        overlay->AddRectangle({std::min(left, Waveform.width - 1), 0, 1, MusicSpectrum.height}, Blue, 0);
        overlay->AddRectangle({std::min(right, Waveform.width - 1), 0, 1, MusicSpectrum.height}, Blue, 0);
    }
}
