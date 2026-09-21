#include "TaikoSessionBuilder.h"
#include "TaikoBuildSupport.h"

namespace finger_drum::mode
{
    using namespace taiko_build;

    void TaikoSessionBuilder::AddLongNote(
        const chart::CompiledPatternNote &compiledHead,
        const chart::CompiledPatternNote &compiledTail, const chart::MusicalTimeline &timeline,
        const std::shared_ptr<const rhythm::JudgementProfile> &profile, rhythm::Lane &lane,
        PlaySession &session, rhythm::NoteId &nextId, std::vector<chart::Diagnostic> &diagnostics)
    {
        const chart::PatternNote &head = compiledHead.note;
        const chart::PatternNote &tail = compiledTail.note;
        const rhythm::RhythmTime begin = compiledHead.timing;
        const rhythm::RhythmTime end = compiledTail.timing;
        if (end <= begin)
        {
            diagnostics.push_back({chart::DiagnosticSeverity::Error, tail.source,
                                   "A long-note end must be later than its start."});
            return;
        }

        // Translate only the Taiko-specific options here. The resulting rule
        // and sound policy remain backend-neutral rhythm objects.
        const TaikoNoteType type = static_cast<TaikoNoteType>(head.keyType);
        std::unique_ptr<rhythm::INoteRule> rule;
        std::shared_ptr<const rhythm::INoteSoundPolicy> soundPolicy;
        std::optional<TaikoAction> buzzAction;
        std::vector<rhythm::RhythmTime> tickTimes;
        switch (type)
        {
        case TaikoNoteType::Roll:
        case TaikoNoteType::BigRoll: {
            const auto hitCount = ReadHitCount(
                head, timeline.CountSubdivisions(head.position, tail.position, 12), diagnostics);
            if (!hitCount.has_value())
            {
                break;
            }
            rule = std::make_unique<rhythm::DrumRollInputRule>(
                std::vector<rhythm::NoteAction>{ActionValue(TaikoAction::Don),
                                                ActionValue(TaikoAction::Kat)},
                end, *hitCount);
            soundPolicy = MakeTickSoundPolicy(head.hitSound);
            break;
        }
        case TaikoNoteType::TickRoll:
        case TaikoNoteType::BigTickRoll:
            if (const auto division = ReadPositiveOption(
                    head, finger_drum::mode::taiko_option::TickDivision, 16, diagnostics))
            {
                tickTimes = timeline.CompileSubdivisions(head.position, tail.position, *division);
                rule = std::make_unique<rhythm::TickRollInputRule>(
                    std::vector<rhythm::NoteAction>{ActionValue(TaikoAction::Don),
                                                    ActionValue(TaikoAction::Kat)},
                    end, tickTimes);
                soundPolicy = MakeTickSoundPolicy(head.hitSound);
            }
            break;
        case TaikoNoteType::Balloon:
            if (const auto hitCount =
                    ReadHitCount(head, timeline.CountSubdivisions(head.position, tail.position, 12),
                                 diagnostics))
            {
                rule = std::make_unique<rhythm::TimedSequenceInputRule>(
                    std::vector<rhythm::NoteAction>{ActionValue(TaikoAction::Don)}, *hitCount, end);
                soundPolicy = MakeBalloonSoundPolicy(head.hitSound);
            }
            break;
        case TaikoNoteType::DengDeng:
            if (const auto hitCount =
                    ReadHitCount(head, timeline.CountSubdivisions(head.position, tail.position, 12),
                                 diagnostics))
            {
                rule = std::make_unique<rhythm::TimedSequenceInputRule>(
                    std::vector<rhythm::NoteAction>{ActionValue(TaikoAction::Don),
                                                    ActionValue(TaikoAction::Kat)},
                    *hitCount, end);
                soundPolicy = MakeAlternatingSoundPolicy(*hitCount, head.hitSound);
            }
            break;
        case TaikoNoteType::Buzz:
            buzzAction = ReadBuzzAction(head, diagnostics);
            if (const auto division = ReadPositiveOption(
                    head, finger_drum::mode::taiko_option::TickDivision, 16, diagnostics);
                buzzAction.has_value() && division.has_value())
            {
                tickTimes = timeline.CompileSubdivisions(head.position, tail.position, *division);
                // The head has its own timing score, not a body tick.
                if (!tickTimes.empty())
                {
                    tickTimes.erase(tickTimes.begin());
                }
                rule = std::make_unique<rhythm::HoldInputRule>(ActionValue(*buzzAction), end,
                                                               tickTimes);
                const std::string buzzSound =
                    head.hitSound.empty()
                        ? (*buzzAction == TaikoAction::Don ? finger_drum::mode::taiko_sound::DonHit
                                                           : finger_drum::mode::taiko_sound::KatHit)
                        : head.hitSound;
                auto buzzPolicy = std::make_shared<rhythm::MappedNoteSoundPolicy>();
                buzzPolicy->Bind(rhythm::SoundBinding{
                    .eventType = rhythm::NoteEventType::HitAccepted, .cue = Cue(buzzSound)});
                buzzPolicy->Bind(
                    rhythm::SoundBinding{.eventType = rhythm::NoteEventType::TickAccepted,
                                         .cue = Cue(buzzSound, "TickSound")});
                soundPolicy = std::move(buzzPolicy);
            }
            break;
        default:
            diagnostics.push_back({chart::DiagnosticSeverity::Error, head.source,
                                   "The long-note key type is not supported by Taiko mode."});
            return;
        }
        if (rule == nullptr || soundPolicy == nullptr)
        {
            return;
        }

        // One rule-based note owns the complete interval; presentation keeps
        // the tail time separate from its input and scoring rule.
        const rhythm::NoteId noteId = nextId++;
        lane.AddNote(std::make_unique<rhythm::RuleBasedNote>(
            noteId, begin, profile, std::move(rule), std::move(soundPolicy)));
        session.SetNotePresentation(
            noteId, {LongNoteVisualId(type, buzzAction), end, true, std::move(tickTimes)});
    }
} // namespace finger_drum::mode
