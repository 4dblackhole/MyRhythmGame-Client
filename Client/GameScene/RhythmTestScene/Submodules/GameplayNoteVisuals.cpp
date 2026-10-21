#include "GameplayPresenter.h"
#include "GameplaySupport.h"

namespace
{
    constexpr std::wstring_view NoteImageKeys[]{L"note",
                                                L"noteoverlay",
                                                L"bignote",
                                                L"bigcircleoverlay",
                                                L"LNBody",
                                                L"LNBodyOverlay",
                                                L"LNTail",
                                                L"LNTailOverlay",
                                                L"BigLNBody",
                                                L"BigLNBodyOverlay",
                                                L"BigLNTail",
                                                L"BigLNTailOverlay",
                                                L"TickMarker",
                                                L"BuzzTickDiamond",
                                                L"Balloon",
                                                L"DengDeng",
                                                L"BalloonProcessing",
                                                L"BalloonBurst",
                                                L"DengDengProcessing",
                                                L"HitCounterCloud"};
}

bool GameplayPresenter::PrepareAssets(const EngineServices &services)
{
    if (preparingImages_.empty())
    {
        for (const auto file : NoteImageKeys)
            preparingImages_.push_back(InGameSkinAssetPath(file));
        for (const auto file : {L"AccuracyIndicator",
                                L"InputPanel",
                                L"LaneLight",
                                L"JudgementCircle",
                                L"Lane",
                                L"KeyButtonLeftKat",
                                L"KeyButtonLeftDon",
                                L"KeyButtonRightDon",
                                L"KeyButtonRightKat",
                                L"KeyLightStrong",
                                L"KeyLightWeak",
                                L"KeyPressFlash",
                                L"Progress.Background",
                                L"Progress.Track",
                                L"Progress.Fill",
                                L"Progress.Marker",
                                L"Judgements.MAX",
                                L"Judgements.PERFECT",
                                L"Judgements.GREAT",
                                L"Judgements.GOOD",
                                L"Judgements.BAD",
                                L"Judgements.MISS",
                                L"Judgements.POOR",
                                L"Judgements.OK"})
            preparingImages_.push_back(InGameSkinAssetPath(file));
    }
    std::string error;
    const bool ready = services.visual2DRendering.PrepareImages(preparingImages_, error);
    if (!error.empty())
        throw std::runtime_error(error);
    if (!ready || backgroundPath_.empty())
        return ready;
    const std::array background{backgroundPath_};
    const bool backgroundReady = services.visual2DRendering.PrepareImages(background, error);
    if (!error.empty())
    {
        backgroundWarning_ = "Background image: " + error;
        backgroundPath_.clear();
        return true;
    }
    return backgroundReady;
}

void GameplayPresenter::UpdateMeasureLines(RhythmTime time, bool enabled)
{
    for (auto &visual : measureLineVisuals_)
        visual.node->SetVisible(false);
    if (!enabled)
        return;
    const auto &lines = session_->MeasureLines();
    std::size_t used = 0;
    const auto distance = session_->ScrollDistance(time, {});
    measureVisibility_.Visit(distance, distance,
                             [&](std::size_t index)
                             {
                                 const auto it = lines.begin() + static_cast<std::ptrdiff_t>(index);
                                 const auto position =
                                     LaneObjectPosition(it->timing, time, static_cast<float>(it->scrollMultiplier));
                                 if (position.y > judgementLocalY_ + noteTravelDistance_ ||
                                     position.y < judgementLocalY_ - noteTravelDistance_)
                                     return;
                                 if (used == measureLineVisuals_.size())
                                 {
                                     auto &line = CreatePanel(*laneRoot_, {}, "Lane.MeasureLine");
                                     line.SetPivot({0.5F, 0.5F});
                                     SetColor(line, {.78F, .85F, .90F, .72F});
                                     line.SetZIndex(1);
                                     measureLineVisuals_.push_back({{}, &line});
                                 }
                                 auto &visual = measureLineVisuals_[used++];
                                 visual.timing = it->timing;
                                 visual.node->SetVisible(true);
                                 visual.node->SetSize({laneWidth_, 4});
                                 visual.node->SetPosition(position);
                             });
}

