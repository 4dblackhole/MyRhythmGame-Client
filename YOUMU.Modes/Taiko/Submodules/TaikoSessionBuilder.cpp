#include "TaikoSessionBuilder.h"
#include "Automation/EffectValidation.h"
#include "Mode/Submodules/PlaySessionTypes.h"
#include "Note/Submodules/CountedHitInputRule.h"
#include "Note/Submodules/RuleBasedNote.h"
#include "Note/Submodules/SequenceInputRule.h"
#include "Note/Submodules/TapInputRule.h"
#include "Taiko/Results/TaikoResultCollector.h"
#include "TaikoBuildSupport.h"
#include "TaikoChartNotes.h"
#include "TaikoNoteTransform.h"

namespace
{
    std::vector<MeasureLinePresentationInfo> BuildMeasureLines(const MusicalTimeline &timeline,
                                                               const EffectLookup &effects)
    {
        std::vector<MeasureLinePresentationInfo> result;
        result.reserve(timeline.MeasureSpans().size());
        for (const auto &span : timeline.MeasureSpans())
            if (span.end > span.begin)
                result.push_back({span.begin, effects.Value(EffectCommandType::NoteSpeed, span.begin)});
        return result;
    }
} // namespace

ModeLoadResult TaikoSessionBuilder::CreateSession(const PatternDocument &pattern,
                                                  const EffectDocument &effects,
                                                  const PlaySessionOptions &options, ModeInputLayout inputLayout)
{
    ModeLoadResult result;
    if (pattern.mode != "Taiko")
    {
        result.diagnostics.push_back({DiagnosticSeverity::Error, {pattern.sourcePath, 1, 1},
                                      "Taiko charts must explicitly specify Mode: Taiko."});
        return result;
    }
    options.play.Validate();
    const auto judgeLevel = options.play.judgeLevel.value_or(pattern.judgementLevel);
    TaikoNoteTransform transform(options.play);
    auto session = std::make_unique<PlaySession>();
    session->SetPlayOptions(options.play, pattern.baseBpm);
    Lane &lane = session->Gear().CreateLane();
    auto defaultRange = std::make_shared<const AccuracyRange>("Taiko.Default", judgeLevel);
    auto syncopationRange = std::make_shared<const AccuracyRange>("Taiko.Syncopation", judgeLevel,
        AccuracyRange::DefaultLevel50Bands(), RhythmDuration{10'000});

    MusicalTimeline timeline(pattern, options.globalOffset);
    ValidateEffectPositions(effects, timeline);
    auto compiledEffects = timeline.CompileEffects(effects);
    if (options.play.ignoreScrollEffects)
        std::erase_if(compiledEffects, [](const auto &effect) {
            return effect.command.type == EffectCommandType::NoteSpeed || effect.command.type == EffectCommandType::ScrollSpeed;
        });
    const EffectLookup effectLookup(compiledEffects);
    auto compiled = PlayableInSourceOrder(timeline.CompileNotes(pattern));
    PlayResultSummary summary;
    summary.patternPath = pattern.sourcePath;
    summary.globalOffset = options.globalOffset;
    summary.patternName = pattern.name;
    summary.judgementLevel = judgeLevel;
    summary.playOptions = options.play;
    auto collector = std::make_unique<TaikoResultCollector>(std::move(summary), compiled.size());
    ConfigureHitSounds(*session, pattern, effects, timeline, result.diagnostics);
    if (!result.diagnostics.empty())
        return result;
    for (auto &note : compiled)
    {
        if (!note.note.hitSound.empty() && !effects.hitSounds.contains(note.note.hitSound))
        {
            result.diagnostics.push_back({DiagnosticSeverity::Error, note.note.source,
                "Note references an undefined YME hit sound index."});
            return result;
        }
        note.scrollMultiplier = effectLookup.Value(EffectCommandType::NoteSpeed, note.timing);
        // Explicit per-note assignments take precedence over timed defaults.
        if (effects.hitSounds.contains(note.note.hitSound))
        {
            note.note.hitSound = ChartSoundId(note.note.hitSound);
        }
    }
    // The Taiko Lane treats a long note as one active head/tail
    // transaction per lane. Keeping that rule prevents a stray second
    // LNStart from replacing the real head before its matching tail.
    std::optional<CompiledPatternNote> longNoteHead;
    NoteId nextId = 1;

    for (const CompiledPatternNote &compiledNote : compiled)
    {
        PatternNote source = compiledNote.note;
        const auto profile =
            UsesSyncopation(effectLookup, timeline, source.position) ? syncopationRange : defaultRange;
        TaikoNoteType type = static_cast<TaikoNoteType>(source.keyType);
        const TaikoPatternAction action = static_cast<TaikoPatternAction>(source.actionType);

        if (action == TaikoPatternAction::LongNoteStart)
        {
            if (!longNoteHead.has_value())
            {
                longNoteHead = compiledNote;
            }
            else
                result.diagnostics.push_back({DiagnosticSeverity::Warning, source.source,
                    "Ignored overlapping long-note start; the active long note keeps its original head."});
            continue;
        }
        if (action == TaikoPatternAction::LongNoteEnd)
        {
            if (!longNoteHead.has_value() || longNoteHead->note.keyType != source.keyType)
            {
                result.diagnostics.push_back({DiagnosticSeverity::Warning, source.source,
                    "Ignored long-note end without a matching active head."});
                continue;
            }
            const auto longNoteId = nextId;
            transform.Apply(longNoteHead->note);
            AddLongNote(*longNoteHead, compiledNote, timeline,
                        UsesSyncopation(effectLookup, timeline, longNoteHead->note.position) ? syncopationRange
                                                                                             : defaultRange,
                        lane, *session, nextId, result.diagnostics);
            if (nextId > longNoteId)
            {
                session->SetNoteScrollMultiplier(longNoteId, longNoteHead->scrollMultiplier);
                collector->RegisterNote(longNoteId, static_cast<TaikoNoteType>(longNoteHead->note.keyType),
                                        longNoteHead->timing);
            }
            longNoteHead.reset();
            continue;
        }
        if (action != TaikoPatternAction::Down)
        {
            result.diagnostics.push_back({DiagnosticSeverity::Warning, source.source,
                "Ignored unsupported Taiko note action."});
            continue;
        }
        // Match the original RPG Lane transaction: while a long-note
        // head is waiting for its tail, ordinary Down events in that same
        // lane do not create a second focus target inside the roll.
        if (longNoteHead.has_value())
        {
            result.diagnostics.push_back({DiagnosticSeverity::Warning, source.source,
                "Ignored ordinary note inside an active long note."});
            continue;
        }

        transform.Apply(source);
        type = static_cast<TaikoNoteType>(source.keyType);
        const bool don = IsDonType(type);
        const NoteAction requiredAction = ActionValue(don ? TaikoAction::Don : TaikoAction::Kat);
        const bool big = type == TaikoNoteType::BigDon || type == TaikoNoteType::BigKat;
        std::unique_ptr<INoteRule> rule =
            big ? std::unique_ptr<INoteRule>(std::make_unique<CountedHitInputRule>(
                      requiredAction, 2, JudgementGrade::Good))
                : std::unique_ptr<INoteRule>(std::make_unique<TapInputRule>(requiredAction));
        if (type == TaikoNoteType::Purple)
        {
            rule = std::make_unique<SequenceInputRule>(
                std::vector<NoteAction>{ActionValue(TaikoAction::Don), ActionValue(TaikoAction::Kat)},
                JudgementGrade::Good, true);
        }
        std::string soundId = NoteSound(source, don ? TaikoAction::Don : TaikoAction::Kat, big);
        const NoteId noteId = nextId++;
        std::shared_ptr<const INoteSoundPolicy> soundPolicy =
            big ? MakeBigSoundPolicy(std::move(soundId)) : MakeTapSoundPolicy(std::move(soundId));
        if (type == TaikoNoteType::Purple)
        {
            auto purplePolicy = std::make_shared<MappedNoteSoundPolicy>();
            for (const TaikoAction inputAction : {TaikoAction::Don, TaikoAction::Kat})
            {
                purplePolicy->Bind(SoundBinding{.eventType = NoteEventType::HitAccepted,
                                                        .cue = Cue(NoteSound(source, inputAction)),
                                                        .inputAction = ActionValue(inputAction)});
            }
            soundPolicy = std::move(purplePolicy);
        }
        lane.AddNote(std::make_unique<RuleBasedNote>(noteId, compiledNote.timing, profile, std::move(rule),
                                                             std::move(soundPolicy)));
        session->SetNotePresentation(noteId, {TapVisualId(type), {}, false});
        session->SetNoteScrollMultiplier(noteId, compiledNote.scrollMultiplier);
        collector->RegisterNote(noteId, type, compiledNote.timing);
    }

    if (longNoteHead)
        result.diagnostics.push_back({DiagnosticSeverity::Warning, longNoteHead->note.source,
            "Ignored long-note head without a matching end."});

    std::map<PhysicalKey, NoteAction> inputMapping;
    for (const auto &binding : inputLayout.bindings)
        for (const auto key : binding.keys)
            inputMapping.emplace(key, binding.action);
    session->SetInputLayout(std::move(inputLayout));
    session->SetInputMapping(inputMapping);
    collector->SetInputMapping(std::move(inputMapping));
    session->SetFreeInputCue(ActionValue(TaikoAction::Don),
                             Cue(DonFreeInput, "UserInputFeedback"));
    session->SetFreeInputCue(ActionValue(TaikoAction::Kat),
                             Cue(KatFreeInput, "UserInputFeedback"));
    session->SetMeasureLines(BuildMeasureLines(timeline, effectLookup));
    session->SetEffects(std::move(compiledEffects));
    if (options.play.metronome && !timeline.MeasureSpans().empty())
        session->metronomeBeats = CompileQuarterBeats(timeline,
            static_cast<std::int64_t>(timeline.MeasureSpans().size()) - 1, timeline.MeasureSpans().back().end);
    session->SetTimeline(std::move(timeline));
    session->Gear().Finalize();
    session->SetResultCollector(std::move(collector));
    result.session = std::move(session);
    return result;
}
