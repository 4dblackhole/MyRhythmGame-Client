#include "GameplayPresenter.h"
#include "GameplaySupport.h"
using namespace gameplay;

void GameplayPresenter::PresentAudioError(const std::string_view message)
{
    if (audioErrorLabel_ == nullptr || message.empty())
    {
        return;
    }
    RequireComponent<mrg::visual2d::TextVisualComponent>(*audioErrorLabel_)
        .SetText(L"AUDIO: " + Utf8ToWide(message));
    audioErrorLabel_->SetVisible(true);
}

void GameplayPresenter::PresentFocusCounter(NoteVisualLayers &layers,
                                            const finger_drum::rhythm::NoteProgress &progress,
                                            const float visualHeight)
{
    if (canvas_ == nullptr || layers.counter == nullptr || layers.counterText == nullptr)
    {
        return;
    }

    const auto bounds = layers.counter->Bounds();
    layers.counter->SetBounds({-bounds.width * 0.5F, visualHeight * 0.5F - bounds.height * 0.28F,
                               bounds.width, bounds.height});
    layers.counter->SetVisible(true);
    const std::size_t remaining =
        progress.required > progress.accepted ? progress.required - progress.accepted : 0;
    RequireComponent<mrg::visual2d::TextVisualComponent>(*layers.counterText)
        .SetText(std::to_wstring(remaining));
}

void GameplayPresenter::PresentFocusNoteProcessing(
    NoteVisualLayers &layers, const finger_drum::rhythm::NoteProgress &progress,
    const finger_drum::rhythm::RhythmTime time)
{
    if (canvas_ == nullptr || layers.processing == nullptr || progress.required == 0)
    {
        return;
    }

    const float completion = std::clamp(
        static_cast<float>(progress.accepted) / static_cast<float>(progress.required), 0.0F, 1.0F);
    const float scale =
        layers.focusType == FocusNoteType::Balloon ? 0.72F + completion * 0.38F : 1.0F;
    const mrg::visual2d::Size size{layers.processingSize.width * scale,
                                   layers.processingSize.height * scale};
    layers.processing->SetSize(size);
    layers.processing->SetPosition({0.0F, 0.0F});
    if (layers.focusType == FocusNoteType::DengDeng)
    {
        const float turns = static_cast<float>(time.count()) / 600'000.0F;
        layers.processing->Transform().SetRotationRollPitchYaw(0.0F, 0.0F,
                                                               std::fmod(turns * TwoPi, TwoPi));
    }
    RequireComponent<mrg::visual2d::SpriteVisualComponent>(*layers.processing)
        .SetTint({1.0F, 1.0F, 1.0F, 1.0F});
    layers.processing->SetVisible(true);
    PresentFocusCounter(layers, progress, size.height);
}

void GameplayPresenter::PresentCompletionEffect(const finger_drum::rhythm::RhythmTime time)
{
    auto selected = completionEffects_.end();
    for (auto effect = completionEffects_.begin(); effect != completionEffects_.end();)
    {
        const auto elapsed = time - effect->second;
        if (elapsed >= FocusSuccessDuration)
        {
            effect = completionEffects_.erase(effect);
            continue;
        }
        if (elapsed >= finger_drum::rhythm::RhythmDuration::zero() &&
            (selected == completionEffects_.end() || effect->second > selected->second))
        {
            selected = effect;
        }
        ++effect;
    }
    if (selected == completionEffects_.end() || canvas_ == nullptr)
    {
        return;
    }

    const auto visual = noteVisuals_.find(selected->first);
    if (visual == noteVisuals_.end())
    {
        return;
    }
    NoteVisualLayers &layers = visual->second;
    presentedNoteIds_.push_back(selected->first);
    const float progress = std::clamp(static_cast<float>((time - selected->second).count()) /
                                          static_cast<float>(FocusSuccessDuration.count()),
                                      0.0F, 1.0F);
    mrg::visual2d::Visual2DNode *node = layers.processing;
    mrg::visual2d::Size size = layers.processingSize;
    if (layers.focusType == FocusNoteType::Balloon && layers.success != nullptr)
    {
        node = layers.success;
        const float burstScale = 0.82F + progress * 0.28F;
        size = {layers.successSize.width * burstScale, layers.successSize.height * burstScale};
    }
    if (node == nullptr)
    {
        return;
    }

    node->SetSize(size);
    node->SetPosition({0.0F, 0.0F});
    if (layers.focusType == FocusNoteType::DengDeng)
    {
        const float turns = static_cast<float>(time.count()) / 600'000.0F;
        node->Transform().SetRotationRollPitchYaw(0.0F, 0.0F, std::fmod(turns * TwoPi, TwoPi));
    }
    RequireComponent<mrg::visual2d::SpriteVisualComponent>(*node).SetTint(
        {1.0F, 1.0F, 1.0F, 1.0F - progress});
    node->SetVisible(true);
}

void GameplayPresenter::UpdateAccuracyPresentation()
{
    const std::optional<double> accuracy = session_->AccuracyRate();
    RequireComponent<mrg::visual2d::TextVisualComponent>(*accuracyIndicator_)
        .SetText(accuracy.has_value() ? std::format(L"{:.2f}%", *accuracy * 100.0) : L"--.--%");
#if defined(_DEBUG)
    std::wstring debug = L"Focus: none";
    if (session_->Gear().LaneCount() > 0)
    {
        if (const auto *note = session_->Gear().Lanes().front()->CurrentNote())
        {
            debug = L"Focus: " + note->DebugText();
        }
    }
    if (const auto &last = session_->LastNoteAccuracy())
    {
        debug += std::format(L"\nLast #{}: {}", last->noteId, last->DebugText());
    }
    else
    {
        debug += L"\nLast: none";
    }
    RequireComponent<mrg::visual2d::TextVisualComponent>(*noteDebugLabel_)
        .SetText(std::move(debug));
#endif
}
