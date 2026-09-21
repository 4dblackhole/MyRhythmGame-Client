#include "MusicSelectView.h"
#include "SongSelectLayout.h"
#include "SongSelectionText.h"

using namespace song_select;

void MusicSelectView::Initialize(const mrg::EngineServices &services)
{
    width_ = services.windowWidth;
    height_ = services.windowHeight;
    canvasHandle_ = screenVisuals_.CreateOwnedCanvas(
        {layout::CanvasSize, mrg::visual2d::CanvasScaleMode::FixedHeight});
    canvas_ = canvasHandle_.Get();
    if (canvas_ == nullptr)
    {
        throw std::runtime_error("Failed to create the Lobby screen Canvas.");
    }

    board_ = &canvas_->CreateNode(mrg::visual2d::Anchor::Center,
                                  IsEditorSongSelect() ? "FingerDrum.EditorSongSelect.Board"
                                                       : "FingerDrum.SongSelect.Board");
    board_->SetPivot({0.5F, 0.5F});
    board_->SetSize(layout::CanvasSize);
    board_->SetPosition({0.0F, 0.0F});

    background_ = &AddPanel(*board_, layout::DesignBoard, CanvasBlue, "Background");
    CreateCategoryBar();
    if (!IsEditorSongSelect())
    {
        CreateRecordPanel();
    }
    CreateSongInformationPanel();
    CreateSongBrowser();
    CreateFooter();
    options_.Initialize(*canvas_);
    ApplyTexts();
    UpdateResponsiveLayout();
    RebuildVisibleSongs();
}

SongSelectCommand MusicSelectView::Update(const mrg::platform::InputState &input,
                                           const double deltaSeconds)
{
    command_ = SongSelectCommand::None;
    if (!canvas_)
        return command_;
    if (textRevision_ != texts_.Revision())
    {
        ApplyTexts();
        RebuildSongCards();
    }
    options_.Update(deltaSeconds);
    const bool controlDown =
        input.IsKeyDown(VK_LCONTROL) || input.IsKeyDown(VK_RCONTROL);
    if (controlDown && input.WasKeyPressed(static_cast<std::uint16_t>('O')))
        options_.Toggle();
    ProcessPointer(input);
    options_.ProcessInput(input, canvasPointer_, inputRouter_.HoveredNode(), inputRouter_);
    if (!ProcessActions())
        ProcessKeyboard(input);
    return command_;
}

void MusicSelectView::OnResize(const std::uint32_t width, const std::uint32_t height)
{
    width_ = width;
    height_ = height;
    if (canvas_ != nullptr)
    {
        UpdateResponsiveLayout();
        RebuildSongCards();
        EnsureFocusedCardVisible();
        inputRouter_.InvalidateHitTest();
    }
}

void MusicSelectView::Shutdown() noexcept
{
    if (canvas_ != nullptr)
    {
        inputRouter_.Reset(*canvas_);
    }
    options_.Shutdown();
    canvasPointer_.reset();
    difficultyButtonIds_.clear();
    songCards_.clear();
    goLabel_ = nullptr;
    goFill_ = nullptr;
    goButton_ = nullptr;
    backLabel_ = nullptr;
    backFill_ = nullptr;
    backButton_ = nullptr;
    optionLabel_ = nullptr;
    optionButton_ = nullptr;
    selectedDetailsLabel_ = nullptr;
    selectedCreatorLabel_ = nullptr;
    selectedPatternLabel_ = nullptr;
    selectedArtistLabel_ = nullptr;
    selectedSongLabel_ = nullptr;
    browserHint_ = nullptr;
    sortSelector_ = nullptr;
    searchCountLabel_ = nullptr;
    searchField_ = nullptr;
    scrollbarHandle_ = nullptr;
    scrollbarTrack_ = nullptr;
    songContent_ = nullptr;
    songViewport_ = nullptr;
    browserPanel_ = nullptr;
    informationDivider_ = nullptr;
    creatorHeading_ = nullptr;
    difficultyHeading_ = nullptr;
    difficultyInformation_ = nullptr;
    previewEmpty_ = nullptr;
    previewTitle_ = nullptr;
    preview_ = nullptr;
    informationPanel_ = nullptr;
    emptyRecordMessage_ = nullptr;
    recordSelector_ = nullptr;
    recordPanel_ = nullptr;
    categoryBar_ = nullptr;
    categoryLabel_ = nullptr;
    background_ = nullptr;
    board_ = nullptr;
    canvas_ = nullptr;
    canvasHandle_.Reset();
}

void MusicSelectView::RebuildVisibleSongs()
{
    selection_.RebuildVisibleSongs();
    scrollOffset_ = 0.0F;
    RefreshSelection();
    RefreshSearchPresentation();
}

bool MusicSelectView::IsEditorSongSelect() const noexcept
{
    return purpose_ == SongSelectPurpose::Editor;
}

void MusicSelectView::RefreshSelection()
{
    RebuildSongCards();
    RefreshSelectionPresentation();
    EnsureFocusedCardVisible();
}
