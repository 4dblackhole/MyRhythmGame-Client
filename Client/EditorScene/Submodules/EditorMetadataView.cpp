#include "EditorView.h"
#include <algorithm>
#include <sstream>
using namespace editor_ui;

void EditorView::DrawMetadata()
{
    Text({136, 105, 1000, 40}, L"메타데이터", 28);
    Box({132, 173, 1728, 800}, White, 12);
    auto metadata =
        [this](float y, const wchar_t *title, std::string value,
               std::function<void(chart::PatternDocument &, const std::string &)> apply) {
            Text({156, y, 360, 40}, title, 22);
            Button({520, y, 1295, 44}, Wide(value), [this, title, value, apply] {
                if (auto edited = EditText(title, value))
                {
                    auto p = state_.editor->Pattern();
                    apply(p, *edited);
                    state_.editor->Replace(p, state_.editor->Effects());
                    state_.analysis.Invalidate();
                    state_.rebuild = true;
                }
            });
        };
    const auto &p = state_.editor->Pattern();
    metadata(225, L"YMM 상대 경로", Utf8(p.musicMetadataFile.wstring()),
             [](auto &d, const auto &value) {
                 d.musicMetadataFile = std::filesystem::path(Wide(value));
             });
    metadata(305, L"패턴 이름", p.name, [](auto &d, const auto &value) { d.name = value; });
    auto join = [](const std::vector<std::string> &values) {
        std::string r;
        for (const auto &s : values)
        {
            if (!r.empty())
                r += '\n';
            r += s;
        }
        return r;
    };
    auto lines = [](const std::string &value) {
        std::vector<std::string> r;
        std::istringstream in(value);
        std::string s;
        while (std::getline(in, s))
        {
            if (!s.empty() && s.back() == '\r')
                s.pop_back();
            if (!s.empty())
                r.push_back(s);
        }
        return r;
    };
    metadata(385, L"제작자 (한 줄에 한 명)", join(p.makers),
             [lines](auto &d, const auto &value) { d.makers = lines(value); });
    metadata(465, L"태그 (한 줄에 하나)", join(p.tags),
             [lines](auto &d, const auto &value) { d.tags = lines(value); });
    metadata(545, L"오프셋 (ms)", std::to_string(p.patternOffsetMilliseconds),
             [](auto &d, const auto &value) { d.patternOffsetMilliseconds = Number(value); });
    metadata(625, L"기본 BPM", std::to_string(p.baseBpm),
             [](auto &d, const auto &value) { d.baseBpm = Number(value); });
    std::string sounds;
    for (const auto &[id, path] : p.hitSounds)
        sounds += id + ": " + Utf8(path.wstring()) + "\n";
    metadata(705, L"히트사운드 테이블", sounds, [](auto &d, const auto &value) {
        const auto result = chart::ChartParser{}.ParsePattern("[HitSounds]\n" + value);
        if (!result.Succeeded())
            throw std::invalid_argument("Invalid hit sound table.");
        d.hitSounds = result.document.hitSounds;
    });
    Text({156, 805, 1640, 95},
         L"히트사운드는 인덱스: 상대경로로 등록합니다. 예: 1: Sounds/pop.wav\nYMP와 같은 폴더에 "
         L"같은 이름의 YME를 "
         L"저장합니다. Ctrl+S: 저장",
         21);
}
