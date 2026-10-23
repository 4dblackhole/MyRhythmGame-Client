#include "TaikoResultLayout.h"
#include "TaikoResultView.h"
#include <format>
using namespace taiko_result_ui::layout;
void TaikoResultView::BuildOptions(Visual2DNode &root)
{
    const auto &o = result_.summary.playOptions;
    struct Badge
    {
        const char *key;
        std::wstring symbol, value;
    };
    std::vector<Badge> badges;
    if (o.scrollUnit == PlayOptions::ScrollUnit::Bpm || o.scrollSpeed != 1.0 || o.ignoreScrollEffects)
        badges.push_back(
            {"scrollSpeed", L"»",
             std::format(L"{:g}{}{}", o.scrollSpeed, o.scrollUnit == PlayOptions::ScrollUnit::Bpm ? L" BPM" : L"x",
                         o.ignoreScrollEffects ? L" FIX" : L"")});
    if (o.metronome)
        badges.push_back({"metronome", L"♩", L"ON"});
    if (o.mirror)
        badges.push_back({"mirror", L"↔", L"Don / Kat"});
    if (o.random)
        badges.push_back({"random", L"?", std::format(L"{:g}%  #{}", o.randomAttemptProbability * 100, o.randomSeed)});
    if (o.accelerate != 1.0)
        badges.push_back({"accelerate", L"×",
                          std::format(L"{:.2f}x  BGM {} / HIT {}", o.accelerate, o.preserveMusicPitch ? L"=" : L"↗",
                                      o.preserveHitSoundPitch ? L"=" : L"↗")});
    if (o.judgeLevel)
        badges.push_back({"judgeLevel", L"JL", std::format(L"{:.2f}", o.judgeLevel->Value())});
    for (std::size_t i = 0; i < badges.size(); ++i)
    {
        const auto &badge = badges[i];
        const float x = 636 + static_cast<float>(i % 2) * 342, y = 726 + static_cast<float>(i / 2) * 72;
        auto &icon = CreatePanel(root, Bounds({x, y, 44, 44}), std::string("Result.Option.") + badge.key);
        auto &visual = *icon.GetComponent<SpriteVisualComponent>();
        visual.SetTint(Accent);
        visual.SetCornerRadius(10);
        Label(root, {x, y, 44, 44}, badge.symbol, 22, 800, TextAlignment::Center, ResultColor(0xFFFFFF));
        Label(root, {x + 54, y, 278, 20}, std::wstring(texts_.Get(std::string("PlayOptions.") + badge.key)), 12, 700);
        Label(root, {x + 54, y + 23, 278, 32}, badge.value, 12, 600);
    }
}
