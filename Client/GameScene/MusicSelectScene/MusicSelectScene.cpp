#include "MusicSelectScene.h"
#include "Submodules/MusicSelectView.h"
#include "Submodules/SongPreviewController.h"
#include "Submodules/SongSelectionText.h"
#include "App/AssetPaths.h"
#include "GameFlow/FingerDrumSceneIds.h"
#include "Taiko/TaikoMode.h"
#include <stdexcept>

using namespace song_select;

MusicSelectScene::MusicSelectScene(mrg::visual2d::ScreenVisual2DManager &visuals,
                                   mrg::audio::AudioPlaybackManager &playback,
                                   std::shared_ptr<finger_drum::GameplayLaunchStore> request,
                                   SongSelectPurpose purpose)
    : launchRequest_(std::move(request)), purpose_(purpose),
      selection_(std::make_unique<SongSelectionState>()),
      view_(std::make_unique<MusicSelectView>(visuals, *selection_, purpose)),
      preview_(std::make_unique<SongPreviewController>(playback))
{
}
MusicSelectScene::~MusicSelectScene() = default;

void MusicSelectScene::Initialize(const mrg::EngineServices &services)
{
    if (!launchRequest_)
        throw std::invalid_argument("Lobby requires a gameplay launch request.");
    selection_->catalog_ =
        finger_drum::chart::SongCatalog{}.Load(mrg_client::asset_paths::UserSongs());
    if (selection_->catalog_.songs.empty() && !selection_->catalog_.diagnostics.empty())
        selection_->launchError_ =
            L"CATALOG ERROR: " +
            DecodeDisplayText(selection_->catalog_.diagnostics.front().message);
    preview_->Initialize(services.audio);
    view_->Initialize(services);
}
void MusicSelectScene::BeginScene()
{
    selection_->catalog_ =
        finger_drum::chart::SongCatalog{}.Load(mrg_client::asset_paths::UserSongs());
    view_->RebuildVisibleSongs();
    active_ = true;
    view_->SetVisible(true);
    preview_->SyncPreviewToFocusedSong(*selection_, active_);
}
void MusicSelectScene::EndScene() noexcept
{
    active_ = false;
    preview_->StopPreviewAudio();
    view_->SetVisible(false);
}
void MusicSelectScene::Update(const mrg::UpdateContext &context, mrg::scene::SceneManager &scenes)
{
    preview_->UpdatePreviewAudio(context.deltaSeconds);
    const auto command = view_->Update(context.input);
    preview_->SyncPreviewToFocusedSong(*selection_, active_);
    if (command == SongSelectCommand::Back)
    {
        if (!scenes.ChangeScene(finger_drum::scene_ids::Logo))
            throw std::runtime_error("Failed to return to the FingerDrum Logo.");
    }
    else if (command == SongSelectCommand::Launch)
        StartSelectedPattern(scenes);
}
void MusicSelectScene::Render(const mrg::graphics::RenderContext &)
{
}
void MusicSelectScene::OnResize(std::uint32_t width, std::uint32_t height)
{
    view_->OnResize(width, height);
}
void MusicSelectScene::Shutdown() noexcept
{
    preview_->StopPreviewAudio();
    view_->Shutdown();
    *selection_ = {};
}

bool MusicSelectScene::StartSelectedPattern(mrg::scene::SceneManager &scenes)
{
    if (selection_->focusedSongPosition_ >= selection_->visibleSongIndices_.size())
    {
        return false;
    }
    const auto &song =
        selection_->catalog_
            .songs[selection_->visibleSongIndices_[selection_->focusedSongPosition_]];
    if (song.patterns.empty() || selection_->selectedPatternIndex_ >= song.patterns.size())
    {
        return false;
    }
    const auto &pattern = song.patterns[selection_->selectedPatternIndex_];

    if (purpose_ == SongSelectPurpose::Editor)
    {
        selection_->launchError_.clear();
        launchRequest_->Set(
            {pattern.patternPath, pattern.effectPath, song.audioPath, pattern.pattern.mode});
        if (!scenes.ChangeScene(finger_drum::scene_ids::Editor))
        {
            throw std::runtime_error("Failed to enter the editor scene.");
        }
        return true;
    }

    if (!pattern.pattern.mode.empty() && pattern.pattern.mode != "Taiko")
    {
        selection_->launchError_ = L"UNSUPPORTED MODE: " + DecodeDisplayText(pattern.pattern.mode);
        view_->RefreshSelectionPresentation();
        return false;
    }

    try
    {
        finger_drum::mode::ModeLoadResult validation =
            finger_drum::mode::TaikoMode{}.LoadSession(pattern.patternPath, pattern.effectPath);
        if (!validation.Succeeded())
        {
            selection_->launchError_ =
                L"PATTERN ERROR: " +
                DecodeDisplayText(validation.diagnostics.empty()
                                      ? std::string_view{"Unknown chart error."}
                                      : std::string_view{validation.diagnostics.front().message});
            view_->RefreshSelectionPresentation();
            return false;
        }
    }
    catch (const std::exception &exception)
    {
        selection_->launchError_ = L"PATTERN ERROR: " + DecodeDisplayText(exception.what());
        view_->RefreshSelectionPresentation();
        return false;
    }

    selection_->launchError_.clear();
    launchRequest_->Set(
        {pattern.patternPath, pattern.effectPath, song.audioPath, pattern.pattern.mode});
    if (!scenes.ChangeScene(finger_drum::scene_ids::RhythmTest))
    {
        throw std::runtime_error("Failed to enter transient gameplay.");
    }
    return true;
}
