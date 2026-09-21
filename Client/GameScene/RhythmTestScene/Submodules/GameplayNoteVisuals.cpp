#include "GameplayPresenter.h"
#include "GameplaySupport.h"
using namespace gameplay;

void GameplayPresenter::CreateMeasureLineVisuals()
{
    for (const finger_drum::rhythm::RhythmTime timing : session_->MeasureLines())
    {
        auto &line = mrg::visual2d::CreatePanel(*laneRoot_, {0.0F, 0.0F, laneWidth_, 4.0F},
                                                "Lane.MeasureLine");
        SetColor(line, {0.78F, 0.85F, 0.90F, 0.72F});
        line.SetZIndex(1);
        line.SetVisible(false);
        measureLineVisuals_.push_back({timing, &line});
    }
}

void GameplayPresenter::CreateNoteVisuals()
{
    const auto noteImage = screenVisuals_.RegisterImage(InGameSkinAssetPath(L"note.png"));
    const auto noteOverlay = screenVisuals_.RegisterImage(InGameSkinAssetPath(L"noteoverlay.png"));
    const auto bigNoteImage = screenVisuals_.RegisterImage(InGameSkinAssetPath(L"bignote.png"));
    const auto bigOverlay =
        screenVisuals_.RegisterImage(InGameSkinAssetPath(L"bigcircleoverlay.png"));
    const auto bodyImage = screenVisuals_.RegisterImage(InGameSkinAssetPath(L"LNBody.png"));
    const auto bodyOverlayImage =
        screenVisuals_.RegisterImage(InGameSkinAssetPath(L"LNBodyOverlay.png"));
    const auto tailImage = screenVisuals_.RegisterImage(InGameSkinAssetPath(L"LNTail.png"));
    const auto tailOverlayImage =
        screenVisuals_.RegisterImage(InGameSkinAssetPath(L"LNTailOverlay.png"));
    const auto bigBodyImage = screenVisuals_.RegisterImage(InGameSkinAssetPath(L"BigLNBody.png"));
    const auto bigBodyOverlayImage =
        screenVisuals_.RegisterImage(InGameSkinAssetPath(L"BigLNBodyOverlay.png"));
    const auto bigTailImage = screenVisuals_.RegisterImage(InGameSkinAssetPath(L"BigLNTail.png"));
    const auto bigTailOverlayImage =
        screenVisuals_.RegisterImage(InGameSkinAssetPath(L"BigLNTailOverlay.png"));
    const auto tickImage = screenVisuals_.RegisterImage(InGameSkinAssetPath(L"TickMarker.png"));
    const auto buzzTickImage =
        screenVisuals_.RegisterImage(InGameSkinAssetPath(L"BuzzTickDiamond.png"));
    const auto balloonImage = screenVisuals_.RegisterImage(InGameSkinAssetPath(L"Balloon.png"));
    const auto dengDengImage = screenVisuals_.RegisterImage(InGameSkinAssetPath(L"DengDeng.png"));
    const auto balloonProcessingImage =
        screenVisuals_.RegisterImage(InGameSkinAssetPath(L"BalloonProcessing.png"));
    const auto balloonBurstImage =
        screenVisuals_.RegisterImage(InGameSkinAssetPath(L"BalloonBurst.png"));
    const auto dengDengProcessingImage =
        screenVisuals_.RegisterImage(InGameSkinAssetPath(L"DengDengProcessing.png"));
    const auto counterImage =
        screenVisuals_.RegisterImage(InGameSkinAssetPath(L"HitCounterCloud.png"));

    const auto noteSize = ScaledImageSize(screenVisuals_, noteImage);
    const auto bigNoteSize = ScaledImageSize(screenVisuals_, bigNoteImage);
    const auto bodySize = ScaledImageSize(screenVisuals_, bodyImage);
    const auto tailSize = ScaledImageSize(screenVisuals_, tailImage);
    const auto bigBodySize = ScaledImageSize(screenVisuals_, bigBodyImage);
    const auto bigTailSize = ScaledImageSize(screenVisuals_, bigTailImage);
    const auto tickSize = ScaledImageSize(screenVisuals_, tickImage);
    const auto buzzTickSize = ScaledImageSize(screenVisuals_, buzzTickImage);
    const auto balloonSize = ScaledImageSize(screenVisuals_, balloonImage);
    const auto dengDengSize = ScaledImageSize(screenVisuals_, dengDengImage);
    const auto balloonProcessingSize = ScaledImageSize(screenVisuals_, balloonProcessingImage);
    const auto balloonBurstSize = ScaledImageSize(screenVisuals_, balloonBurstImage);
    const auto dengDengProcessingSize = ScaledImageSize(screenVisuals_, dengDengProcessingImage);
    const auto counterSize = ScaledImageSize(screenVisuals_, counterImage);

    if (laneRoot_ == nullptr)
    {
        throw std::logic_error("Note visuals require a Lane root.");
    }
    for (const std::unique_ptr<finger_drum::rhythm::Lane> &lane : session_->Gear().Lanes())
    {
        for (const std::unique_ptr<finger_drum::rhythm::INote> &note : lane->Notes())
        {
            const finger_drum::mode::NotePresentationInfo *presentation =
                session_->FindNotePresentation(note->Id());
            const finger_drum::mode::NoteVisualKind visualKind =
                presentation == nullptr ? finger_drum::mode::NoteVisualKind::Don
                                        : presentation->visualKind;
            const bool big = IsBigVisual(visualKind);
            const bool balloon = visualKind == finger_drum::mode::NoteVisualKind::Balloon;
            const bool dengDeng = visualKind == finger_drum::mode::NoteVisualKind::DengDeng;
            const bool customHead = balloon || dengDeng;
            const bool longNote = presentation != nullptr && presentation->hasEndTime &&
                                  IsLongVisual(visualKind) && !customHead;
            const bool bigLongParts = visualKind == finger_drum::mode::NoteVisualKind::BigRoll;
            const bool buzz = visualKind == finger_drum::mode::NoteVisualKind::DonBuzz ||
                              visualKind == finger_drum::mode::NoteVisualKind::KatBuzz;
            const auto headImage =
                balloon ? balloonImage
                        : (dengDeng ? dengDengImage : (big ? bigNoteImage : noteImage));
            const auto headSize =
                balloon ? balloonSize : (dengDeng ? dengDengSize : (big ? bigNoteSize : noteSize));
            const float diameter = headSize.width;
            const mrg::visual2d::Color ambientColor = AmbientColor(visualKind);

            NoteVisualLayers layers;
            layers.focusType = balloon ? FocusNoteType::Balloon
                                       : (dengDeng ? FocusNoteType::DengDeng : FocusNoteType::None);
            layers.root = &laneRoot_->CreateChild(std::format("Lane.Note.{}", note->Id()));
            layers.root->SetPivot({0.5F, 0.5F});
            layers.root->SetSize({diameter, diameter});
            layers.root->SetPosition({laneWidth_ * 0.5F, judgementLocalY_});
            layers.root->SetZIndex(3);
            layers.root->SetVisible(false);
            layers.diameter = diameter;

            // Body and tail are Ambient-colored and sit behind the circular
            // head. The white head overlay is a separate untinted draw packet.
            if (longNote)
            {
                const auto selectedBodyImage = bigLongParts ? bigBodyImage : bodyImage;
                const auto selectedBodyOverlayImage =
                    bigLongParts ? bigBodyOverlayImage : bodyOverlayImage;
                const auto selectedTailImage = bigLongParts ? bigTailImage : tailImage;
                const auto selectedTailOverlayImage =
                    bigLongParts ? bigTailOverlayImage : tailOverlayImage;
                const auto selectedBodySize = bigLongParts ? bigBodySize : bodySize;
                const auto selectedTailSize = bigLongParts ? bigTailSize : tailSize;
                layers.bodyWidth = selectedBodySize.width;
                layers.tailWidth = selectedTailSize.width;
                layers.tailHeight = selectedTailSize.height;
                const float bodyX = (diameter - layers.bodyWidth) * 0.5F;
                const float tailX = (diameter - layers.tailWidth) * 0.5F;
                layers.body = &mrg::visual2d::CreateSprite(
                    *layers.root, {bodyX, diameter * 0.5F, layers.bodyWidth, 1.0F},
                    selectedBodyImage, "AmbientBody");
                RequireComponent<mrg::visual2d::SpriteVisualComponent>(*layers.body)
                    .SetTint(ambientColor);
                layers.body->SetZIndex(0);
                layers.bodyOverlay = &mrg::visual2d::CreateSprite(
                    *layers.root, {bodyX, diameter * 0.5F, layers.bodyWidth, 1.0F},
                    selectedBodyOverlayImage, "BodyOverlay");
                layers.bodyOverlay->SetZIndex(1);
                layers.tail = &mrg::visual2d::CreateSprite(
                    *layers.root, {tailX, 0.0F, layers.tailWidth, layers.tailHeight},
                    selectedTailImage, "AmbientTail");
                RequireComponent<mrg::visual2d::SpriteVisualComponent>(*layers.tail)
                    .SetTint(ambientColor);
                layers.tail->SetZIndex(2);
                layers.tail->SetPivot({0.5F, 0.5F});
                layers.tail->SetPosition(
                    {tailX + layers.tailWidth * 0.5F, layers.tailHeight * 0.5F});
                layers.tailOverlay = &mrg::visual2d::CreateSprite(
                    *layers.root, {tailX, 0.0F, layers.tailWidth, layers.tailHeight},
                    selectedTailOverlayImage, "TailOverlay");
                layers.tailOverlay->SetZIndex(3);
                layers.tailOverlay->SetPivot({0.5F, 0.5F});
                layers.tailOverlay->SetPosition(
                    {tailX + layers.tailWidth * 0.5F, layers.tailHeight * 0.5F});

                if (presentation != nullptr)
                {
                    const auto selectedTickImage = buzz ? buzzTickImage : tickImage;
                    const auto selectedTickSize = buzz ? buzzTickSize : tickSize;
                    for (const auto tickTime : presentation->tickTimes)
                    {
                        if (tickTime <= note->Timing())
                        {
                            continue;
                        }
                        const float offset =
                            static_cast<float>((tickTime - note->Timing()).count()) /
                            static_cast<float>(ApproachDuration.count()) * TravelDistance *
                            static_cast<float>(presentation->scrollMultiplier);
                        auto &tick = mrg::visual2d::CreateSprite(
                            *layers.root,
                            {(diameter - selectedTickSize.width) * 0.5F,
                             diameter * 0.5F + offset - selectedTickSize.height * 0.5F,
                             selectedTickSize.width, selectedTickSize.height},
                            selectedTickImage, "LongNote.Tick");
                        tick.SetZIndex(4);
                        layers.ticks.push_back({tickTime, &tick});
                    }
                }
            }

            layers.ambient = &mrg::visual2d::CreateSprite(
                *layers.root, {0.0F, 0.0F, headSize.width, headSize.height}, headImage,
                "AmbientHead");
            RequireComponent<mrg::visual2d::SpriteVisualComponent>(*layers.ambient)
                .SetTint(customHead ? mrg::visual2d::Color{1.0F, 1.0F, 1.0F, 1.0F} : ambientColor);
            layers.ambient->SetZIndex(3);
            if (!customHead)
            {
                layers.overlay = &mrg::visual2d::CreateSprite(
                    *layers.root, {0.0F, 0.0F, headSize.width, headSize.height},
                    big ? bigOverlay : noteOverlay, "UntintedOverlay");
                RequireComponent<mrg::visual2d::SpriteVisualComponent>(*layers.overlay)
                    .SetTint({1.0F, 1.0F, 1.0F, 1.0F});
                layers.overlay->SetZIndex(4);
            }
            if (customHead)
            {
                const auto processingImage =
                    balloon ? balloonProcessingImage : dengDengProcessingImage;
                layers.processingSize = balloon ? balloonProcessingSize : dengDengProcessingSize;
                layers.processing = &mrg::visual2d::CreateSprite(
                    canvas_->Root(),
                    {0.0F, 0.0F, layers.processingSize.width, layers.processingSize.height},
                    processingImage, std::format("Hud.NoteProcessing.{}", note->Id()));
                layers.processing->SetPivot({0.5F, 0.5F});
                layers.processing->SetZIndex(7);
                layers.processing->SetVisible(false);
                if (balloon)
                {
                    layers.successSize = balloonBurstSize;
                    layers.success = &mrg::visual2d::CreateSprite(
                        canvas_->Root(),
                        {0.0F, 0.0F, balloonBurstSize.width, balloonBurstSize.height},
                        balloonBurstImage, std::format("Hud.NoteSuccess.{}", note->Id()));
                    layers.success->SetPivot({0.5F, 0.5F});
                    layers.success->SetZIndex(9);
                    layers.success->SetVisible(false);
                }
                layers.counter = &mrg::visual2d::CreateSprite(
                    canvas_->Root(), {0.0F, 0.0F, counterSize.width, counterSize.height},
                    counterImage, std::format("Hud.NoteCounter.{}", note->Id()));
                layers.counter->SetZIndex(8);
                layers.counter->SetVisible(false);
                layers.counterText =
                    &mrg::visual2d::CreateLabel(*layers.counter,
                                                {0.0F, counterSize.height * 0.22F,
                                                 counterSize.width, counterSize.height * 0.58F},
                                                L"", "RemainingHits");
                auto &text =
                    RequireComponent<mrg::visual2d::TextVisualComponent>(*layers.counterText);
                text.SetFontSize(22.0F);
                text.SetTextColor({1.0F, 1.0F, 1.0F, 1.0F});
                text.SetHorizontalAlignment(mrg::visual2d::TextAlignment::Center);
            }
            noteVisuals_.emplace(note->Id(), layers);
        }
    }
}

