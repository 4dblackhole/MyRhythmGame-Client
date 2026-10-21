#include "../../EditorSupport.h"
#include "TaikoEditorMode.h"
#include "Parsing/ChartParser.h"
#include "Texts/EditorScene/Taiko/TaikoEditorTexts.h"
#include <algorithm>
using namespace editor_ui;

#include "App/AssetPaths.h"
#include "Taiko/Submodules/TaikoAudioPreview.h"
#include "Taiko/Submodules/TaikoChartAudio.h"

std::map<std::string, std::filesystem::path> TaikoEditorMode::AudioFiles(
    const IEditorDocument &document, const GameplayLaunchRequest &request) const
{
    // YMP metadata references are relative to Songs, not the YMP folder.
    const auto root = request.songsRoot.empty() ? UserSongs() : request.songsRoot;
    const auto metadataPath = root / document.Pattern().musicMetadataFile;
    const auto music = ChartParser{}.ParseMusicFile(metadataPath);
    if (!music.Succeeded())
        throw std::runtime_error("Audio analysis: the selected YMM could not be parsed.");
    std::map<std::string, std::filesystem::path> files{
        {"Music", metadataPath.parent_path() / music.document.audioFile}};
    for (const auto &[id, file] : std::array<std::pair<const char *, const wchar_t *>, 4>{
             {{DonHit, L"don.wav"},
              {KatHit, L"kat.wav"},
              {BigDonFirstHit, L"bigdon.wav"},
              {BigKatFirstHit, L"bigkat.wav"}}})
        files[id] = TaikoHitSound(file);
    for (const auto &[id, path] : document.Effects().hitSounds)
        files[ChartSoundId(id)] = document.Effects().sourcePath.parent_path() / path;
    return files;
}

std::vector<AudioMarker> TaikoEditorMode::AudioMarkers(const IEditorDocument &document) const
{
    std::vector<AudioMarker> markers;
    for (const auto &cue : BuildTaikoAudioPreview(document.Pattern(), document.Effects(),
                                                                     document.Timeline(), document.Notes()))
        markers.push_back({cue.time.count() / 1e6, cue.sound});
    return markers;
}

std::vector<EditorSoundChoice> TaikoEditorMode::SoundChoices(const TextCatalog &catalog) const
{
    const auto &text = TaikoEditor(catalog);
    return {{1, std::wstring(text.don), std::wstring(text.donSound)},
            {2, std::wstring(text.kat), std::wstring(text.katSound)}};
}
std::wstring_view TaikoEditorMode::SoundEffectHelp(const TextCatalog &catalog) const
{
    return TaikoEditor(catalog).effectsHelp;
}
