#include "Taiko/TaikoMode.h"

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <utility>

namespace finger_drum::mode
{
    namespace
    {
        [[nodiscard]] rhythm::NoteAction ActionValue(
            const TaikoAction action) noexcept
        {
            return static_cast<rhythm::NoteAction>(action);
        }

        [[nodiscard]] rhythm::AudioCueRequest Cue(
            std::string sound,
            std::string bus = "HitSound")
        {
            rhythm::AudioCueRequest result;
            result.sound = std::move(sound);
            result.bus = std::move(bus);
            return result;
        }

        [[nodiscard]] bool IsDonType(const TaikoNoteType type) noexcept
        {
            return type == TaikoNoteType::Don ||
                type == TaikoNoteType::BigDon;
        }

        [[nodiscard]] std::vector<rhythm::RhythmTime> MakeTicks(
            const rhythm::RhythmTime begin,
            const rhythm::RhythmTime end)
        {
            std::vector<rhythm::RhythmTime> ticks;
            constexpr rhythm::RhythmDuration TickStep{125'000};
            for (rhythm::RhythmTime time = begin + TickStep;
                 time < end;
                 time += TickStep)
            {
                ticks.push_back(time);
            }
            return ticks;
        }

        [[nodiscard]] std::string_view LongNoteVisualId(
            const TaikoNoteType type) noexcept
        {
            if (type == TaikoNoteType::BigRoll ||
                type == TaikoNoteType::BigTickRoll)
            {
                return "Taiko.BigRoll";
            }
            if (type == TaikoNoteType::Balloon)
            {
                return "Taiko.Balloon";
            }
            return "Taiko.Roll";
        }

        [[nodiscard]] std::string_view TapVisualId(
            const TaikoNoteType type) noexcept
        {
            switch (type)
            {
            case TaikoNoteType::Don: return "Taiko.Don";
            case TaikoNoteType::Kat: return "Taiko.Kat";
            case TaikoNoteType::BigDon: return "Taiko.BigDon";
            case TaikoNoteType::BigKat: return "Taiko.BigKat";
            default: return "Taiko.Unknown";
            }
        }
    }

    std::string_view TaikoMode::Id() const noexcept
    {
        return "Taiko";
    }

    ModeLoadResult TaikoMode::LoadSession(
        const std::filesystem::path& patternPath,
        const std::optional<std::filesystem::path>& effectPath) const
    {
        chart::ChartParser parser;
        chart::ParseResult<chart::PatternDocument> pattern =
            parser.ParsePatternFile(patternPath);
        chart::ParseResult<chart::EffectDocument> effect;
        if (effectPath.has_value())
        {
            effect = parser.ParseEffectFile(*effectPath);
        }

        ModeLoadResult result;
        result.diagnostics = std::move(pattern.diagnostics);
        result.diagnostics.insert(
            result.diagnostics.end(),
            std::make_move_iterator(effect.diagnostics.begin()),
            std::make_move_iterator(effect.diagnostics.end()));
        if (std::ranges::any_of(
                result.diagnostics,
                [](const chart::Diagnostic& diagnostic)
                {
                    return diagnostic.severity ==
                        chart::DiagnosticSeverity::Error;
                }))
        {
            return result;
        }

        ModeLoadResult created = CreateSession(
            pattern.document,
            effect.document);
        result.session = std::move(created.session);
        result.diagnostics.insert(
            result.diagnostics.end(),
            std::make_move_iterator(created.diagnostics.begin()),
            std::make_move_iterator(created.diagnostics.end()));
        return result;
    }

