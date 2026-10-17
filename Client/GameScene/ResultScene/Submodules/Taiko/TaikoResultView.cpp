#include "TaikoResultView.h"
#include "App/AssetPaths.h"
#include "Judgement/AccuracyScore.h"
#include "Presentation/SkinVisualComponent.h"
#include "TaikoResultLayout.h"
#include "Texts/GameScene/ResultScene/ResultTexts.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string_view>
namespace v = mrg::visual2d;
using namespace taiko_result_ui::layout;

namespace
{
    std::wstring_view RankImageKey(const finger_drum::rhythm::AccuracyScore accuracy) noexcept
    {
        struct RankThreshold
        {
            std::uint32_t minimumPercent;
            std::wstring_view imageKey;
        };
        static constexpr std::array<RankThreshold, 10> ranks{{
            {100, L"Rank.Y"},
            {99, L"Rank.PERFECT"},
            {98, L"Rank.SSS"},
            {96, L"Rank.SS"},
            {93, L"Rank.S"},
            {90, L"Rank.A"},
            {85, L"Rank.B"},
            {80, L"Rank.C"},
            {75, L"Rank.D"},
            {70, L"Rank.E"},
        }};
        // Compare the recorded six-decimal units, never rounded display text.
        for (const auto &rank : ranks)
            if (accuracy.Units() >= rank.minimumPercent * 1'000'000U)
                return rank.imageKey;
        return L"Rank.F";
    }
} // namespace

