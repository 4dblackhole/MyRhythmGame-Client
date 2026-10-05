#include "GameplayPresenter.h"
#include "GameplaySupport.h"
using namespace gameplay;

void GameplayPresenter::UpdateMeasureLines(finger_drum::rhythm::RhythmTime time,
                                           finger_drum::rhythm::RhythmDuration horizon, bool enabled)
{
    for (auto &visual : measureLineVisuals_)
        visual.node->SetVisible(false);
    if (!enabled)
        return;
    const auto &lines = session_->MeasureLines();
    std::size_t used = 0;
    for (auto it = std::ranges::lower_bound(lines, time - MissedTravelDuration);
         it != lines.end() && *it <= time + horizon; ++it)
    {
        const float y = judgementLocalY_ + TravelPixels(*it - time);
        if (y > judgementLocalY_ + noteTravelDistance_ || y < judgementLocalY_ - noteTravelDistance_)
            continue;
        if (used == measureLineVisuals_.size())
        {
            auto &line = mrg::visual2d::CreatePanel(*laneRoot_, {}, "Lane.MeasureLine");
            SetColor(line, {.78F, .85F, .90F, .72F});
            line.SetZIndex(1);
            measureLineVisuals_.push_back({{}, &line});
        }
        auto &visual = measureLineVisuals_[used++];
        visual.timing = *it;
        visual.node->SetVisible(true);
        visual.node->SetBounds({0, y, laneWidth_, 4});
    }
}

void GameplayPresenter::UpdateTickVisuals(NoteVisualLayers &layers,
    const finger_drum::mode::NotePresentationInfo &presentation,
    finger_drum::rhythm::RhythmTime time, finger_drum::rhythm::RhythmDuration horizon)
{
    std::size_t used = 0;
    const auto &ticks = presentation.tickTimes;
    const float speed = static_cast<float>(presentation.scrollMultiplier);
    for (auto it = std::ranges::lower_bound(ticks, time - MissedTravelDuration);
         it != ticks.end() && *it <= time + horizon; ++it)
    {
        if (*it <= layers.timing)
            continue;
        const float y = judgementLocalY_ + TravelPixels(*it - time, speed);
        if (y < 0 || y > laneRoot_->NodeSize().height + layers.tickSize.height)
            continue;
        if (used == layers.ticks.size())
        {
            auto &node = mrg::visual2d::CreateSprite(*layers.root, {}, layers.tickImage, "LongNote.Tick");
            node.SetZIndex(4);
            layers.ticks.push_back({{}, &node});
        }
        auto &visual = layers.ticks[used++];
        visual.timing = *it;
        visual.node->SetVisible(true);
        visual.node->SetBounds({(layers.diameter - layers.tickSize.width) * .5F,
            layers.diameter * .5F - layers.tickSize.height * .5F + TravelPixels(*it - layers.timing, speed),
            layers.tickSize.width, layers.tickSize.height});
    }
    for (; used < layers.ticks.size(); ++used)
        layers.ticks[used].node->SetVisible(false);
}

mrg::visual2d::ImageHandle GameplayPresenter::NoteImage(std::wstring_view file)
{
    const auto found = noteImages_.find(file);
    if (found != noteImages_.end())
        return found->second;
    const auto image = screenVisuals_.RegisterImage(InGameSkinAssetPath(file));
    noteImages_.emplace(file, image);
    return image;
}

void GameplayPresenter::CreateNoteVisuals()
{
    static constexpr std::wstring_view files[]{L"note.png", L"noteoverlay.png", L"bignote.png",
        L"bigcircleoverlay.png", L"LNBody.png", L"LNBodyOverlay.png", L"LNTail.png", L"LNTailOverlay.png",
        L"BigLNBody.png", L"BigLNBodyOverlay.png", L"BigLNTail.png", L"BigLNTailOverlay.png",
        L"TickMarker.png", L"BuzzTickDiamond.png", L"Balloon.png", L"DengDeng.png",
        L"BalloonProcessing.png", L"BalloonBurst.png", L"DengDengProcessing.png", L"HitCounterCloud.png"};
    for (const auto file : files)
        static_cast<void>(NoteImage(file));
    const auto normalSize = ScaledImageSize(screenVisuals_, NoteImage(L"note.png"));
    pixelsPerWholeNote_ = normalSize.width * 16.0F * SixteenthSpacingInHeadDiameters;
    for (const auto file : {L"note.png", L"bignote.png", L"Balloon.png", L"DengDeng.png"})
        largestHeadRadius_ = std::max(largestHeadRadius_, ScaledImageSize(screenVisuals_, NoteImage(file)).width * .5F);
    if (!laneRoot_)
        throw std::logic_error("Note visuals require a Lane root.");
    for (const auto &lane : session_->Gear().Lanes())
        for (const auto &note : lane->Notes())
            noteModels_.emplace(note->Id(), note.get());
}

