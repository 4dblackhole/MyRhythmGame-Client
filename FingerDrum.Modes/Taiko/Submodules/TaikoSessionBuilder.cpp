#include "TaikoSessionBuilder.h"
#include "TaikoBuildSupport.h"

namespace finger_drum::mode
{
    using namespace taiko_build;

    ModeLoadResult TaikoSessionBuilder::CreateSession(const chart::PatternDocument &pattern,
                                                      const chart::EffectDocument &effects)
    {
        ModeLoadResult result;
        auto session = std::make_unique<PlaySession>();
        rhythm::Lane &lane = session->Gear().CreateLane();
        auto profile =
            std::make_shared<rhythm::JudgementProfile>("Taiko.Default", pattern.judgementLevel);

        chart::MusicalTimeline timeline(pattern);
        std::vector<chart::CompiledPatternNote> compiled = timeline.CompileNotes(pattern);
        ConfigureHitSounds(*session, pattern, effects, timeline, result.diagnostics);
        if (!result.diagnostics.empty())
            return result;
        for (auto &note : compiled)
        {
            note.scrollMultiplier =
                timeline.EffectValueAt(effects, chart::EffectCommandType::NoteSpeed,
                                       note.note.position) *
                timeline.EffectValueAt(effects, chart::EffectCommandType::ScrollSpeed,
                                       note.note.position);
            // Explicit per-note assignments take precedence over timed defaults.
            if (pattern.hitSounds.contains(note.note.hitSound))
            {
                note.note.hitSound = ChartSoundId(note.note.hitSound);
            }
        }
        // The legacy RPG Lane treated a long note as one active head/tail
        // transaction per lane. Keeping that rule prevents a stray second
        // LNStart from replacing the real head before its matching tail.
        std::optional<chart::CompiledPatternNote> longNoteHead;
        rhythm::NoteId nextId = 1;

        for (const chart::CompiledPatternNote &compiledNote : compiled)
        {
            const chart::PatternNote &source = compiledNote.note;
            const TaikoNoteType type = static_cast<TaikoNoteType>(source.keyType);
            const TaikoPatternAction action = static_cast<TaikoPatternAction>(source.actionType);

            if (action == TaikoPatternAction::LongNoteStart)
            {
                if (!longNoteHead.has_value())
                {
                    longNoteHead = compiledNote;
                }
                continue;
            }
            if (action == TaikoPatternAction::LongNoteEnd)
            {
                if (!longNoteHead.has_value() || longNoteHead->note.keyType != source.keyType)
                {
                    continue;
                }
                const auto longNoteId = nextId;
                AddLongNote(*longNoteHead, compiledNote, timeline, profile, lane, *session, nextId,
                            result.diagnostics);
                if (nextId > longNoteId)
                    session->SetNoteScrollMultiplier(longNoteId, longNoteHead->scrollMultiplier);
                longNoteHead.reset();
                continue;
            }
            if (action != TaikoPatternAction::Down)
            {
                continue;
            }
            // Match the original RPG Lane transaction: while a long-note
            // head is waiting for its tail, ordinary Down events in that same
            // lane do not create a second focus target inside the roll.
            if (longNoteHead.has_value())
            {
                continue;
            }

            const bool don = IsDonType(type);
            const rhythm::NoteAction requiredAction =
                ActionValue(don ? TaikoAction::Don : TaikoAction::Kat);
            const bool big = type == TaikoNoteType::BigDon || type == TaikoNoteType::BigKat;
            std::unique_ptr<rhythm::INoteRule> rule =
                big ? std::unique_ptr<rhythm::INoteRule>(
                          std::make_unique<rhythm::CountedHitInputRule>(
                              requiredAction, 2, rhythm::JudgementGrade::Good))
                    : std::unique_ptr<rhythm::INoteRule>(
                          std::make_unique<rhythm::TapInputRule>(requiredAction));
            if (type == TaikoNoteType::Purple)
            {
                rule = std::make_unique<rhythm::SequenceInputRule>(
                    std::vector<rhythm::NoteAction>{ActionValue(TaikoAction::Don),
                                                    ActionValue(TaikoAction::Kat)},
                    rhythm::JudgementGrade::Good, true);
            }
            std::string soundId = source.hitSound;
            if (soundId.empty())
            {
                soundId = big ? (don ? finger_drum::mode::taiko_sound::BigDonFirstHit
                                     : finger_drum::mode::taiko_sound::BigKatFirstHit)
                              : (don ? finger_drum::mode::taiko_sound::DonHit
                                     : finger_drum::mode::taiko_sound::KatHit);
            }
            const rhythm::NoteId noteId = nextId++;
            std::shared_ptr<const rhythm::INoteSoundPolicy> soundPolicy =
                big ? MakeBigSoundPolicy(std::move(soundId))
                    : MakeTapSoundPolicy(std::move(soundId));
            if (type == TaikoNoteType::Purple)
            {
                auto purplePolicy = std::make_shared<rhythm::MappedNoteSoundPolicy>();
                for (const TaikoAction inputAction : {TaikoAction::Don, TaikoAction::Kat})
                {
                    purplePolicy->Bind(rhythm::SoundBinding{
                        .eventType = rhythm::NoteEventType::HitAccepted,
                        .cue = Cue(source.hitSound.empty()
                                       ? (inputAction == TaikoAction::Don
                                              ? finger_drum::mode::taiko_sound::DonHit
                                              : finger_drum::mode::taiko_sound::KatHit)
                                       : source.hitSound),
                        .inputAction = ActionValue(inputAction)});
                }
                soundPolicy = std::move(purplePolicy);
            }
            lane.AddNote(std::make_unique<rhythm::RuleBasedNote>(
                noteId, compiledNote.timing, profile, std::move(rule), std::move(soundPolicy)));
            session->SetNotePresentation(noteId, {TapVisualId(type), {}, false});
            session->SetNoteScrollMultiplier(noteId, compiledNote.scrollMultiplier);
        }

        std::map<rhythm::PhysicalKey, rhythm::NoteAction> inputMapping;
        for (const TaikoInputBinding &binding : TaikoInputBindings)
        {
            const rhythm::NoteAction action = ActionValue(binding.action);
            inputMapping.emplace(binding.primaryKey, action);
            for (const rhythm::PhysicalKey secondaryKey : binding.secondaryKeys)
            {
                inputMapping.emplace(secondaryKey, action);
            }
        }
        session->SetInputMapping(std::move(inputMapping));
        session->SetFreeInputCue(
            ActionValue(TaikoAction::Don),
            Cue(finger_drum::mode::taiko_sound::DonFreeInput, "UserInputFeedback"));
        session->SetFreeInputCue(
            ActionValue(TaikoAction::Kat),
            Cue(finger_drum::mode::taiko_sound::KatFreeInput, "UserInputFeedback"));
        session->SetEffects(timeline.CompileEffects(effects));
        session->SetMeasureLines(timeline.CompileMeasureStarts());
        session->Gear().Finalize();
        result.session = std::move(session);
        return result;
    }
} // namespace finger_drum::mode
