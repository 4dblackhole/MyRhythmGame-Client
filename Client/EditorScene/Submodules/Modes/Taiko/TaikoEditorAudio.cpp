#include "../../EditorSupport.h"
#include "TaikoEditorMode.h"
#include "Texts/EditorScene/Taiko/TaikoEditorTexts.h"
#include <algorithm>
using namespace editor_ui;

#include "App/AssetPaths.h"
#include "Taiko/Submodules/TaikoAudioPreview.h"
#include "Taiko/Submodules/TaikoChartAudio.h"

std::map<std::string, std::filesystem::path> TaikoEditorMode::AudioFiles(
    const chart::IEditorDocument &document, const finger_drum::GameplayLaunchRequest &) const
{
    // YMP metadata references are relative to Songs, not the YMP folder.
    const auto metadataPath = mrg_client::asset_paths::UserSongs() / document.Pattern().musicMetadataFile;
    const auto music = chart::ChartParser{}.ParseMusicFile(metadataPath);
    if (!music.Succeeded())
        throw std::runtime_error("Audio analysis: the selected YMM could not be parsed.");
    std::map<std::string, std::filesystem::path> files{
        {"Music", metadataPath.parent_path() / music.document.audioFile}};
    for (const auto &[id, file] : std::array<std::pair<const char *, const wchar_t *>, 4>{
             {{finger_drum::mode::taiko_sound::DonHit, L"don.wav"},
              {finger_drum::mode::taiko_sound::KatHit, L"kat.wav"},
              {finger_drum::mode::taiko_sound::BigDonFirstHit, L"bigdon.wav"},
              {finger_drum::mode::taiko_sound::BigKatFirstHit, L"bigkat.wav"}}})
        files[id] = mrg_client::asset_paths::skin::TaikoHitSound(file);
    for (const auto &[id, path] : document.Effects().hitSounds)
        files[finger_drum::mode::taiko_audio::ChartSoundId(id)] = document.Effects().sourcePath.parent_path() / path;
    return files;
}

std::vector<AudioMarker> TaikoEditorMode::AudioMarkers(const chart::IEditorDocument &document) const
{
    std::vector<AudioMarker> markers;
    for (const auto &cue : finger_drum::mode::BuildTaikoAudioPreview(document.Pattern(), document.Effects(),
                                                                     document.Timeline(), document.Notes()))
        markers.push_back({cue.time.count() / 1e6, cue.sound});
    return markers;
}

std::vector<EditorSoundChoice> TaikoEditorMode::SoundChoices(finger_drum::texts::Language language) const
{
    const auto &text = finger_drum::texts::TaikoEditor(language);
    return {{1, std::wstring(text.don), std::wstring(text.donSound)},
            {2, std::wstring(text.kat), std::wstring(text.katSound)}};
}
std::wstring_view TaikoEditorMode::SoundEffectHelp(finger_drum::texts::Language language) const
{
    return finger_drum::texts::TaikoEditor(language).effectsHelp;
}