void GameplayPresenter::HideTransientNoteVisuals()
{
    for (const finger_drum::rhythm::NoteId id : presentedNoteIds_)
    {
        const auto visual = noteVisuals_.find(id);
        if (visual == noteVisuals_.end())
        {
            continue;
        }
        NoteVisualLayers &layers = visual->second;
        layers.root->SetVisible(false);
        if (layers.processing != nullptr)
        {
            layers.processing->SetVisible(false);
        }
        if (layers.success != nullptr)
        {
            layers.success->SetVisible(false);
        }
        if (layers.counter != nullptr)
        {
            layers.counter->SetVisible(false);
        }
    }
    presentedNoteIds_.clear();
}

void GameplayPresenter::UpdatePresentation(const finger_drum::rhythm::RhythmTime time)
{
    UpdateAccuracyPresentation();
    using Duration = finger_drum::rhythm::RhythmDuration;
    // Bound conversion and time + horizon even for extremely small speeds.
    const auto maximumHorizon = std::numeric_limits<Duration::rep>::max() / 2;
    const auto horizon = std::min(static_cast<double>(maximumHorizon),
                                  ApproachDuration.count() / session_->MinimumScrollMultiplier());
    const auto snapshot = session_->Gear().BuildSnapshot(
        time, Duration{static_cast<Duration::rep>(horizon)}, MissedTravelDuration);
    HideTransientNoteVisuals();
    bool showMeasureLines = true;
    for (const auto &value : session_->EvaluateAutomation(time))
        if (value.type == finger_drum::chart::EffectCommandType::MeasureLineVisible)
            showMeasureLines = value.value >= 0.5;
    for (TimedVisual &measureLine : measureLineVisuals_)
    {
        const auto delta = measureLine.timing - time;
        const bool visible =
            showMeasureLines && delta <= ApproachDuration && delta >= -MissedTravelDuration;
        measureLine.node->SetVisible(visible);
        if (visible)
        {
            const float localY = judgementLocalY_ +
                                 static_cast<float>(delta.count()) /
                                     static_cast<float>(ApproachDuration.count()) * TravelDistance;
            measureLine.node->SetBounds({0.0F, localY, laneWidth_, 4.0F});
        }
    }
    bool focusClaimed{};
    for (const finger_drum::rhythm::ScrollNoteSnapshot &note : snapshot.notes)
    {
        const auto found = noteVisuals_.find(note.noteId);
        if (found == noteVisuals_.end())
        {
            continue;
        }
        NoteVisualLayers &layers = found->second;
        presentedNoteIds_.push_back(note.noteId);
        const finger_drum::mode::NotePresentationInfo *presentation =
            session_->FindNotePresentation(note.noteId);
        const bool focusNote = layers.focusType != FocusNoteType::None;
        float normalizedTravel = static_cast<float>(note.timeFromJudgement.count()) /
                                 static_cast<float>(ApproachDuration.count());
        const bool missed = note.state == NoteState::Missed;
        const bool completed = note.state == NoteState::Completed;
        const bool processing = focusNote && !missed && !completed && time >= note.timing &&
                                note.progress.has_value() && note.progress->accepted > 0;
        if (focusNote && time >= note.timing && !missed)
        {
            normalizedTravel = 0.0F;
        }
        else if (focusNote && missed)
        {
            normalizedTravel = static_cast<float>((note.expireTime - time).count()) /
                               static_cast<float>(ApproachDuration.count());
        }
        const float speed =
            presentation != nullptr ? static_cast<float>(presentation->scrollMultiplier) : 1.0F;
        const float localY = judgementLocalY_ + normalizedTravel * TravelDistance * speed;
        layers.root->SetPosition({laneWidth_ * 0.5F, localY});
        const bool showLaneHead =
            focusNote ? !completed && (!processing || missed) : !completed && !missed;
        layers.root->SetVisible(showLaneHead);

        if (processing && !focusClaimed)
        {
            PresentFocusNoteProcessing(layers, *note.progress, time);
            focusClaimed = true;
        }

        if (presentation != nullptr && presentation->hasEndTime && layers.body != nullptr &&
            layers.tail != nullptr)
        {
            const auto endDelta = presentation->endTime - time;
            const float endTravel =
                std::clamp(static_cast<float>(endDelta.count()) /
                               static_cast<float>(ApproachDuration.count()) * speed,
                           -0.05F, 1.65F);
            const float tailLocalY = judgementLocalY_ + endTravel * TravelDistance;
            const float length = std::max(tailLocalY - localY, 1.0F);
            const float bodyX = (layers.diameter - layers.bodyWidth) * 0.5F;
            layers.body->SetBounds({bodyX, layers.diameter * 0.5F, layers.bodyWidth, length});
            if (layers.bodyOverlay != nullptr)
            {
                layers.bodyOverlay->SetBounds(
                    {bodyX, layers.diameter * 0.5F, layers.bodyWidth, length});
            }
            const float tailX = (layers.diameter - layers.tailWidth) * 0.5F;
            layers.tail->SetSize({layers.tailWidth, layers.tailHeight});
            layers.tail->SetPosition(
                {tailX + layers.tailWidth * 0.5F, length + layers.tailHeight * 0.5F});
            if (layers.tailOverlay != nullptr)
            {
                layers.tailOverlay->SetSize({layers.tailWidth, layers.tailHeight});
                layers.tailOverlay->SetPosition(
                    {tailX + layers.tailWidth * 0.5F, length + layers.tailHeight * 0.5F});
            }
        }
    }
    PresentCompletionEffect(time);
}
