#include "LogoView.h"
#include "LogoStyle.h"
#include "App/AssetPaths.h"
#include "Texts/GameScene/FingerDrumLogoScene/LogoTexts.h"
using namespace logo_ui;

void LogoView::CreateLogoStrip()
{
    auto &strip = canvas_->CreateNode(mrg::visual2d::Anchor::Center, "FingerDrum.LogoStrip");
    strip.SetPivot({0.5F, 0.5F});
    logoStrip_ = &strip;

    // Every panel receives its own centered pivot. Their positions are then
    // calculated within the strip, which prevents aspect-ratio changes from
    // favoring either edge of the composition.
    leftFade_ = &mrg::visual2d::CreateSprite(
        strip, {0.0F, 0.0F, 1.0F, 1.0F},
        screenVisuals_.RegisterImage(mrg_client::asset_paths::default_skin::title::LeftFade()),
        "FingerDrum.LeftFade");
    centerLogo_ = &mrg::visual2d::CreateSprite(
        strip, {0.0F, 0.0F, 1.0F, 1.0F},
        screenVisuals_.RegisterImage(mrg_client::asset_paths::default_skin::title::Center()),
        "FingerDrum.CenterLogo");
    rightFade_ = &mrg::visual2d::CreateSprite(
        strip, {0.0F, 0.0F, 1.0F, 1.0F},
        screenVisuals_.RegisterImage(mrg_client::asset_paths::default_skin::title::RightFade()),
        "FingerDrum.RightFade");
}

void LogoView::CreateMenu()
{
    const auto &text = finger_drum::texts::Logo(texts_.CurrentLanguage());
    auto &menu = canvas_->CreateNode(mrg::visual2d::Anchor::Center, "FingerDrum.Menu");
    menu.SetPivot({0.5F, 0.5F});
    menu.SetSize(MenuSize);
    menu.SetPosition({0.0F, MenuCenterOffsetY});
    menu.SetZIndex(10);
    auto &gameStart = mrg::visual2d::CreateButton(
        menu, {ButtonLeft, MenuSize.height - ButtonHeight, ButtonWidth, ButtonHeight},
        std::wstring(text.menu[GameStartIndex]), "FingerDrum.GameStart");
    auto &editor = mrg::visual2d::CreateButton(menu,
                                               {ButtonLeft,
                                                MenuSize.height - ButtonVerticalStep - ButtonHeight,
                                                ButtonWidth, ButtonHeight},
                                               std::wstring(text.menu[EditorIndex]), "FingerDrum.Editor");
    auto &option = mrg::visual2d::CreateButton(
        menu,
        {ButtonLeft, MenuSize.height - ButtonVerticalStep * 2.0F - ButtonHeight, ButtonWidth,
         ButtonHeight},
        std::wstring(text.menu[OptionIndex]), "FingerDrum.Option");
    auto &exit = mrg::visual2d::CreateButton(
        menu,
        {ButtonLeft, MenuSize.height - ButtonVerticalStep * 3.0F - ButtonHeight, ButtonWidth,
         ButtonHeight},
        std::wstring(text.menu[ExitIndex]), "FingerDrum.Exit");
    menuButtons_ = {&gameStart, &editor, &option, &exit};
    menuButtonIds_ = {gameStart.Id(), editor.Id(), option.Id(), exit.Id()};

    for (mrg::visual2d::Visual2DNode *button : menuButtons_)
    {
        RequireComponent<mrg::visual2d::TextVisualComponent>(*button).SetFontSize(25.0F);
    }

    const mrg::visual2d::ImageHandle cursorImage = screenVisuals_.RegisterImage(
        mrg_client::asset_paths::default_skin::title::SelectionCursor());
    auto &cursor =
        mrg::visual2d::CreateSprite(menu, {0.0F, 0.0F, CursorSize.width, CursorSize.height},
                                    cursorImage, "FingerDrum.SelectionCursor");
    cursor.SetPivot({0.5F, 0.5F});
    cursor.SetZIndex(2);
    selectionCursor_ = &cursor;

    SetSelectedMenuItem(GameStartIndex);
}

void LogoView::ApplyTexts()
{
    const auto &text = finger_drum::texts::Logo(texts_.CurrentLanguage());
    for (std::size_t index = 0; index < menuButtons_.size(); ++index)
    {
        auto &component =
            RequireComponent<mrg::visual2d::TextVisualComponent>(*menuButtons_[index]);
        component.SetText(std::wstring(text.menu[index]));
        texts_.ApplyFont(component);
    }
    options_.RefreshTexts();
    textRevision_ = texts_.Revision();
}

void LogoView::UpdateLogoStripLayout()
{
    if (canvas_ == nullptr || logoStrip_ == nullptr || leftFade_ == nullptr ||
        centerLogo_ == nullptr || rightFade_ == nullptr)
    {
        return;
    }

    // The authored panels share one source height, so scale the complete strip
    // from that height. FixedHeight Canvas scaling then keeps the art exactly
    // flush with the physical viewport vertically at every window size.
    // Extra horizontal art remains outside the viewport instead of distorting
    // the source aspect ratio.
    const mrg::visual2d::Size logicalSize = canvas_->LogicalSize();
    const float scale = logicalSize.height / StripSourceHeight;

    const mrg::visual2d::Size leftSize{LeftFadeSourceSize.width * scale,
                                       LeftFadeSourceSize.height * scale};
    const mrg::visual2d::Size centerSize{CenterLogoSourceSize.width * scale,
                                         CenterLogoSourceSize.height * scale};
    const mrg::visual2d::Size rightSize{RightFadeSourceSize.width * scale,
                                        RightFadeSourceSize.height * scale};
    const mrg::visual2d::Size stripSize{leftSize.width + centerSize.width + rightSize.width,
                                        leftSize.height};

    logoStrip_->SetSize(stripSize);
    logoStrip_->SetPosition({0.0F, 0.0F});

    const float centerY = stripSize.height * 0.5F;
    SetCenteredImageLayout(*leftFade_, leftSize.width * 0.5F, centerY, leftSize);
    SetCenteredImageLayout(*centerLogo_, leftSize.width + centerSize.width * 0.5F, centerY,
                           centerSize);
    SetCenteredImageLayout(*rightFade_, leftSize.width + centerSize.width + rightSize.width * 0.5F,
                           centerY, rightSize);
}
