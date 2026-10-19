#include "../../EditorSupport.h"
#include "TaikoEditorMode.h"
#include "Parsing/ChartParser.h"
#include <algorithm>
using namespace editor_ui;

#include <sstream>

void TaikoEditorMode::DrawMetadata(IEditorModeCanvas &canvas, IEditorContext &state,
                                   const finger_drum::texts::TextCatalog &catalog)
{
    const auto &text = finger_drum::texts::Editor(catalog);
    canvas.Text({136, 105, 1000, 40}, std::wstring(text.metadataTitle), 28);
    canvas.Box({132, 173, 1728, 800}, White, 12);
    auto metadata =
        [&state, &canvas, &catalog](float y, const wchar_t *title, std::string value,
                                    std::function<void(chart::PatternDocument &, const std::string &)> apply)
    {
        canvas.Text({156, y, 360, 40}, title, 22);
        canvas.Button({520, y, 1295, 44}, Wide(value),
                      [&state, &catalog, title, value, apply]
                      {
                          if (auto edited = EditText(title, value, finger_drum::texts::Editor(catalog)))
                          {
                              auto p = state.Document().Pattern();
                              apply(p, *edited);
                              state.Replace(p, state.Document().Effects());
                              state.RequestRebuild();
                          }
                      });
    };
    const auto &p = state.Document().Pattern();
    metadata(225, text.ymmRelativePath.data(), Utf8(p.musicMetadataFile.wstring()),
             [](auto &d, const auto &value) { d.musicMetadataFile = std::filesystem::path(Wide(value)); });
    metadata(295, text.patternName.data(), p.name, [](auto &d, const auto &value) { d.name = value; });
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
    metadata(365, text.makers.data(), join(p.makers), [lines](auto &d, const auto &value) { d.makers = lines(value); });
    metadata(435, text.tags.data(), join(p.tags), [lines](auto &d, const auto &value) { d.tags = lines(value); });
    metadata(505, text.offsetMilliseconds.data(), std::to_string(p.patternOffsetMilliseconds),
             [](auto &d, const auto &value) { d.patternOffsetMilliseconds = Number(value); });
    metadata(575, text.baseBpm.data(), std::to_string(p.baseBpm),
             [](auto &d, const auto &value) { d.baseBpm = Number(value); });
    metadata(645, catalog.Get("Editor.accuracyDifficulty").data(), p.judgementLevel.Format(),
             [](auto &d, const auto &value) { d.judgementLevel = editor_values::JudgeLevel(value); });
    std::string sounds;
    for (const auto &[id, path] : state.Document().Effects().hitSounds)
        sounds += id + ": " + Utf8(path.wstring()) + "\n";
    canvas.Text({156, 715, 360, 40}, text.hitSoundTable.data(), 22);
    canvas.Button({520, 715, 1295, 44}, Wide(sounds),
                  [&state, &catalog, sounds]
                  {
                      const auto &labels = finger_drum::texts::Editor(catalog);
                      if (auto edited = EditText(labels.hitSoundTable.data(), sounds, labels))
                      {
                          const auto parsed = chart::ChartParser{}.ParseEffect("[HitSounds]\n" + *edited);
                          if (!parsed.Succeeded())
                              throw std::invalid_argument("Invalid YME hit sound table.");
                          auto effects = state.Document().Effects();
                          effects.hitSounds = parsed.document.hitSounds;
                          state.Replace(state.Document().Pattern(), std::move(effects));
                          state.RequestRebuild();
                      }
                  });
    metadata(785, catalog.Get("Editor.backgroundFile").data(), Utf8(p.backgroundFile.wstring()),
             [](auto &d, const auto &value) { d.backgroundFile = std::filesystem::path(Wide(value)); });
    metadata(855, catalog.Get("Editor.backgroundOffset").data(),
             std::to_string(p.backgroundOffsetX) + ", " + std::to_string(p.backgroundOffsetY),
             [](auto &d, const auto &value)
             {
                 const auto parsed = chart::ChartParser{}.ParsePattern("[Metadata]\noffset: " + value);
                 if (!parsed.Succeeded())
                     throw std::invalid_argument("Background offset requires x, y numbers.");
                 d.backgroundOffsetX = parsed.document.backgroundOffsetX;
                 d.backgroundOffsetY = parsed.document.backgroundOffsetY;
             });
    canvas.Text({156, 925, 1640, 42}, std::wstring(text.metadataHelp), 17);
}
