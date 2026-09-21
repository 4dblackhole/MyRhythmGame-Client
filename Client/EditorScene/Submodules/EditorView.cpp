#include "EditorView.h"
#include <algorithm>
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
    ReloadSize();
    Build();
}

void EditorView::Build()
{
    controls.clear();
    visual->packets.clear();
    Box({0, 0, 1920, 1080}, Background);
    static constexpr const wchar_t *tabs[]{L"패턴", L"박자표", L"메타데이터", L"이펙트"};
    for (int i = 0; i < 4; ++i)
        Button(
            {i * 220.0F, 0, 220, 60}, tabs[i],
            [this, i] {
                state_.tab = i;
                state_.listOffset = 0;
                state_.popup = -1;
                state_.rebuild = true;
            },
            state_.tab == i);
    if (state_.tab == 0)
        DrawScore();
    else if (state_.tab == 1)
        DrawTiming();
    else if (state_.tab == 2)
        DrawMetadata();
    else
        DrawEffects();
    Button(
        {1740, 1035, 150, 36}, state_.editor->Dirty() ? L"저장 *" : L"저장",
        [this] { state_.Save(); }, true);
    Text({24, 1040, 1690, 32}, Wide(state_.status), 16);
    state_.rebuild = false;
}

void EditorView::Box(v::Rect r, v::Color color, float radius)
{
    v::DrawPacket p{};
    p.bounds = {r.x - 960, 540 - r.y - r.height, r.width, r.height};
    p.color = color;
    p.cornerRadius = radius;
    visual->packets.push_back(std::move(p));
}

void EditorView::Text(v::Rect r, std::wstring text, float size, v::Color color)
{
    v::DrawPacket p{};
    p.type = v::DrawPacketType::Text;
    p.bounds = {r.x - 960, 540 - r.y - r.height, r.width, r.height};
    p.color = color;
    p.text = std::move(text);
    p.fontSize = size;
    visual->packets.push_back(std::move(p));
}

void EditorView::Button(v::Rect r, std::wstring text, std::function<void()> action, bool selected)
{
    Box(r, selected ? Blue : Pale, 5);
    Text({r.x + 10, r.y + 3, r.width - 15, r.height - 4}, std::move(text), 20,
         selected ? White : Ink);
    controls.push_back({r, std::move(action)});
}

void EditorView::Field(v::Rect r, const wchar_t *label, std::string &value)
{
    Text({r.x, r.y - 28, r.width, 24}, label, 17);
    Button(r, Wide(value), [this, label, &value] {
        if (auto edited = EditText(label, value))
        {
            value = *edited;
            state_.rebuild = true;
        }
    });
}

void EditorView::Circle(float x, float y, int type, float radius)
{
    const auto color = type == 2 || type == 4 ? Kat
                       : type == 5            ? v::Color{.65F, .30F, .85F, 1}
                       : type > 5             ? Gold
                                              : Don;
    Box({x - radius - 2, y - radius - 2, radius * 2 + 4, radius * 2 + 4}, White, radius + 2);
    Box({x - radius, y - radius, radius * 2, radius * 2}, color, radius);
}

void EditorView::ReloadSize()
{
    const float scale = std::min(1.0F, canvas.Get()->LogicalSize().width / 1920.0F);
    node->Transform().SetScale(scale, scale, 1);
    state_.rebuild = true;
}
