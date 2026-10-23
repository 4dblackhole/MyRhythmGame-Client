#include "App/AssetPaths.h"
#include "GameFlow/GameplaySessionController.h"
#include "Mode/PlayGameModeFactory.h"
#include "Taiko/TaikoMode.h"
#include <stdexcept>
namespace
{
    void AddPatternNote(PatternDocument &pattern, const std::int64_t measure,
                        const std::int64_t numerator, const std::int64_t denominator,
                        const TaikoNoteType type, const TaikoPatternAction action,
                        std::vector<std::string> extraData = {})
    {
        PatternNote note;
        note.position =
            MusicalPosition{measure, Rational{numerator, denominator}};
        note.keyType = static_cast<int>(type);
        note.actionType = static_cast<int>(action);
        note.extraData = std::move(extraData);
        note.sourceOrder = pattern.notes.size();
        pattern.notes.push_back(std::move(note));
    }

} // namespace

std::unique_ptr<PlaySession> GameplaySessionController::CreateSession()
{
    if (!launchRequest_.IsValid())
    {
        return debugMode_ ? CreateLongNoteDebugSession() : CreateDemoSession();
    }
    auto mode = CreatePlayGameMode(
        launchRequest_.mode,
        {RhythmDuration{static_cast<std::int64_t>(launchRequest_.globalOffsetMs) * 1000},
         launchRequest_.inputKeys, launchRequest_.playOptions});
    ModeLoadResult loaded = mode->LoadSession(launchRequest_.patternPath, launchRequest_.effectPath);
    if (!loaded.Succeeded())
    {
        const std::string detail =
            loaded.diagnostics.empty() ? "Unknown chart error." : loaded.diagnostics.front().message;
        throw std::runtime_error("Failed to load the selected pattern: " + detail);
    }
    return std::move(loaded.session);
}

std::unique_ptr<PlaySession> GameplaySessionController::CreateDemoSession()
{
    PatternDocument pattern;
    pattern.name = "Architecture test";
    pattern.mode = "Taiko";
    pattern.baseBpm = 120.0;
    pattern.judgementLevel = 50;

    using NoteType = TaikoNoteType;
    using Action = TaikoPatternAction;
    AddPatternNote(pattern, 0, 0, 1, NoteType::Don, Action::Down);
    AddPatternNote(pattern, 0, 1, 4, NoteType::Kat, Action::Down);
    AddPatternNote(pattern, 0, 2, 4, NoteType::BigDon, Action::Down);
    AddPatternNote(pattern, 0, 3, 4, NoteType::BigKat, Action::Down);
    AddPatternNote(pattern, 1, 0, 1, NoteType::Roll, Action::LongNoteStart);
    AddPatternNote(pattern, 2, 0, 1, NoteType::Roll, Action::LongNoteEnd);
    AddPatternNote(pattern, 2, 1, 4, NoteType::TickRoll, Action::LongNoteStart);
    AddPatternNote(pattern, 3, 0, 1, NoteType::TickRoll, Action::LongNoteEnd);

    TaikoMode mode;
    ModeLoadResult result = mode.CreateSession(pattern);
    if (!result.Succeeded())
    {
        throw std::runtime_error("Failed to create the Taiko demo session.");
    }
    return std::move(result.session);
}

std::unique_ptr<PlaySession> GameplaySessionController::CreateLongNoteDebugSession()
{
    PatternDocument pattern;
    pattern.name = "Long Notes Debug";
    pattern.mode = "Taiko";
    pattern.baseBpm = 180.0;
    pattern.patternOffsetMilliseconds = 104.0;
    pattern.judgementLevel = 50;

    using NoteType = TaikoNoteType;
    using Action = TaikoPatternAction;
    const auto addLong = [&pattern](const std::int64_t measure, const std::int64_t startNumerator, const NoteType type,
                                    std::vector<std::string> extraData = {})
    {
        AddPatternNote(pattern, measure, startNumerator, 4, type, Action::LongNoteStart, std::move(extraData));
        AddPatternNote(pattern, measure, startNumerator + 1, 4, type, Action::LongNoteEnd);
    };

    addLong(0, 0, NoteType::Roll);
    addLong(0, 2, NoteType::BigRoll);
    addLong(1, 0, NoteType::TickRoll, {"TickDivision=16"});
    addLong(1, 2, NoteType::BigTickRoll, {"TickDivision=16"});
    addLong(2, 0, NoteType::Balloon, {"8"});
    addLong(2, 2, NoteType::DengDeng, {"HitCount=8"});
    addLong(3, 0, NoteType::Buzz, {"Action=Don", "TickDivision=16"});
    addLong(3, 2, NoteType::Buzz, {"Action=Kat", "TickDivision=16"});

    TaikoMode mode;
    ModeLoadResult result = mode.CreateSession(pattern);
    if (!result.Succeeded())
    {
        throw std::runtime_error("Failed to create the built-in long-note debug session.");
    }
    return std::move(result.session);
}

void GameplaySessionController::PrepareDebugLaunchRequest()
{
    if (!debugMode_ || launchRequest_.IsValid())
    {
        return;
    }
    const std::filesystem::path songs =
        launchRequest_.songsRoot.empty() ? UserSongs() : launchRequest_.songsRoot;
    const std::filesystem::path patternPath =
        songs / L"Patterns\\angeldream\\Long Notes Debug\\angeldream [long notes test].ymp";
    const std::filesystem::path musicPath = songs / L"Music\\Angeldream\\angel dream hand shaking.mp3";
    if (!std::filesystem::is_regular_file(patternPath) || !std::filesystem::is_regular_file(musicPath))
    {
        return;
    }
    launchRequest_.patternPath = patternPath;
    launchRequest_.musicPath = musicPath;
    launchRequest_.mode = "Taiko";
}
