#include "EditorView.h"
#include <algorithm>
#include <format>
#include <sstream>
using namespace editor_ui;

void EditorView::Initialize(const mrg::EngineServices &services)
{
    width = services.windowWidth;
    height = services.windowHeight;
    canvas = visuals.CreateOwnedCanvas({{1920, 1080}, v::CanvasScaleMode::FixedHeight});
    static_cast<void>(canvas.SetVisible(true));
    node = &canvas.Get()->CreateNode(v::Anchor::Center, "Editor");
    visual = &node->AddComponent<EditorVisual>();
    divisionSliderNode_ =
        &v::CreateSlider(*node,
                         {DivisionSliderRect.x - 960, 540 - DivisionSliderRect.y - DivisionSliderRect.height,
                          DivisionSliderRect.width, DivisionSliderRect.height},
                         (state_.Score().division - 1) / 15.0F, "Beat division");
    v::VisualStyle sliderStyle{};
    sliderStyle.normal.alpha = 0;
    sliderStyle.hovered.alpha = 0;
    sliderStyle.pressed.alpha = 0;
    sliderStyle.disabled.alpha = 0;
    divisionSliderNode_->GetComponent<v::SpriteVisualComponent>()->SetStyle(sliderStyle);
    timelineSliderNode_ =
        &v::CreateSlider(*node,
                         {TimelineSliderRect.x - 960, 540 - TimelineSliderRect.y - TimelineSliderRect.height,
                          TimelineSliderRect.width, TimelineSliderRect.height},
                         0, "Editor timeline");
    timelineSliderNode_->GetComponent<v::SpriteVisualComponent>()->SetStyle(sliderStyle);
    playbackRateSliderNode_ = &v::CreateSlider(
        *node, {PlaybackRateSliderRect.x - 960, 540 - PlaybackRateSliderRect.y - PlaybackRateSliderRect.height,
                PlaybackRateSliderRect.width, PlaybackRateSliderRect.height},
        static_cast<float>((state_.PlaybackRate() - EditorPlayback::MinimumRate) /
                           (EditorPlayback::MaximumRate - EditorPlayback::MinimumRate)), "Editor playback rate");
    playbackRateSliderNode_->GetComponent<v::SpriteVisualComponent>()->SetStyle(sliderStyle);
    InitializeAudioSelection();
    menuBar_.Initialize(*canvas.Get(), texts_);
    ReloadSize();
    Build();
}

void EditorView::BuildContent()
{
    for (auto *root : audioRoots_)
        if (root)
            root->SetVisible(state_.Tab() == EditorTab::Audio);
    controls.clear();
    visual->Clear();
    Box({0, 0, 1920, 1080}, Background);
    DrawNavigation();
    if (state_.Tab() == EditorTab::Pattern)
        DrawScore();
    else if (state_.Tab() == EditorTab::Timing)
        DrawTiming();
    else if (state_.Tab() == EditorTab::Metadata)
        state_.Mode().DrawMetadata(*this, state_, texts_);
    else if (state_.Tab() == EditorTab::Effects)
        DrawEffects();
    else
        DrawAudio();
    SyncInputControls();
    DrawFooter();
    visual->Finish();
    textRevision_ = texts_.Revision();
    state_.FinishBuild();
}

void EditorView::SyncInputControls()
{
    const bool showDivisionSlider = state_.Tab() == EditorTab::Pattern;
    const bool showAudioControls = state_.Tab() == EditorTab::Audio;
    if (playbackRateSliderNode_->IsVisible() != showAudioControls)
    {
        sliderInput_.Reset(*canvas.Get());
        audioSelectionAnchor_.reset();
        playbackRateSliderNode_->SetVisible(showAudioControls);
        sliderInput_.InvalidateHitTest();
    }
    for (auto *track : audioSelectionNodes_)
        track->SetVisible(showAudioControls);
    if (divisionSliderNode_->IsVisible() != showDivisionSlider)
    {
        if (!showDivisionSlider)
            sliderInput_.Reset(*canvas.Get());
        divisionSliderNode_->SetVisible(showDivisionSlider);
        sliderInput_.InvalidateHitTest();
    }
    const bool showTimelineSlider = state_.Tab() == EditorTab::Pattern || state_.Tab() == EditorTab::Audio;
    if (timelineSliderNode_->IsVisible() != showTimelineSlider)
    {
        if (!showTimelineSlider)
            sliderInput_.Reset(*canvas.Get());
        timelineSliderNode_->SetVisible(showTimelineSlider);
        sliderInput_.InvalidateHitTest();
    }
    SyncDivisionSlider();
    SyncTimelineSlider();
    playbackRateSliderNode_->GetComponent<v::SliderBehaviorComponent>()->SetValue(
        static_cast<float>((state_.PlaybackRate() - EditorPlayback::MinimumRate) /
                           (EditorPlayback::MaximumRate - EditorPlayback::MinimumRate)));
}

void EditorView::Box(v::Rect r, v::Color color, float radius)
{
    visual->AddRectangle({r.x - 960, 540 - r.y - r.height, r.width, r.height}, color, radius);
}