    ModeLoadResult TaikoMode::CreateSession(
        const chart::PatternDocument& pattern,
        const chart::EffectDocument& effects) const
    {
        ModeLoadResult result;
        auto session = std::make_unique<PlaySession>();
        rhythm::Lane& lane = session->Gear().CreateLane();
        auto profile = std::make_shared<rhythm::JudgementProfile>(
            "Taiko.Default",
            pattern.judgementLevel);

        chart::MusicalTimeline timeline(pattern);
        const std::vector<chart::CompiledPatternNote> compiled =
            timeline.CompileNotes(pattern);
        // The legacy RPG Lane treated a long note as one active head/tail
        // transaction per lane. Keeping that rule prevents a stray second
        // LNStart from replacing the real head before its matching tail.
        std::optional<chart::CompiledPatternNote> longNoteHead;
        rhythm::NoteId nextId = 1;

        for (const chart::CompiledPatternNote& compiledNote : compiled)
        {
            const chart::PatternNote& source = compiledNote.note;
            const TaikoNoteType type =
                static_cast<TaikoNoteType>(source.keyType);
            const TaikoPatternAction action =
                static_cast<TaikoPatternAction>(source.actionType);

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
                if (!longNoteHead.has_value() ||
                    longNoteHead->note.keyType != source.keyType)
                {
                    continue;
                }
                const rhythm::RhythmTime begin = longNoteHead->timing;
                const rhythm::RhythmTime end = compiledNote.timing;
                std::unique_ptr<rhythm::INoteRule> rule;
                if (type == TaikoNoteType::TickRoll ||
                    type == TaikoNoteType::BigTickRoll)
                {
                    rule = std::make_unique<rhythm::HoldInputRule>(
                        ActionValue(TaikoAction::Don),
                        end,
                        MakeTicks(begin, end));
                }
                else
                {
                    rule = std::make_unique<rhythm::DrumRollInputRule>(
                        std::vector<rhythm::NoteAction>{
                            ActionValue(TaikoAction::Don),
                            ActionValue(TaikoAction::Kat)},
                        end);
                }
                const rhythm::NoteId noteId = nextId++;
                lane.AddNote(std::make_unique<rhythm::RuleBasedNote>(
                    noteId,
                    begin,
                    profile,
                    std::move(rule),
                    MakeTickSoundPolicy("Taiko.LongNote.Tick")));
                session->SetNotePresentation(noteId, {
                    std::string(LongNoteVisualId(type)),
                    end,
                    true});
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
            const rhythm::NoteAction requiredAction = ActionValue(
                don ? TaikoAction::Don : TaikoAction::Kat);
            const bool big = type == TaikoNoteType::BigDon ||
                type == TaikoNoteType::BigKat;
            std::unique_ptr<rhythm::INoteRule> rule = big
                ? std::unique_ptr<rhythm::INoteRule>(
                    std::make_unique<rhythm::CountedHitInputRule>(
                        requiredAction,
                        2,
                        rhythm::JudgementGrade::Good))
                : std::unique_ptr<rhythm::INoteRule>(
                    std::make_unique<rhythm::TapInputRule>(requiredAction));
            std::string soundId = source.hitSound;
            if (soundId.empty())
            {
                soundId = big
                    ? (don ? "Taiko.BigDon.FirstHit" : "Taiko.BigKat.FirstHit")
                    : (don ? "Taiko.Don.Hit" : "Taiko.Kat.Hit");
            }
            const rhythm::NoteId noteId = nextId++;
            lane.AddNote(std::make_unique<rhythm::RuleBasedNote>(
                noteId,
                compiledNote.timing,
                profile,
                std::move(rule),
                big
                    ? MakeBigSoundPolicy(std::move(soundId))
                    : MakeTapSoundPolicy(std::move(soundId))));
            session->SetNotePresentation(noteId, {
                std::string(TapVisualId(type)),
                {},
                false});
        }

        session->SetInputMapping({
            {static_cast<rhythm::PhysicalKey>('F'), ActionValue(TaikoAction::Don)},
            {static_cast<rhythm::PhysicalKey>('J'), ActionValue(TaikoAction::Don)},
            {static_cast<rhythm::PhysicalKey>('D'), ActionValue(TaikoAction::Kat)},
            {static_cast<rhythm::PhysicalKey>('K'), ActionValue(TaikoAction::Kat)},
        });
        session->SetFreeInputCue(
            ActionValue(TaikoAction::Don),
            Cue("Taiko.Don.FreeInput", "UserInputFeedback"));
        session->SetFreeInputCue(
            ActionValue(TaikoAction::Kat),
            Cue("Taiko.Kat.FreeInput", "UserInputFeedback"));
        session->SetEffects(timeline.CompileEffects(effects));
        session->Gear().Finalize();
        result.session = std::move(session);
        return result;
    }

    std::shared_ptr<const rhythm::INoteSoundPolicy>
    TaikoMode::MakeTapSoundPolicy(std::string soundId)
    {
        auto policy = std::make_shared<rhythm::MappedNoteSoundPolicy>();
        policy->Bind(rhythm::SoundBinding{
            .eventType = rhythm::NoteEventType::HitAccepted,
            .cue = Cue(std::move(soundId))});
        return policy;
    }

    std::shared_ptr<const rhythm::INoteSoundPolicy>
    TaikoMode::MakeBigSoundPolicy(std::string soundId)
    {
        auto policy = std::make_shared<rhythm::MappedNoteSoundPolicy>();
        policy->Bind(rhythm::SoundBinding{
            .eventType = rhythm::NoteEventType::HitAccepted,
            .maximumGrade = rhythm::JudgementGrade::Good,
            .hitIndex = 0,
            .cue = Cue(std::move(soundId)),
            .stopAfterMatch = true});
        return policy;
    }

    std::shared_ptr<const rhythm::INoteSoundPolicy>
    TaikoMode::MakeTickSoundPolicy(std::string soundId)
    {
        auto policy = std::make_shared<rhythm::MappedNoteSoundPolicy>();
        policy->Bind(rhythm::SoundBinding{
            .eventType = rhythm::NoteEventType::TickAccepted,
            .cue = Cue(std::move(soundId), "TickSound")});
        return policy;
    }
}