void GameplayPresenter::UpdateTickVisuals(NoteVisualLayers &layers,
                                          const NotePresentationInfo &presentation,
                                          RhythmTime time,
                                          RhythmTime anchor, std::size_t processedTicks)
{
    std::size_t used = 0;
    const auto &ticks = presentation.tickTimes;
    const float speed = static_cast<float>(presentation.scrollMultiplier);
    const auto first = ticks.begin() + static_cast<std::ptrdiff_t>(std::min(processedTicks, ticks.size()));
    const long double distance = session_->ScrollDistance(time, {});
    const long double scale = session_->Timeline().BaseBpm() / 240'000'000.0L * pixelsPerWholeNote_ * speed;
    const auto cumulative = [this](auto tick) { return session_->ScrollDistance(tick, {}); };
    const auto last = std::ranges::upper_bound(
        first, ticks.end(),
        distance + (laneRoot_->NodeSize().height + layers.tickSize.height - judgementLocalY_) / scale, {}, cumulative);
    for (auto it = std::ranges::lower_bound(first, last, distance - (judgementLocalY_ + layers.tickSize.height) / scale,
                                            {}, cumulative);
         it != last; ++it)
    {
        if (*it <= layers.timing)
            continue;
        const float y = LaneObjectPosition(*it, time, speed).y;
        if (y < 0 || y > laneRoot_->NodeSize().height + layers.tickSize.height)
            continue;
        if (used == layers.ticks.size())
        {
            auto &node = CreateSprite(*layers.root, {}, layers.tickImage, "LongNote.Tick");
            node.SetZIndex(4);
            layers.ticks.push_back({{}, &node});
        }
        auto &visual = layers.ticks[used++];
        visual.timing = *it;
        visual.node->SetVisible(true);
        visual.node->SetBounds({(layers.diameter - layers.tickSize.width) * .5F,
            layers.diameter * .5F - layers.tickSize.height * .5F + TravelPixelsAt(*it, anchor, speed),
            layers.tickSize.width, layers.tickSize.height});
    }
    for (; used < layers.ticks.size(); ++used)
        layers.ticks[used].node->SetVisible(false);
}

ImageHandle GameplayPresenter::NoteImage(std::wstring_view file)
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
    for (const auto file : NoteImageKeys)
        static_cast<void>(NoteImage(file));
    const auto normalSize = ScaledImageSize(screenVisuals_, NoteImage(L"note"));
    pixelsPerWholeNote_ = normalSize.width * 16.0F * SixteenthSpacingInHeadDiameters;
    for (const auto file : {L"note", L"bignote", L"Balloon", L"DengDeng"})
        largestHeadRadius_ = std::max(largestHeadRadius_, ScaledImageSize(screenVisuals_, NoteImage(file)).width * .5F);
    if (!laneRoot_)
        throw std::logic_error("Note visuals require a Lane root.");
    for (const auto &lane : session_->Gear().Lanes())
        for (const auto &note : lane->Notes())
            noteModels_.emplace(note->Id(), note.get());
}