TaikoResultView::TaikoResultView(const finger_drum::mode::TaikoPlayResult &result, v::ScreenVisual2DManager &visuals,
                                 finger_drum::texts::TextCatalog &texts, ResultViewContext context)
    : result_(result), visuals_(visuals), texts_(texts), context_(std::move(context))
{
}
TaikoResultView::~TaikoResultView()
{
    Shutdown();
}
void TaikoResultView::LoadAssets()
{
    const auto load = [this](const auto &path)
    { return visuals_.RegisterImage(mrg_client::asset_paths::skin::Result(path)); };
    sky_ = load(L"Background.Sky");
    clouds_ = load(L"Background.Clouds");
    modeIcon_ = load(L"Mode.Taiko");
    rankFrame_ = load(L"Rank.Frame");
    rankImage_ = load(RankImageKey(result_.summary.accuracy));
    font_ = {v::TextFontSource::File, mrg_client::asset_paths::fonts::NotoSansKR().wstring()};
}
void TaikoResultView::Initialize(const mrg::EngineServices &services)
{
    width_ = services.windowWidth;
    height_ = services.windowHeight;
    canvasHandle_ = visuals_.CreateOwnedCanvas({{Width, Height}, v::CanvasScaleMode::FixedHeight});
    canvas_ = canvasHandle_.Get();
    if (!canvas_)
        throw std::runtime_error("Failed to create result Canvas.");
    canvas_->SetViewportSize({static_cast<float>(width_), static_cast<float>(height_)});
    LoadAssets();
    Build();
}
v::Visual2DNode &TaikoResultView::Label(v::Visual2DNode &root, v::Rect designBounds, std::wstring text, float size,
                                        std::uint16_t weight, v::TextAlignment alignment, v::Color color)
{
    auto &label = v::CreateLabel(root, Bounds(designBounds), std::move(text), "Result.Label");
    auto &component = *label.GetComponent<v::TextVisualComponent>();
    component.SetFont(font_);
    component.SetFontSize(size);
    component.SetFontWeight(weight);
    component.SetTextColor(color);
    component.SetHorizontalAlignment(alignment);
    label.SetClipRect({0, 0, designBounds.width, designBounds.height});
    return label;
}
void TaikoResultView::Panel(v::Visual2DNode &root, v::Rect designBounds, v::Color fill, float radius, float border,
                            v::Color borderColor)
{
    const auto layer = [&](v::Rect bounds, v::Color color, float corner)
    {
        auto &panel = v::CreatePanel(root, Bounds(bounds), "Result.Panel");
        auto &visual = *panel.GetComponent<v::SpriteVisualComponent>();
        visual.SetTint(color);
        visual.SetCornerRadius(corner);
    };
    if (border > 0)
    {
        layer(designBounds, borderColor, radius);
        designBounds = {designBounds.x + border, designBounds.y + border, designBounds.width - 2 * border,
                        designBounds.height - 2 * border};
    }
    layer(designBounds, fill, std::max(0.0F, radius - border));
}
v::Visual2DNode &TaikoResultView::Button(v::Visual2DNode &root, v::Rect designBounds, std::string name,
                                         std::wstring title, bool outlined)
{
    auto &button = v::CreateButton(root, Bounds(designBounds), L"", std::move(name));
    auto &sprite = *button.GetComponent<v::SpriteVisualComponent>();
    v::VisualStyle style;
    style.normal = outlined ? Accent : Color(0x75B9E4);
    style.hovered = Accent;
    style.pressed = Ink;
    style.disabled = style.normal;
    sprite.SetStyle(style);
    sprite.SetCornerRadius(20);
    if (outlined)
    {
        auto &inner =
            v::CreatePanel(button, {3, 3, designBounds.width - 6, designBounds.height - 6}, "Result.ButtonInset");
        inner.GetComponent<v::SpriteVisualComponent>()->SetTint(Surface);
        inner.GetComponent<v::SpriteVisualComponent>()->SetCornerRadius(17);
        inner.SetZIndex(-1);
    }
    const auto key = button.Name().substr(std::string_view("Result.").size()) + "Button";
    finger_drum::presentation::BindSkinImage(button, visuals_, "Result", key, "Result.ButtonInset");
    // Child text paints after the inset. Parent-component text would be hidden
    // by the inset's later rectangle/depth contribution.
    auto &caption =
        v::CreateLabel(button, {20, 15, designBounds.width - 40, 32}, std::move(title), "Result.ButtonText");
    auto &text = *caption.GetComponent<v::TextVisualComponent>();
    text.SetFont(font_);
    text.SetFontSize(20);
    text.SetFontWeight(700);
    text.SetHorizontalAlignment(v::TextAlignment::Center);
    text.SetTextColor(outlined ? Ink : Color(0xFFFFFF));
    return button;
}
void TaikoResultView::BuildBackground()
{
    const auto size = canvas_->LogicalSize();
    auto &anchor = canvas_->AnchorNode(v::Anchor::Center);
    auto &sky =
        v::CreateSprite(anchor, {-size.width / 2, -size.height / 2, size.width, size.height}, sky_, "Result.Sky");
    sky.SetZIndex(-10);
    backgroundId_ = sky.Id();
    auto &clouds =
        v::CreateSprite(anchor, {-size.width / 2, -size.height / 2, size.width, 168}, clouds_, "Result.Clouds");
    // Footer decoration stays above the sky and behind all result content.
    clouds.SetZIndex(-9);
    footerId_ = clouds.Id();
}
void TaikoResultView::Build()
{
    router_.Reset(*canvas_);
    for (const auto id : {contentId_, backId_, backgroundId_, footerId_})
        if (id)
            static_cast<void>(canvas_->RemoveNode(id));
    auto &root = canvas_->CreateNode(v::Anchor::Center, "Result.Content");
    root.SetBounds({0, 0, Width, Height});
    root.SetPivot({.5F, .5F});
    const float scale = std::min(canvas_->LogicalSize().width / Width, 1.0F);
    root.Transform().SetScale(scale, scale, 1);
    contentId_ = root.Id();
    BuildBackground();
    BuildSongBanner(root);
    BuildSummary(root);
    BuildMetadata(root);
    BuildGraphs(root);
    BuildActions(root);
    textRevision_ = texts_.Revision();
    router_.InvalidateHitTest();
}
void TaikoResultView::SetVisible(bool visible) noexcept
{
    static_cast<void>(canvasHandle_.SetVisible(visible));
}
void TaikoResultView::ProcessPointer(const mrg::platform::InputState &input)
{
    const auto position = input.IsMouseInsideWindow()
                              ? v::MapScreenPointer({static_cast<float>(input.MousePositionX()),
                                                     static_cast<float>(input.MousePositionY())},
                                                    {static_cast<float>(width_), static_cast<float>(height_)}, *canvas_)
                              : std::nullopt;
    v::PointerInput pointer;
    pointer.available = position.has_value();
    pointer.position = position.value_or(v::Point{});
    pointer.leftButtonDown = input.IsMouseButtonDown(mrg::platform::MouseButton::Left);
    pointer.leftButtonPressed = input.WasMouseButtonPressed(mrg::platform::MouseButton::Left);
    pointer.leftButtonReleased = input.WasMouseButtonReleased(mrg::platform::MouseButton::Left);
    router_.Process(*canvas_, pointer);
}
ResultAction TaikoResultView::Update(const mrg::platform::InputState &input)
{
    if (!canvas_)
        return ResultAction::None;
    if (textRevision_ != texts_.Revision())
        Build();
    ProcessPointer(input);
    for (const auto &action : canvas_->TakeActions())
    {
        if (action.type != v::ActionType::Clicked)
            continue;
        if (action.source == backId_)
            return ResultAction::Back;
        if (action.source == retryId_ && context_.canRetry)
            return ResultAction::Retry;
    }
    return input.WasKeyPressed(VK_ESCAPE) || input.WasKeyPressed(VK_RETURN) ? ResultAction::Back : ResultAction::None;
}
void TaikoResultView::OnResize(std::uint32_t width, std::uint32_t height)
{
    if (width == 0 || height == 0)
        return;
    width_ = width;
    height_ = height;
    if (canvas_)
    {
        canvas_->SetViewportSize({static_cast<float>(width), static_cast<float>(height)});
        Build();
    }
}
void TaikoResultView::Shutdown() noexcept
{
    if (canvas_)
        router_.Reset(*canvas_);
    contentId_ = backId_ = retryId_ = backgroundId_ = footerId_ = 0;
    canvas_ = nullptr;
    canvasHandle_.Reset();
}