void GameplayPresenter::CreateNoteVisual(const finger_drum::rhythm::INote &source)
{
    const auto *note = &source;
    const auto noteImage = NoteImage(L"note.png");
    const auto noteOverlay = NoteImage(L"noteoverlay.png");
    const auto bigNoteImage = NoteImage(L"bignote.png");
    const auto bigOverlay =
        NoteImage(L"bigcircleoverlay.png");
    const auto bodyImage = NoteImage(L"LNBody.png");
    const auto bodyOverlayImage =
        NoteImage(L"LNBodyOverlay.png");
    const auto tailImage = NoteImage(L"LNTail.png");
    const auto tailOverlayImage =
        NoteImage(L"LNTailOverlay.png");
    const auto bigBodyImage = NoteImage(L"BigLNBody.png");
    const auto bigBodyOverlayImage =
        NoteImage(L"BigLNBodyOverlay.png");
    const auto bigTailImage = NoteImage(L"BigLNTail.png");
    const auto bigTailOverlayImage =
        NoteImage(L"BigLNTailOverlay.png");
    const auto tickImage = NoteImage(L"TickMarker.png");
    const auto buzzTickImage =
        NoteImage(L"BuzzTickDiamond.png");
    const auto balloonImage = NoteImage(L"Balloon.png");
    const auto dengDengImage = NoteImage(L"DengDeng.png");
    const auto balloonProcessingImage =
        NoteImage(L"BalloonProcessing.png");
    const auto balloonBurstImage =
        NoteImage(L"BalloonBurst.png");
    const auto dengDengProcessingImage =
        NoteImage(L"DengDengProcessing.png");
    const auto counterImage =
        NoteImage(L"HitCounterCloud.png");

    const auto noteSize = ScaledImageSize(screenVisuals_, noteImage);
    pixelsPerWholeNote_ = noteSize.width * 16.0F * SixteenthSpacingInHeadDiameters;
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
    layers.timing = note->Timing();
    if (presentation != nullptr && presentation->hasEndTime)
        layers.endTime = presentation->endTime;

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

        layers.tickImage = buzz ? buzzTickImage : tickImage;
        layers.tickSize = buzz ? buzzTickSize : tickSize;
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
        texts_.ApplyFont(text);
        text.SetFontSize(22.0F);
        text.SetTextColor({1.0F, 1.0F, 1.0F, 1.0F});
        text.SetHorizontalAlignment(mrg::visual2d::TextAlignment::Center);
    }
    noteVisuals_.emplace(note->Id(), std::move(layers));
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
    const auto horizon = std::max(VisibleTravelDuration(), Duration{1});
    const auto snapshot = session_->Gear().BuildSnapshot(
        time, horizon, MissedTravelDuration);
    HideTransientNoteVisuals();
    bool showMeasureLines = true;
    for (const auto &value : session_->EvaluateAutomation(time))
        if (value.type == finger_drum::chart::EffectCommandType::MeasureLineVisible)
            showMeasureLines = value.value >= 0.5;
    UpdateMeasureLines(time, horizon, showMeasureLines);
    bool focusClaimed{};
    for (const finger_drum::rhythm::ScrollNoteSnapshot &note : snapshot.notes)
    {
        auto found = noteVisuals_.find(note.noteId);
        if (found == noteVisuals_.end())
        {
            const auto model = noteModels_.find(note.noteId);
            if (model == noteModels_.end())
                continue;
            CreateNoteVisual(*model->second);
            found = noteVisuals_.find(note.noteId);
        }
        NoteVisualLayers &layers = found->second;
        presentedNoteIds_.push_back(note.noteId);
        const finger_drum::mode::NotePresentationInfo *presentation =
            session_->FindNotePresentation(note.noteId);
        const bool focusNote = layers.focusType != FocusNoteType::None;
        auto targetTime = layers.timing;
        const bool missed = note.state == NoteState::Missed;
        const bool completed = note.state == NoteState::Completed;
        const bool processing = focusNote && !missed && !completed && time >= note.timing &&
                                note.progress.has_value() && note.progress->accepted > 0;
        if (focusNote && time >= note.timing && !missed)
        {
            targetTime = time;
        }
        else if (focusNote && missed)
        {
            targetTime = note.expireTime;
        }
        const float speed =
            presentation != nullptr ? static_cast<float>(presentation->scrollMultiplier) : 1.0F;
        const float localY = judgementLocalY_ + TravelPixels(targetTime - time, speed);
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
            UpdateTickVisuals(layers, *presentation, time, horizon);
            const float tailTravel = std::clamp(TravelPixels(layers.endTime - time, speed),
                                                -0.05F * noteTravelDistance_,
                                                1.65F * noteTravelDistance_);
            const float tailLocalY = judgementLocalY_ + tailTravel;
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
    RemoveUnusedNoteVisuals();
}

void GameplayPresenter::RemoveUnusedNoteVisuals()
{
    for (auto it = noteVisuals_.begin(); it != noteVisuals_.end();)
    {
        if (std::ranges::find(presentedNoteIds_, it->first) != presentedNoteIds_.end() ||
            completionEffects_.contains(it->first))
        {
            ++it;
            continue;
        }
        // Model IDs/images stay cached, so seeking backward can recreate visuals.
        // Remove independent HUD nodes before the Lane subtree owning long parts.
        const auto &layers = it->second;
        for (auto *node : {layers.processing, layers.success, layers.counter, layers.root})
            if (node)
                static_cast<void>(canvas_->RemoveNode(node->Id()));
        it = noteVisuals_.erase(it);
    }
}