void GameplayPresenter::CreateNoteVisual(const INote &source)
{
    const auto *note = &source;
    const auto noteImage = NoteImage(L"note");
    const auto noteOverlay = NoteImage(L"noteoverlay");
    const auto bigNoteImage = NoteImage(L"bignote");
    const auto bigOverlay = NoteImage(L"bigcircleoverlay");
    const auto bodyImage = NoteImage(L"LNBody");
    const auto bodyOverlayImage = NoteImage(L"LNBodyOverlay");
    const auto tailImage = NoteImage(L"LNTail");
    const auto tailOverlayImage = NoteImage(L"LNTailOverlay");
    const auto bigBodyImage = NoteImage(L"BigLNBody");
    const auto bigBodyOverlayImage = NoteImage(L"BigLNBodyOverlay");
    const auto bigTailImage = NoteImage(L"BigLNTail");
    const auto bigTailOverlayImage = NoteImage(L"BigLNTailOverlay");
    const auto tickImage = NoteImage(L"TickMarker");
    const auto buzzTickImage = NoteImage(L"BuzzTickDiamond");
    const auto balloonImage = NoteImage(L"Balloon");
    const auto dengDengImage = NoteImage(L"DengDeng");
    const auto balloonProcessingImage = NoteImage(L"BalloonProcessing");
    const auto balloonBurstImage = NoteImage(L"BalloonBurst");
    const auto dengDengProcessingImage = NoteImage(L"DengDengProcessing");
    const auto counterImage = NoteImage(L"HitCounterCloud");

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

    const NotePresentationInfo *presentation =
        session_->FindNotePresentation(note->Id());
    const NoteVisualKind visualKind =
        presentation == nullptr ? NoteVisualKind::Don
                                : presentation->visualKind;
    const bool big = IsBigVisual(visualKind);
    const bool balloon = visualKind == NoteVisualKind::Balloon;
    const bool dengDeng = visualKind == NoteVisualKind::DengDeng;
    const bool customHead = balloon || dengDeng;
    const bool roll = (visualKind == NoteVisualKind::Roll ||
                       visualKind == NoteVisualKind::BigRoll) &&
                      note->Progress().has_value();
    const bool longNote = presentation != nullptr && presentation->hasEndTime &&
                          IsLongVisual(visualKind) && !customHead;
    const bool bigLongParts = visualKind == NoteVisualKind::BigRoll;
    const bool buzz = visualKind == NoteVisualKind::DonBuzz ||
                      visualKind == NoteVisualKind::KatBuzz;
    const auto headImage =
        balloon ? balloonImage
                : (dengDeng ? dengDengImage : (big ? bigNoteImage : noteImage));
    const auto headSize =
        balloon ? balloonSize : (dengDeng ? dengDengSize : (big ? bigNoteSize : noteSize));
    const float diameter = headSize.width;
    const Color ambientColor = AmbientColor(visualKind);

    NoteVisualLayers layers;
    layers.focusType = balloon ? FocusNoteType::Balloon
                               : (dengDeng ? FocusNoteType::DengDeng : FocusNoteType::None);
    layers.root = &noteRoot_->CreateChild(std::format("Lane.Note.{}", note->Id()));
    layers.root->SetPivot({0.5F, 0.5F});
    layers.root->SetSize({diameter, diameter});
    layers.root->SetPosition({laneWidth_ * 0.5F, judgementLocalY_});
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
        layers.body = &CreateSprite(
            *layers.root, {bodyX, diameter * 0.5F, layers.bodyWidth, 1.0F},
            selectedBodyImage, "AmbientBody");
        RequireComponent<SpriteVisualComponent>(*layers.body)
            .SetTint(ambientColor);
        layers.body->SetZIndex(0);
        layers.bodyOverlay = &CreateSprite(
            *layers.root, {bodyX, diameter * 0.5F, layers.bodyWidth, 1.0F},
            selectedBodyOverlayImage, "BodyOverlay");
        layers.bodyOverlay->SetZIndex(1);
        layers.tail = &CreateSprite(
            *layers.root, {tailX, 0.0F, layers.tailWidth, layers.tailHeight},
            selectedTailImage, "AmbientTail");
        RequireComponent<SpriteVisualComponent>(*layers.tail)
            .SetTint(ambientColor);
        layers.tail->SetZIndex(2);
        layers.tail->SetPivot({0.5F, 0.5F});
        layers.tail->SetPosition(
            {tailX + layers.tailWidth * 0.5F, layers.tailHeight * 0.5F});
        layers.tailOverlay = &CreateSprite(
            *layers.root, {tailX, 0.0F, layers.tailWidth, layers.tailHeight},
            selectedTailOverlayImage, "TailOverlay");
        layers.tailOverlay->SetZIndex(3);
        layers.tailOverlay->SetPivot({0.5F, 0.5F});
        layers.tailOverlay->SetPosition(
            {tailX + layers.tailWidth * 0.5F, layers.tailHeight * 0.5F});

        layers.tickImage = buzz ? buzzTickImage : tickImage;
        layers.tickSize = buzz ? buzzTickSize : tickSize;
    }

    layers.ambient = &CreateSprite(
        *layers.root, {0.0F, 0.0F, headSize.width, headSize.height}, headImage,
        "AmbientHead");
    RequireComponent<SpriteVisualComponent>(*layers.ambient)
        .SetTint(customHead ? Color{1.0F, 1.0F, 1.0F, 1.0F} : ambientColor);
    layers.ambient->SetZIndex(3);
    if (!customHead)
    {
        layers.overlay = &CreateSprite(
            *layers.root, {0.0F, 0.0F, headSize.width, headSize.height},
            big ? bigOverlay : noteOverlay, "UntintedOverlay");
        RequireComponent<SpriteVisualComponent>(*layers.overlay)
            .SetTint({1.0F, 1.0F, 1.0F, 1.0F});
        layers.overlay->SetZIndex(4);
    }
    if (customHead)
    {
        const auto processingImage =
            balloon ? balloonProcessingImage : dengDengProcessingImage;
        layers.processingSize = balloon ? balloonProcessingSize : dengDengProcessingSize;
        layers.processing = &CreateSprite(
            canvas_->Root(),
            {0.0F, 0.0F, layers.processingSize.width, layers.processingSize.height},
            processingImage, std::format("Hud.NoteProcessing.{}", note->Id()));
        layers.processing->SetPivot({0.5F, 0.5F});
        layers.processing->SetZIndex(7);
        layers.processing->SetVisible(false);
        if (balloon)
        {
            layers.successSize = balloonBurstSize;
            layers.success = &CreateSprite(
                canvas_->Root(),
                {0.0F, 0.0F, balloonBurstSize.width, balloonBurstSize.height},
                balloonBurstImage, std::format("Hud.NoteSuccess.{}", note->Id()));
            layers.success->SetPivot({0.5F, 0.5F});
            layers.success->SetZIndex(9);
            layers.success->SetVisible(false);
        }
    }
    if (customHead || roll)
    {
        layers.counter = &CreateSprite(
            canvas_->Root(), {0.0F, 0.0F, counterSize.width, counterSize.height},
            counterImage, std::format("Hud.NoteCounter.{}", note->Id()));
        layers.counter->SetZIndex(8);
        layers.counter->SetVisible(false);
        layers.counterText =
            &CreateLabel(*layers.counter,
                                        {0.0F, counterSize.height * 0.22F,
                                         counterSize.width, counterSize.height * 0.58F},
                                        L"", "RemainingHits");
        auto &text =
            RequireComponent<TextVisualComponent>(*layers.counterText);
        texts_.ApplyFont(text);
        text.SetFontSize(22.0F);
        text.SetTextColor({1.0F, 1.0F, 1.0F, 1.0F});
        text.SetHorizontalAlignment(TextAlignment::Center);
    }
    noteVisuals_.emplace(note->Id(), std::move(layers));
}

