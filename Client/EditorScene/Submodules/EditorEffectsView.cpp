#include "EditorView.h"
#include <algorithm>
#include <sstream>
using namespace editor_ui;

void EditorView::DrawEffects()
{
    Text({136, 105, 1000, 40}, L"이펙트", 28);
    Box({132, 173, 1728, 310}, White, 12);
    Box({132, 503, 1728, 365}, White, 12);
    Text({156, 190, 1650, 38}, L"시작 마디 / 박자      종류           값 / 참조 인덱스", 22);
    const auto &e = state_.editor->Effects();
    const auto total = e.commands.size() + e.hitSoundChanges.size();
    static constexpr const wchar_t *names[]{L"볼륨",        L"마디선",    L"노트 속도",
                                            L"스크롤 속도", L"동 사운드", L"캇 사운드"};
    for (std::size_t i = state_.listOffset; i < total && i < state_.listOffset + 4; ++i)
    {
        const float y = 245 + static_cast<float>(i - state_.listOffset) * 48;
        std::wstring text;
        if (i < e.commands.size())
        {
            const auto &c = e.commands[i];
            text = std::to_wstring(c.position.measure + 1) + L" / " +
                   Wide(Fraction(c.position.fraction)) + L"    " +
                   std::to_wstring(static_cast<int>(c.type)) + L"    " +
                   std::to_wstring(c.beginValue) + L" → " + std::to_wstring(c.endValue);
        }
        else
        {
            const auto &c = e.hitSoundChanges[i - e.commands.size()];
            text = std::to_wstring(c.position.measure + 1) + L" / " +
                   Wide(Fraction(c.position.fraction)) +
                   (c.keyType == 1 ? L"    동    " : L"    캇    ") + Wide(c.soundIndex);
        }
        Button({156, y, 1430, 40}, text, [this, i] {
            const auto &effects = state_.editor->Effects();
            state_.effectFields[2].clear();
            state_.effectFields[3].clear();
            if (i < effects.commands.size())
            {
                const auto &c = effects.commands[i];
                if (c.type == chart::EffectCommandType::BusVolume)
                    state_.effectType = 0;
                else if (c.type == chart::EffectCommandType::MeasureLineVisible)
                    state_.effectType = 1;
                else if (c.type == chart::EffectCommandType::NoteSpeed)
                    state_.effectType = 2;
                else if (c.type == chart::EffectCommandType::ScrollSpeed)
                    state_.effectType = 3;
                else
                    throw std::runtime_error(
                        "This legacy effect is preserved; its editor is not part of this task.");
                state_.effectFields[0] = std::to_string(c.position.measure + 1);
                state_.effectFields[1] = Fraction(c.position.fraction);
                state_.effectFields[4] = std::to_string(c.beginValue);
                state_.effectFields[5] = std::to_string(c.endValue);
                state_.effectFields[6] = c.target;
                state_.curve = static_cast<int>(c.curve);
                if (c.endPosition)
                {
                    state_.effectFields[2] = std::to_string(c.endPosition->measure + 1);
                    state_.effectFields[3] = Fraction(c.endPosition->fraction);
                }
            }
            else
            {
                const auto &c = effects.hitSoundChanges[i - effects.commands.size()];
                state_.effectType = c.keyType == 1 ? 4 : 5;
                state_.effectFields[0] = std::to_string(c.position.measure + 1);
                state_.effectFields[1] = Fraction(c.position.fraction);
                state_.effectFields[4] = c.soundIndex;
            }
            state_.rebuild = true;
        });
        Button({1620, y, 195, 40}, L"삭제", [this, i] {
            auto effects = state_.editor->Effects();
            if (i < effects.commands.size())
                effects.commands.erase(effects.commands.begin() + i);
            else
                effects.hitSoundChanges.erase(effects.hitSoundChanges.begin() +
                                              (i - effects.commands.size()));
            state_.editor->Replace(state_.editor->Pattern(), effects);
            state_.rebuild = true;
        });
    }
    Field({156, 595, 188, 42}, L"시작 마디", state_.effectFields[0]);
    Field({370, 595, 188, 42}, L"시작 박자", state_.effectFields[1]);
    Field({584, 595, 188, 42}, L"끝 마디 (선택)", state_.effectFields[2]);
    Field({798, 595, 188, 42}, L"끝 박자 (선택)", state_.effectFields[3]);
    Button({1036, 595, 789, 42}, names[state_.effectType], [this] {
        state_.effectType = (state_.effectType + 1) % 6;
        state_.rebuild = true;
    });
    Field({156, 718, 319, 42},
          state_.effectType >= 4 ? L"히트사운드 인덱스" : L"시작 값 (마디선 0/1)",
          state_.effectFields[4]);
    Field({500, 718, 319, 42}, L"끝 값", state_.effectFields[5]);
    static constexpr const wchar_t *curves[]{L"Step", L"Linear", L"Smoothstep", L"Exponential"};
    Button({844, 718, 360, 42}, curves[state_.curve], [this] {
        state_.curve = (state_.curve + 1) % 4;
        state_.rebuild = true;
    });
    Field({1230, 718, 245, 42}, L"오디오 버스", state_.effectFields[6]);
    Button(
        {1508, 718, 317, 42}, L"추가 / 같은 위치 수정",
        [this] {
            auto effects = state_.editor->Effects();
            const auto p = Position(state_.effectFields[0], state_.effectFields[1]);
            if (state_.effectType >= 4)
            {
                const int key = state_.effectType == 4 ? 1 : 2;
                std::erase_if(effects.hitSoundChanges,
                              [&](const auto &c) { return c.position == p && c.keyType == key; });
                effects.hitSoundChanges.push_back({p, state_.effectFields[4], key});
            }
            else
            {
                constexpr chart::EffectCommandType types[]{
                    chart::EffectCommandType::BusVolume,
                    chart::EffectCommandType::MeasureLineVisible,
                    chart::EffectCommandType::NoteSpeed, chart::EffectCommandType::ScrollSpeed};
                chart::EffectCommand c;
                c.position = p;
                c.type = types[state_.effectType];
                c.target = state_.effectType == 0 ? state_.effectFields[6] : "";
                c.beginValue = Number(state_.effectFields[4]);
                c.endValue = Number(state_.effectFields[5]);
                c.curve = static_cast<chart::AutomationCurve>(state_.curve);
                if (!state_.effectFields[2].empty() || !state_.effectFields[3].empty())
                    c.endPosition = Position(state_.effectFields[2], state_.effectFields[3]);
                else
                    c.endValue = c.beginValue;
                if (state_.effectType >= 2 && (c.beginValue <= 0 || c.endValue <= 0))
                    throw std::invalid_argument("Speed must be positive.");
                std::erase_if(effects.commands, [&](const auto &old) {
                    return old.position == p && old.type == c.type && old.target == c.target;
                });
                effects.commands.push_back(std::move(c));
            }
            state_.editor->Replace(state_.editor->Pattern(), effects);
            state_.rebuild = true;
        },
        true);
    Text({156, 800, 1620, 50},
         L"동·캇은 독립 설정입니다. 둘 다 변경하려면 같은 위치에 각각 추가하세요. 변경 값은 다음 "
         L"지시까지 유지합니다.",
         18);
}
