#include "App/AssetPaths.h"
#include "LogoStyle.h"
#include "LogoView.h"
#include "Presentation/SkinVisualComponent.h"
#include "Texts/GameScene/YOUMULogoScene/LogoTexts.h"
using namespace logo_ui;

void LogoView::CreateLogoStrip()
{
    background_ = &CreateSprite(canvas_->AnchorNode(Anchor::Center), {}, {},
                                               "YOUMU.LogoBackground");
    RequireComponent<SpriteVisualComponent>(*background_).SetTint({1, 1, 1, 1});
    background_->SetZIndex(-1);
    background_->SetVisible(false);
    auto &strip = canvas_->CreateNode(Anchor::Center, "YOUMU.LogoStrip");
    strip.SetPivot({0.5F, 0.5F});
    logoStrip_ = &strip;

    // Every panel receives its own centered pivot. Their positions are then
    // calculated within the strip, which prevents aspect-ratio changes from
    // favoring either edge of the composition.
    leftFade_ = &CreateSprite(
        strip, {0.0F, 0.0F, 1.0F, 1.0F},
        screenVisuals_.RegisterImage(LeftFade()),
        "YOUMU.LeftFade");
    centerLogo_ = &CreateSprite(
        strip, {0.0F, 0.0F, 1.0F, 1.0F},
        screenVisuals_.RegisterImage(Center()),
        "YOUMU.CenterLogo");
    rightFade_ = &CreateSprite(
        strip, {0.0F, 0.0F, 1.0F, 1.0F},
        screenVisuals_.RegisterImage(RightFade()),
        "YOUMU.RightFade");
}

void LogoView::CreateMenu()
{
    const auto &text = Logo(texts_);
    auto &menu = canvas_->CreateNode(Anchor::Center, "YOUMU.Menu");
    menu.SetPivot({0.5F, 0.5F});
    menu.SetSize(MenuSize);
    menu.SetPosition({0.0F, MenuCenterOffsetY});
    menu.SetZIndex(10);
    auto &gameStart = CreateButton(
        menu, {ButtonLeft, MenuSize.height - ButtonHeight, ButtonWidth, ButtonHeight},
        std::wstring(text.menu[GameStartIndex]), "YOUMU.GameStart");
    auto &editor = CreateButton(menu,
                                               {ButtonLeft,
                                                MenuSize.height - ButtonVerticalStep - ButtonHeight,
                                                ButtonWidth, ButtonHeight},
                                               std::wstring(text.menu[EditorIndex]), "YOUMU.Editor");
    auto &option = CreateButton(
        menu,
        {ButtonLeft, MenuSize.height - ButtonVerticalStep * 2.0F - ButtonHeight, ButtonWidth,
         ButtonHeight},
        std::wstring(text.menu[OptionIndex]), "YOUMU.Option");
    auto &exit = CreateButton(
        menu,
        {ButtonLeft, MenuSize.height - ButtonVerticalStep * 3.0F - ButtonHeight, ButtonWidth,
         ButtonHeight},
        std::wstring(text.menu[ExitIndex]), "YOUMU.Exit");
    menuButtons_ = {&gameStart, &editor, &option, &exit};
    menuButtonIds_ = {gameStart.Id(), editor.Id(), option.Id(), exit.Id()};

    for (Visual2DNode *button : menuButtons_)
    {
        RequireComponent<TextVisualComponent>(*button).SetFontSize(25.0F);
    }
    constexpr std::array keys{"StartButton", "EditorButton", "OptionsButton", "ExitButton"};
    for (std::size_t i = 0; i < menuButtons_.size(); ++i)
        BindSkinImage(*menuButtons_[i], screenVisuals_, "Logo", keys[i]);

    const ImageHandle cursorImage = screenVisuals_.RegisterImage(
        SelectionCursor());
    auto &cursor =
        CreateSprite(menu, {0.0F, 0.0F, CursorSize.width, CursorSize.height},
                                    cursorImage, "YOUMU.SelectionCursor");
    cursor.SetPivot({0.5F, 0.5F});
    cursor.SetZIndex(2);
    selectionCursor_ = &cursor;

    SetSelectedMenuItem(GameStartIndex);
}

void LogoView::RefreshSkinImages()
{
    const auto revision = SkinSetSelection::Instance().Revision();
    if (revision == skinRevision_ || leftFade_ == nullptr || centerLogo_ == nullptr ||
        rightFade_ == nullptr || selectionCursor_ == nullptr)
        return;
    RequireComponent<SpriteVisualComponent>(*leftFade_).SetImage(
        screenVisuals_.RegisterImage(LeftFade()));
    RequireComponent<SpriteVisualComponent>(*centerLogo_).SetImage(
        screenVisuals_.RegisterImage(Center()));
    RequireComponent<SpriteVisualComponent>(*rightFade_).SetImage(
        screenVisuals_.RegisterImage(RightFade()));
    RequireComponent<SpriteVisualComponent>(*selectionCursor_).SetImage(
        screenVisuals_.RegisterImage(SelectionCursor()));
    const auto background = SkinSetSelection::Instance().Image("Logo", "Background");
    background_->SetVisible(!background.empty());
    logoStrip_->SetVisible(background.empty());
    if (!background.empty())
        RequireComponent<SpriteVisualComponent>(*background_)
            .SetImage(screenVisuals_.RegisterImage(background));
    for (auto *button : menuButtons_)
        button->GetComponent<SkinVisualComponent>()->Update(0);
    skinRevision_ = revision;
}

void LogoView::ApplyTexts()
{
    const auto &text = Logo(texts_);
    for (std::size_t index = 0; index < menuButtons_.size(); ++index)
    {
        auto &component =
            RequireComponent<TextVisualComponent>(*menuButtons_[index]);
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
    const Size logicalSize = canvas_->LogicalSize();
    background_->SetBounds(
        {-logicalSize.width * 0.5F, -logicalSize.height * 0.5F, logicalSize.width, logicalSize.height});
    const float scale = logicalSize.height / StripSourceHeight;

    const Size leftSize{LeftFadeSourceSize.width * scale,
                                       LeftFadeSourceSize.height * scale};
    const Size centerSize{CenterLogoSourceSize.width * scale,
                                         CenterLogoSourceSize.height * scale};
    const Size rightSize{RightFadeSourceSize.width * scale,
                                        RightFadeSourceSize.height * scale};
    const Size stripSize{leftSize.width + centerSize.width + rightSize.width,
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