void GameplayPresenter::HideTransientNoteVisuals()
{
    for (const NoteId id : presentedNoteIds_)
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

void GameplayPresenter::UpdatePresentation(const RhythmTime time)
{
    progressView_.Update(time);
    UpdateAccuracyPresentation();
    using Duration = RhythmDuration;
    if (visibilityRevision_ != session_->PresentationRevision())
        RebuildVisibilityIndices();
    visibleNoteModels_.clear();
    const auto distance = session_->ScrollDistance(time, {});
    noteVisibility_.Visit(distance, distance,
                          [&](NoteId id)
                          {
                              const auto *model = noteModels_.at(id);
                              if (time <= model->ExpireTime() + MissedTravelDuration)
                                  visibleNoteModels_.push_back(model);
                          });
    std::ranges::sort(visibleNoteModels_, [](const auto *left, const auto *right)
                      {
                          if (left->Timing() != right->Timing())
                              return left->Timing() < right->Timing();
                          return left->Id() < right->Id();
                      });
    if (visibleNoteModels_.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        throw std::length_error("Too many visible notes to order.");
    int noteZIndex = static_cast<int>(visibleNoteModels_.size());
    ++presentationGeneration_;
    HideTransientNoteVisuals();
    bool showMeasureLines = true;
    for (const auto &value : session_->EvaluateAutomation(time))
        if (value.type == EffectCommandType::MeasureLineVisible)
            showMeasureLines = value.value >= 0.5;
    UpdateMeasureLines(time, showMeasureLines);
    bool focusClaimed{};
    for (const auto *model : visibleNoteModels_)
    {
        const ScrollNoteSnapshot note{.noteId = model->Id(),
                                                           .timing = model->Timing(),
                                                           .expireTime = model->ExpireTime(),
                                                           .state = model->State(),
                                                           .progress = model->Progress(),
                                                           .processedTicks = model->ProcessedTickCount()};
        auto found = noteVisuals_.find(note.noteId);
        if (found == noteVisuals_.end())
        {
            CreateNoteVisual(*model);
            found = noteVisuals_.find(note.noteId);
        }
        NoteVisualLayers &layers = found->second;
        // Reassign after visibility changes or backward seeks: creation order is
        // not chronological when Separate scroll speeds differ.
        layers.root->SetZIndex(--noteZIndex);
        presentedNoteIds_.push_back(note.noteId);
        layers.presentedGeneration = presentationGeneration_;
        const NotePresentationInfo *presentation =
            session_->FindNotePresentation(note.noteId);
        const bool focusNote = layers.focusType != FocusNoteType::None;
        auto targetTime = layers.timing;
        const bool missed = note.state == NoteState::Missed;
        const bool completed = note.state == NoteState::Completed;
        const bool countingHits = layers.counter != nullptr && !missed && !completed && time >= note.timing &&
                                  note.progress.has_value() && note.progress->accepted > 0;
        const bool processing = focusNote && countingHits;
        if (focusNote && time >= note.timing && !missed)
        {
            targetTime = time;
        }
        else if (focusNote && missed)
        {
            targetTime = note.expireTime;
        }
        const float speed = presentation != nullptr ? static_cast<float>(presentation->scrollMultiplier) : 1.0F;
        const auto position = LaneObjectPosition(targetTime, time, speed);
        const float localY = position.y;
        layers.root->SetPosition(position);
        const bool showLaneHead = focusNote ? !completed && (!processing || missed) : !completed && !missed;
        layers.root->SetVisible(showLaneHead);

        if (countingHits && !focusClaimed)
        {
            if (focusNote)
                PresentFocusNoteProcessing(layers, *note.progress, time);
            else
                PresentHitCounter(layers, *note.progress, layers.diameter);
            focusClaimed = true;
        }

        if (presentation != nullptr && presentation->hasEndTime && layers.body != nullptr &&
            layers.tail != nullptr)
        {
            UpdateTickVisuals(layers, *presentation, time, targetTime, note.processedTicks);
            const float tailTravel = std::clamp(TravelPixelsAt(layers.endTime, time, speed),
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
        if (it->second.presentedGeneration == presentationGeneration_ || completionEffects_.contains(it->first))
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

void GameplayPresenter::RebuildVisibilityIndices()
{
    using NoteIndex = IntervalIndex<long double, NoteId>;
    using MeasureIndex = IntervalIndex<long double, std::size_t>;
    std::vector<NoteIndex::Entry> notes;
    std::vector<MeasureIndex::Entry> lines;
    notes.reserve(noteModels_.size());
    firstVisibilityDistance_ = std::numeric_limits<long double>::infinity();
    const long double scale = session_->Timeline().BaseBpm() / 240'000'000.0L * pixelsPerWholeNote_;
    for (const auto &[id, model] : noteModels_)
    {
        const auto *info = session_->FindNotePresentation(id);
        const double speed = info ? info->scrollMultiplier : 1;
        const auto start = session_->ScrollDistance(model->Timing(), {});
        const auto endTime =
            info && info->hasEndTime ? std::max(info->endTime, model->ExpireTime()) : model->ExpireTime();
        const auto end = session_->ScrollDistance(endTime, {});
        const auto margin = noteTravelDistance_ / (scale * speed);
        notes.push_back({start - margin, end + margin, id});
        firstVisibilityDistance_ = std::min(firstVisibilityDistance_, start - margin);
    }
    const auto &measures = session_->MeasureLines();
    lines.reserve(measures.size());
    for (std::size_t i = 0; i < measures.size(); ++i)
    {
        const auto distance = session_->ScrollDistance(measures[i].timing, {});
        const auto margin = noteTravelDistance_ / (scale * measures[i].scrollMultiplier);
        lines.push_back({distance - margin, distance + margin, i});
        firstVisibilityDistance_ = std::min(firstVisibilityDistance_, distance - margin);
    }
    noteVisibility_.Assign(std::move(notes));
    measureVisibility_.Assign(std::move(lines));
    visibilityRevision_ = session_->PresentationRevision();
}
