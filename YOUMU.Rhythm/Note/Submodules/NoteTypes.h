#pragma once

#include "Common/RhythmTypes.h"
#include "Judgement/AccuracyRange.h"
#include "Judgement/AccuracyScore.h"
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

enum class NoteState : std::uint8_t
{
    Pending,
    Active,
    AwaitingAdditionalInput,
    Holding,
    Completed,
    Missed,
};

enum class InputEdge : std::uint8_t
{
    Pressed,
    Released,
};

struct RhythmInputEvent
{
    RhythmTime time{};
    NoteAction action{};
    PhysicalKey physicalKey{};
    InputEdge edge{InputEdge::Pressed};
    // Supplied by the input-state owner after applying this physical edge.
    // Direct rule users may instead let the rule track their local keys.
    std::optional<bool> actionHeldAfter;
};

enum class NoteEventType : std::uint8_t
{
    InputRejected,
    HitAccepted,
    StageAdvanced,
    HoldStarted,
    HoldReleased,
    TickAccepted,
    TickMissed,
    Completed,
    Missed,
};

struct NoteEvent
{
    NoteId noteId{};
    NoteEventType type{};
    NoteState stateBefore{NoteState::Pending};
    NoteState stateAfter{NoteState::Pending};
    JudgementResult judgement{};
    RhythmTime eventTime{};
    std::size_t hitIndex{};
    std::size_t tickIndex{};
    std::optional<NoteAction> inputAction;
};

struct AudioCueRequest
{
    SoundId sound;
    AudioBusId bus{"HitSound"};
    RhythmTime timelineTime{};
    float volume{1.0F};
    float pitch{1.0F};
    float pan{};
    float reverbSend{};
    int priority{};
};

enum class NoteAccuracyKind : std::uint8_t
{
    TimingHits,
    HitCount,
    Ticks,
    Hold,
};

struct NoteAccuracyTarget
{
    NoteAccuracyKind kind{NoteAccuracyKind::TimingHits};
    std::size_t hits{1};
    std::size_t ticks{};
};

// A logical note contributes once, independently of its number of inputs.
struct NoteAccuracy
{
    NoteId noteId{};
    NoteAccuracyTarget target;
    std::vector<double> hitScoreRates;
    std::size_t acceptedHits{};
    std::size_t acceptedTicks{};
    bool finalized{};

    [[nodiscard]] AccuracyScore Score() const noexcept;
    [[nodiscard]] double ScoreRate() const noexcept { return Score().Rate(); }
#if defined(_DEBUG)
    [[nodiscard]] std::wstring DebugText() const;
#endif
};

struct NoteProcessResult
{
    std::vector<NoteEvent> events;
    std::vector<AudioCueRequest> audioCues;
    std::vector<NoteAccuracy> finalizedAccuracies;

    void Append(NoteProcessResult other);
};