void EditorView::Image(v::Rect r, const std::filesystem::path &path, v::Color tint)
{
    v::DrawPacket packet{};
    packet.type = v::DrawPacketType::Image;
    packet.bounds = {r.x - 960, 540 - r.y - r.height, r.width, r.height};
    packet.image = visuals.RegisterImage(path);
    if (packet.image.value == 0)
        throw std::runtime_error("Editor skin image could not be loaded.");
    packet.color = tint;
    visual->AddPacket(std::move(packet));
}

void EditorView::Text(v::Rect r, std::wstring text, float size, v::Color color)
{
    v::DrawPacket p{};
    p.type = v::DrawPacketType::Text;
    p.bounds = {r.x - 960, 540 - r.y - r.height, r.width, r.height};
    p.color = color;
    p.text = std::make_shared<v::TextContent>(
        v::TextContent{std::move(text), texts_.CurrentProfile().font, size, v::TextAlignment::Leading, {}});
    visual->AddPacket(std::move(p));
}

finger_drum::texts::EditorTextSet EditorView::Texts() const noexcept
{
    return finger_drum::texts::Editor(texts_);
}

void EditorView::Button(v::Rect r, std::wstring text, std::function<void()> action, bool selected)
{
    Box(r, selected ? Blue : Pale, 5);
    Text({r.x + 10, r.y + 3, r.width - 15, r.height - 4}, std::move(text), 20, selected ? White : Ink);
    controls.push_back({r, std::move(action)});
}

void EditorView::Field(v::Rect r, const wchar_t *label, std::string &value)
{
    Text({r.x, r.y - 28, r.width, 24}, label, 17);
    Button(r, Wide(value), [this, label, &value] {
        if (auto edited = EditText(label, value, Texts()))
        {
            value = *edited;
            state_.RequestRebuild();
        }
    });
}

void EditorView::ReloadSize()
{
    const float scale = std::min(1.0F, canvas.Get()->LogicalSize().width / 1920.0F);
    node->Transform().SetScale(scale, scale, 1);
    menuBar_.Resize();
    sliderInput_.InvalidateHitTest();
    state_.RequestRebuild();
}

void EditorView::SyncDivisionSlider() noexcept
{
    divisionSliderNode_->GetComponent<v::SliderBehaviorComponent>()->SetValue(
        (std::min(state_.Score().division, 16) - 1) / 15.0F);
}

void EditorView::DrawNavigation()
{
    const auto &text = Texts();
    for (int i = 0; i < static_cast<int>(text.tabs.size()); ++i)
        Button(
            {i * 200.0F, EditorMenuBar::Height, 200, 40}, std::wstring(text.tabs[i]),
            [this, i] { state_.SelectTab(static_cast<EditorTab>(i)); }, static_cast<int>(state_.Tab()) == i);
}

void EditorView::DrawFooter()
{
    const auto &text = Texts();
    if (state_.Tab() != EditorTab::Audio)
    {
        Button(
            {132, 1000, 180, 26}, std::wstring(texts_.Get("Editor.undo")), [this] { state_.Undo(); },
            state_.Document().CanUndo());
        Button(
            {320, 1000, 180, 26}, std::wstring(texts_.Get("Editor.redo")), [this] { state_.Redo(); },
            state_.Document().CanRedo());
        if (state_.Tab() != EditorTab::Metadata)
        {
            Button({508, 1000, 180, 26}, std::wstring(texts_.Get("Editor.copy")), [this] { state_.CopySelection(); });
            Button({696, 1000, 210, 26}, std::wstring(texts_.Get("Editor.paste")),
                   [this] { state_.PasteSelection(false); });
            Button({914, 1000, 350, 26}, std::wstring(texts_.Get("Editor.pasteWithBlanks")),
                   [this] { state_.PasteSelection(true); });
            Text({1280, 1000, 600, 26}, std::wstring(texts_.Get("Editor.selectionHelp")), 14);
        }
    }
    Button(
        {1740, 1035, 150, 36}, std::wstring(state_.Document().Dirty() ? text.saveDirty : text.save),
        [this] { state_.Save(); }, true);
    const auto savedPath = state_.SavedPath().wstring();
    Text({24, 1040, 1690, 32},
         savedPath.empty() ? Wide(state_.Status()) : texts_.Format("Editor.savedFormat", savedPath), 16);
}

void EditorView::Build()
{
    try
    {
        menuBar_.Refresh(state_.Document().Dirty());
        BuildContent();
        buildFailed_ = false;
    }
    catch (const std::exception &error)
    {
        // Drop partial packets and their callbacks. Keep navigation and time
        // correction available, with no automatic retry until the next action.
        state_.SetStatus(error.what());
        buildFailed_ = true;
        for (auto *root : audioRoots_)
            if (root)
                root->SetVisible(false);
        controls.clear();
        visual->Clear();
        sliderInput_.Reset(*canvas.Get());
        divisionSliderNode_->SetVisible(false);
        timelineSliderNode_->SetVisible(false);
        playbackRateSliderNode_->SetVisible(false);
        for (auto *track : audioSelectionNodes_)
            track->SetVisible(false);
        audioSelectionAnchor_.reset();
        sliderInput_.InvalidateHitTest();
        Box({0, 0, 1920, 1080}, Background);
        DrawNavigation();
        DrawTimeInput({1600, 887, 280, 32});
        DrawFooter();
        visual->Finish();
        state_.FinishBuild();
        textRevision_ = texts_.Revision();
    }
}
