#include "MusicSelectView.h"
#include "SongSelectLayout.h"
#include "Texts/GameScene/MusicSelectScene/MusicSelectTexts.h"

using namespace song_select;

void MusicSelectView::CreateAdditionMenu()
{
    if (!IsEditorSongSelect())
        return;
    additionMenu_ = &CreatePanel(canvas_->Root(), {0, 0, 200, 84}, "SongAddition.Menu");
    additionMenu_->SetZIndex(80);
    ApplySpriteStyle(*additionMenu_, PanelWhite, BorderBlue);
    SetCornerRadius(*additionMenu_, 6);
    newDifficultyItem_ = &CreateButton(*additionMenu_, {4, 44, 192, 36}, L"", "SongAddition.NewDifficulty");
    convertOsuItem_ = &CreateButton(*additionMenu_, {4, 4, 192, 36}, L"", "SongAddition.ConvertOsu");
    for (auto *item : {newDifficultyItem_, convertOsuItem_})
    {
        ApplyButtonStyle(*item, PanelWhite, FocusBlue, AccentBlue);
        auto &label = RequireComponent<TextVisualComponent>(*item);
        label.SetFontSize(18);
        texts_.ApplyFont(label);
    }
    CloseAdditionMenu();
}
void MusicSelectView::OpenAdditionMenu(const Rect anchor)
{
    if (!additionMenu_)
        return;
    const auto size = canvas_->LogicalSize();
    additionMenu_->SetPosition(
        {std::clamp(anchor.x + anchor.width - 200, -size.width / 2, std::max(-size.width / 2, size.width / 2 - 200)),
         std::clamp(anchor.y - 84, -size.height / 2, std::max(-size.height / 2, size.height / 2 - 84))});
    additionMenu_->SetVisible(true);
    inputRouter_.InvalidateHitTest();
}
void MusicSelectView::CloseAdditionMenu()
{
    if (additionMenu_)
        additionMenu_->SetVisible(false);
    if (canvas_)
        inputRouter_.Reset(*canvas_);
}
bool MusicSelectView::ProcessAdditionMenu(const InputState &input)
{
    if (!additionMenu_ || !additionMenu_->IsVisible())
        return false;
    for (const auto &action : canvas_->TakeActions())
    {
        if (action.type != ActionType::Clicked)
            continue;
        if (action.source == newDifficultyItem_->Id())
            command_ = SongSelectCommand::CreateDifficulty;
        else if (action.source == convertOsuItem_->Id())
            command_ = SongSelectCommand::ConvertOsu;
    }
    const bool outside = input.WasMouseButtonPressed(MouseButton::Left) &&
                         (!canvasPointer_ || !additionMenu_->BoundsInCanvas().Contains(*canvasPointer_));
    if (command_ != SongSelectCommand::None || outside || input.WasKeyPressed(VK_ESCAPE) ||
        input.WasMouseButtonPressed(MouseButton::Right))
        CloseAdditionMenu();
    return true; // An open menu consumes input, including clicks which dismiss it.
}
