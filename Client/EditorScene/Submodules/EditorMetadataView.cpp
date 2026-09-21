#include "EditorView.h"
#include <algorithm>
#include <sstream>
using namespace editor_ui;

void EditorView::DrawMetadata()
{
    const auto &text = Texts();
    Text({136, 105, 1000, 40}, std::wstring(text.metadataTitle), 28);
    Box({132, 173, 1728, 800}, White, 12);
    auto metadata =
        [this](float y, const wchar_t *title, std::string value,
               std::function<void(chart::PatternDocument &, const std::string &)> apply) {
            Text({156, y, 360, 40}, title, 22);
            Button({520, y, 1295, 44}, Wide(value), [this, title, value, apply] {
                if (auto edited = EditText(title, value, Texts()))
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
    metadata(225, text.ymmRelativePath.data(), Utf8(p.musicMetadataFile.wstring()),
             [](auto &d, const auto &value) {
                 d.musicMetadataFile = std::filesystem::path(Wide(value));
             });
    metadata(305, text.patternName.data(), p.name,
             [](auto &d, const auto &value) { d.name = value; });
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
    metadata(385, text.makers.data(), join(p.makers),
             [lines](auto &d, const auto &value) { d.makers = lines(value); });
    metadata(465, text.tags.data(), join(p.tags),
             [lines](auto &d, const auto &value) { d.tags = lines(value); });
    metadata(545, text.offsetMilliseconds.data(), std::to_string(p.patternOffsetMilliseconds),
             [](auto &d, const auto &value) { d.patternOffsetMilliseconds = Number(value); });
    metadata(625, text.baseBpm.data(), std::to_string(p.baseBpm),
             [](auto &d, const auto &value) { d.baseBpm = Number(value); });
    std::string sounds;
    for (const auto &[id, path] : p.hitSounds)
        sounds += id + ": " + Utf8(path.wstring()) + "\n";
    metadata(705, text.hitSoundTable.data(), sounds, [](auto &d, const auto &value) {
        const auto result = chart::ChartParser{}.ParsePattern("[HitSounds]\n" + value);
        if (!result.Succeeded())
            throw std::invalid_argument("Invalid hit sound table.");
        d.hitSounds = result.document.hitSounds;
    });
    Text({156, 805, 1640, 95}, std::wstring(text.metadataHelp), 21);
}
