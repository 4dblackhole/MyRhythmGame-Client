#include "MusicSelectScene.h"
#include "App/AssetPaths.h"
#include "GameFlow/YOUMUSceneIds.h"
#include "Mode/PlayGameModeFactory.h"
#include "Submodules/MusicSelectView.h"
#include "Submodules/NewDifficultyDialog.h"
#include "Submodules/SongPreviewController.h"
#include "Submodules/SongSelectionText.h"
#include <stdexcept>

using namespace song_select;

MusicSelectScene::MusicSelectScene(ScreenVisual2DManager &visuals,
                                   AudioPlaybackManager &playback,
                                   std::shared_ptr<GameplayLaunchStore> request, SongSelectPurpose purpose,
                                   TextCatalog &texts, OptionSettings &options)
    : launchRequest_(std::move(request)), purpose_(purpose), options_(options), texts_(texts),
      selection_(std::make_unique<SongSelectionState>()),
      view_(std::make_unique<MusicSelectView>(visuals, *selection_, purpose, texts, options)),
      preview_(std::make_unique<SongPreviewController>(playback))
{
}
MusicSelectScene::~MusicSelectScene() = default;

void MusicSelectScene::Initialize(const EngineServices &services)
{
    if (!launchRequest_)
        throw std::invalid_argument("Lobby requires a gameplay launch request.");
    preview_->Initialize(services.audio);
    RefreshCatalog();
    view_->Initialize(services);
}
void MusicSelectScene::BeginScene()
{
    // Initialize started the first load. Re-entry still observes user edits.
    if (!firstEntry_ && !osuImport_.Busy())
    {
        RefreshCatalog();
    }
    firstEntry_ = false;
    view_->RebuildVisibleSongs();
    active_ = true;
    view_->SetVisible(true);
    preview_->SyncPreviewToFocusedSong(*selection_, active_);
}
void MusicSelectScene::EndScene() noexcept
{
    active_ = false;
    pendingLaunch_.reset();
    preview_->StopPreviewAudio();
    view_->SetVisible(false);
}
void MusicSelectScene::Update(const UpdateContext &context, SceneManager &scenes)
{
    if (catalogRoot_ != options_.SongsRoot())
        RefreshCatalog();
    PollOsuImport();
    if (auto catalog = catalogLoader_.Poll())
    {
        preview_->StopPreviewAudio();
        selection_->catalogLoading_ = false;
        selection_->catalog_ = std::move(*catalog);
        selection_->ClearLaunchError();
        if (selection_->catalog_.songs.empty() && !selection_->catalog_.diagnostics.empty())
            selection_->SetLaunchError(SongSelectionState::LaunchErrorKind::Catalog,
                                       selection_->catalog_.diagnostics.front().message);
        view_->RebuildVisibleSongs();
    }
    preview_->UpdatePreviewAudio(context.deltaSeconds);
    const auto command = view_->Update(context.input, context.deltaSeconds);
    preview_->SyncPreviewToFocusedSong(*selection_, active_);
    if (command == SongSelectCommand::Back)
    {
        if (!scenes.ChangeScene(LogoSceneId))
            throw std::runtime_error("Failed to return to the title Scene.");
    }
    else if (command == SongSelectCommand::Launch)
        StartSelectedPattern(scenes);
    else if (command == SongSelectCommand::CreateDifficulty)
        CreateDifficulty(scenes);
    else if (command == SongSelectCommand::ConvertOsu)
        ConvertOsu();
    if (command != SongSelectCommand::Back)
        PollPatternLoad(scenes);
}
void MusicSelectScene::ConvertOsu()
{
    if (purpose_ != SongSelectPurpose::Editor || selection_->catalogLoading_ || osuImport_.Busy()) return;
    preview_->StopPreviewAudio();
    try
    {
        if (auto input = SelectOsuFile(texts_))
        {
            osuImport_.Start(*input, catalogRoot_);
            selection_->catalogLoading_ = true;
            selection_->ClearLaunchError();
            view_->RebuildVisibleSongs();
        }
    }
    catch (const std::exception &error)
    {
        selection_->SetLaunchError(SongSelectionState::LaunchErrorKind::Pattern, error.what());
        view_->RefreshSelectionPresentation();
    }
}
void MusicSelectScene::PollOsuImport()
{
    const auto result = osuImport_.Poll();
    if (!result) return;
    if (result->success)
    {
        RefreshCatalog();
        ShowOsuImportCompletion(*result, texts_);
    }
    else
    {
        selection_->catalogLoading_ = false;
        selection_->SetLaunchError(SongSelectionState::LaunchErrorKind::Pattern, result->message);
        view_->RebuildVisibleSongs();
    }
}
void MusicSelectScene::CreateDifficulty(SceneManager &scenes)
{
    if (purpose_ != SongSelectPurpose::Editor || selection_->catalogLoading_ ||
        selection_->focusedSongPosition_ >= selection_->visibleSongIndices_.size())
        return;
    auto &song = selection_->catalog_.songs[selection_->visibleSongIndices_[selection_->focusedSongPosition_]];
    preview_->StopPreviewAudio();
    try
    {
        if (auto created = CreateDifficultyWithDialog(catalogRoot_, song, texts_))
        {
            song.patterns.push_back(std::move(*created));
            selection_->selectedPatternIndex_ = song.patterns.size() - 1;
            StartSelectedPattern(scenes);
        }
    }
    catch (const std::exception &error)
    {
        selection_->SetLaunchError(SongSelectionState::LaunchErrorKind::Pattern, error.what());
        view_->RefreshSelectionPresentation();
    }
}
void MusicSelectScene::Render(const RenderContext &)
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

bool MusicSelectScene::StartSelectedPattern(SceneManager &scenes)
{
    if (selection_->catalogLoading_ || patternLoader_.Busy())
        return false;
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

    // Validate both entry routes before a Scene factory can reject the request.
    if (pattern.pattern.mode != "Taiko")
    {
        selection_->SetLaunchError(SongSelectionState::LaunchErrorKind::UnsupportedMode,
                                   pattern.pattern.mode);
        view_->RefreshSelectionPresentation();
        return false;
    }

    GameplayLaunchRequest request{
        pattern.patternPath, pattern.effectPath,
        song.audioPath,      pattern.pattern.mode,
        SongTitle(song),     pattern.pattern.makers.empty() ? L"" : DecodeDisplayText(pattern.pattern.makers.front())};
    request.globalOffsetMs = options_.Values().globalOffsetMs;
    request.playOptions = selection_->playOptions_;
    request.songsRoot = catalogRoot_;
    if (!pattern.pattern.backgroundFile.empty())
        request.backgroundPath = catalogRoot_ / L"BG" / pattern.pattern.backgroundFile;
    request.backgroundOffsetX = pattern.pattern.backgroundOffsetX;
    request.backgroundOffsetY = pattern.pattern.backgroundOffsetY;
    request.songArtist = SongArtist(song);
    if (const auto keys = options_.Values().modeKeys.find(request.mode); keys != options_.Values().modeKeys.end())
        request.inputKeys = keys->second;
    if (purpose_ == SongSelectPurpose::Editor)
    {
        selection_->ClearLaunchError();
        launchRequest_->Set(request);
        if (!scenes.ChangeScene(EditorSceneId))
        {
            throw std::runtime_error("Failed to enter the editor scene.");
        }
        return true;
    }

    selection_->ClearLaunchError();
    pendingLaunch_ = request;
    patternLoader_.Start(std::move(request));
    return true;
}

void MusicSelectScene::PollPatternLoad(SceneManager &scenes)
{
    auto loaded = patternLoader_.Poll();
    if (!loaded || !pendingLaunch_)
        return;
    auto request = std::move(*pendingLaunch_);
    pendingLaunch_.reset();
    // Navigation remains responsive. A different selection cancels this launch.
    if (selection_->focusedSongPosition_ >= selection_->visibleSongIndices_.size())
        return;
    const auto &song = selection_->catalog_.songs[selection_->visibleSongIndices_[selection_->focusedSongPosition_]];
    if (selection_->selectedPatternIndex_ >= song.patterns.size() ||
        song.patterns[selection_->selectedPatternIndex_].patternPath != request.patternPath)
        return;
    if (!loaded->Succeeded())
    {
        selection_->SetLaunchError(SongSelectionState::LaunchErrorKind::Pattern,
            loaded->diagnostics.empty() ? std::string{} : loaded->diagnostics.front().message);
        view_->RefreshSelectionPresentation();
        return;
    }
    launchRequest_->SetValidated(std::move(request), std::move(loaded->session));
    if (!scenes.ChangeScene(RhythmTestSceneId))
    {
        throw std::runtime_error("Failed to enter transient gameplay.");
    }
}

void MusicSelectScene::RefreshCatalog()
{
    pendingLaunch_.reset();
    catalogRoot_ = options_.SongsRoot();
    preview_->StopPreviewAudio();
    selection_->catalogLoading_ = true;
    selection_->catalog_ = {};
    catalogLoader_.Refresh(catalogRoot_);
    if (active_)
        view_->RebuildVisibleSongs();
}
